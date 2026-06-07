#include "platform/psvita/rendering/PsVitaRenderManager3D.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "CameraProjectionUtils.hpp"
#include "CameraViewportResolver.hpp"
#include "Core.hpp"
#include "EditorAssetBinarySerializer.hpp"
#include "EngineBinaryHeader.hpp"
#include "EngineBinaryHeaderSerializer.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "MaterialBlendMode.hpp"
#include "MaterialCullMode.hpp"
#include "MaterialRenderState.hpp"
#include "MeshComponent.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshAsset.hpp"
#include "ObjectManager.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RuntimeMaterial.hpp"
#include "ShaderMaterialAsset.hpp"
#include "ShaderMaterialAssetBinarySerializer.hpp"
#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeModel.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeSubmesh.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_string.hpp"
#include "system/io/file.hpp"

namespace {
    constexpr int PsVitaScreenWidth = 960;
    constexpr int PsVitaScreenHeight = 544;
    constexpr float PsVitaPerspectiveFieldOfViewRadians = 0.78539816339744831f;
    constexpr float PsVitaMinimumProjectedW = 0.0001f;

    /// Stores one projected triangle and its average depth so the temporary white mesh pass can draw farther triangles first without a material/depth system.
    struct ProjectedTriangle final {
        ::float3 Vertex0;
        ::float3 Vertex1;
        ::float3 Vertex2;
        float AverageDepth;
    };
}

namespace helengine::psvita {
    /// Creates the PS Vita 3D renderer with the Vita display size.
    PsVitaRenderManager3D::PsVitaRenderManager3D() {
        set_MainWindowSize(::int2(PsVitaScreenWidth, PsVitaScreenHeight));
        ActiveViewport = ::float4(0.0f, 0.0f, static_cast<float>(PsVitaScreenWidth), static_cast<float>(PsVitaScreenHeight));
        ActiveViewProjection = ::float4x4::get_Identity();
    }

    /// Assigns the native PS Vita GXM renderer that will receive white mesh triangle batches.
    void PsVitaRenderManager3D::SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer) {
        GxmRenderer = gxmRenderer;
    }

    /// Traverses camera-owned 3D queues, submits white mesh geometry, and forwards ordered 2D queues to the Vita 2D renderer.
    void PsVitaRenderManager3D::Draw() {
        if (Core::Instance == nullptr || Core::Instance->ObjectManager == nullptr || Core::Instance->RenderManager2D == nullptr) {
            return;
        }

        PsVitaRenderManager2D* renderManager2D = dynamic_cast<PsVitaRenderManager2D*>(Core::Instance->RenderManager2D);
        if (renderManager2D == nullptr) {
            return;
        }

        List<::ICamera*>* cameras = Core::Instance->ObjectManager->get_Cameras();
        if (cameras == nullptr) {
            return;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ::ICamera* camera = (*cameras).get_Item(static_cast<int32_t>(cameraIndex));
            if (camera == nullptr) {
                continue;
            }

            DrawCamera(camera);
            renderManager2D->DrawCamera(camera);
        }
    }

    /// Builds one concrete runtime material from one packaged cooked platform material asset.
    ::RuntimeMaterial* PsVitaRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Cooked material asset path must be provided.", "cookedAssetPath");
        }

        ::FileStream* stream = nullptr;
        ::EngineBinaryHeader* header = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            header = ::EngineBinaryHeaderSerializer::Read(stream);
            if (header->FormatId == ShaderMaterialAssetBinarySerializer::FormatId) {
                ::ShaderMaterialAsset* cookedShaderMaterialAsset = ::ShaderMaterialAssetBinarySerializer::Deserialize(stream, header);
                header = nullptr;
                delete stream;
                stream = nullptr;

                ::RuntimeMaterial* runtimeMaterial = BuildMaterialFromCooked(cookedShaderMaterialAsset);
                delete cookedShaderMaterialAsset;
                return runtimeMaterial;
            }

            asset = ::EditorAssetBinarySerializer::Deserialize(stream, header);
            header = nullptr;
            delete stream;
            stream = nullptr;

            ::PlatformMaterialAsset* cookedMaterialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);
            if (cookedMaterialAsset == nullptr) {
                throw new InvalidOperationException("PS Vita cooked material payloads must deserialize as PlatformMaterialAsset.");
            }

            ::RuntimeMaterial* runtimeMaterial = BuildMaterialFromCooked(cookedMaterialAsset);
            delete cookedMaterialAsset;
            asset = nullptr;
            return runtimeMaterial;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }
            if (header != nullptr) {
                delete header;
            }
            if (asset != nullptr) {
                delete asset;
            }

            throw;
        }
    }

    /// Builds one concrete runtime material from one deserialized platform material asset.
    ::RuntimeMaterial* PsVitaRenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }

        auto* runtimeMaterial = new ::RuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset->get_Id());
        runtimeMaterial->set_LightingModel(materialAsset->Lit
            ? RuntimeMaterialLightingModel::MetalRoughPbr
            : RuntimeMaterialLightingModel::Unlit);
        runtimeMaterial->get_RenderState()->set_CullMode(materialAsset->DoubleSided
            ? MaterialCullMode::None
            : MaterialCullMode::Back);
        runtimeMaterial->get_RenderState()->set_BlendMode(materialAsset->BaseColorA < 255
            ? MaterialBlendMode::AlphaBlend
            : MaterialBlendMode::Opaque);
        runtimeMaterial->get_RenderState()->set_DepthWriteEnabled(materialAsset->BaseColorA >= 255);
        return runtimeMaterial;
    }

    /// Builds one concrete runtime material from one deserialized shader material asset while ignoring shader-specific resources for the white-mesh pass.
    ::RuntimeMaterial* PsVitaRenderManager3D::BuildMaterialFromCooked(::ShaderMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }
        if (materialAsset->RenderState == nullptr) {
            throw new InvalidOperationException("PS Vita shader-backed material payloads must include a render state.");
        }

        auto* runtimeMaterial = new ::RuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset->get_Id());
        runtimeMaterial->SetRenderState(materialAsset->RenderState);
        runtimeMaterial->set_CastsShadows(materialAsset->CastsShadows);
        runtimeMaterial->set_ReceivesShadows(materialAsset->ReceivesShadows);
        runtimeMaterial->set_LightingModel(RuntimeMaterialLightingModel::MetalRoughPbr);
        return runtimeMaterial;
    }

    /// Builds one concrete PS Vita runtime model from one packaged cooked model asset.
    ::RuntimeModel* PsVitaRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Cooked model asset path must be provided.", "cookedAssetPath");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            asset = ::AssetSerializer::Deserialize(stream);
            delete stream;
            stream = nullptr;

            ::ModelAsset* cookedModelAsset = he_cpp_try_cast<ModelAsset>(asset);
            if (cookedModelAsset == nullptr) {
                throw new InvalidOperationException("PS Vita cooked model payloads must deserialize as ModelAsset.");
            }

            ::RuntimeModel* runtimeModel = BuildModelFromRaw(cookedModelAsset);
            delete cookedModelAsset;
            asset = nullptr;
            return runtimeModel;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }
            if (asset != nullptr) {
                delete asset;
            }

            throw;
        }
    }

    /// Builds one concrete PS Vita runtime model from raw data.
    ::RuntimeModel* PsVitaRenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }
        if (data->Positions == nullptr || data->Positions->get_Length() == 0) {
            throw new ArgumentException("Model data must include positions.");
        }

        std::vector<::float3> copiedPositions;
        copiedPositions.reserve(static_cast<std::size_t>(data->Positions->get_Length()));
        for (int32_t positionIndex = 0; positionIndex < data->Positions->get_Length(); ++positionIndex) {
            copiedPositions.push_back((*data->Positions)[positionIndex]);
        }

        ::ModelAssetIndexData* indexData = ::ModelAssetIndexData::Resolve(data);
        std::vector<std::uint32_t> resolvedIndices;
        resolvedIndices.reserve(static_cast<std::size_t>(std::max(0, indexData->IndexCount)));
        if (indexData->Uses32BitIndices && indexData->Indices32 != nullptr) {
            for (int32_t index = 0; index < indexData->Indices32->get_Length(); ++index) {
                resolvedIndices.push_back((*indexData->Indices32)[index]);
            }
        } else if (indexData->Indices16 != nullptr) {
            for (int32_t index = 0; index < indexData->Indices16->get_Length(); ++index) {
                resolvedIndices.push_back(static_cast<std::uint32_t>((*indexData->Indices16)[index]));
            }
        }
        delete indexData;

        auto* runtimeModel = new rendering::PsVitaRuntimeModel(std::move(copiedPositions));
        runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);
        runtimeModel->SetSubmeshes(BuildRuntimeSubmeshes(data, resolvedIndices));
        return runtimeModel;
    }

    /// Visits one ordered 3D drawable and renders supported mesh content through the white triangle path.
    void PsVitaRenderManager3D::Visit(::IDrawable3D* drawable) {
        if (drawable == nullptr || ActiveCamera == nullptr) {
            return;
        }

        ::MeshComponent* meshComponent = he_cpp_try_cast<MeshComponent>(drawable);
        if (meshComponent == nullptr || meshComponent->get_Model() == nullptr) {
            return;
        }

        auto* runtimeModel = dynamic_cast<rendering::PsVitaRuntimeModel*>(meshComponent->get_Model());
        if (runtimeModel == nullptr) {
            return;
        }

        DrawRuntimeModel(meshComponent, runtimeModel);
    }

    /// Draws one camera's ordered 3D queue and forwards its 2D queue to the overlay renderer.
    void PsVitaRenderManager3D::DrawCamera(::ICamera* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        ::IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
        if (renderQueue == nullptr) {
            return;
        }

        ActiveCamera = camera;
        ActiveViewport = CameraViewportResolver::ResolveViewport(
            camera->get_Viewport(),
            static_cast<double>(get_MainWindowSize().X),
            static_cast<double>(get_MainWindowSize().Y));
        if (ActiveViewport.Z <= 0.0f || ActiveViewport.W <= 0.0f) {
            ActiveCamera = nullptr;
            return;
        }
        ActiveViewProjection = BuildCameraViewProjection(camera, ActiveViewport);
        renderQueue->VisitOrdered(this);

        if (GxmRenderer != nullptr && !QueuedMeshTriangles.empty()) {
            GxmRenderer->SubmitSolidWhiteMeshTriangles(QueuedMeshTriangles);
            QueuedMeshTriangles.clear();
        }

        ActiveCamera = nullptr;
    }

    /// Draws one mesh component with one concrete PS Vita runtime model.
    void PsVitaRenderManager3D::DrawRuntimeModel(::MeshComponent* meshComponent, rendering::PsVitaRuntimeModel* runtimeModel) {
        if (meshComponent == nullptr || runtimeModel == nullptr || meshComponent->get_Parent() == nullptr) {
            return;
        }

        const std::vector<::float3>& positions = runtimeModel->GetPositions();
        if (positions.empty()) {
            return;
        }

        ::float4x4 world = BuildWorldTransform(meshComponent->get_Parent());
        ::float4x4 worldViewProjection;
        float4x4::Multiply__ref0_ref1_out2(world, ActiveViewProjection, worldViewProjection);

        Array<::RuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
        if (submeshes == nullptr || submeshes->get_Length() == 0) {
            return;
        }

        std::vector<ProjectedTriangle> projectedTriangles;
        for (int32_t submeshIndex = 0; submeshIndex < submeshes->get_Length(); ++submeshIndex) {
            auto* submesh = dynamic_cast<rendering::PsVitaRuntimeSubmesh*>((*submeshes)[submeshIndex]);
            if (submesh == nullptr) {
                continue;
            }

            const std::vector<std::uint32_t>& triangleIndices = submesh->GetTriangleIndices();
            for (std::size_t index = 0; index + 2 < triangleIndices.size(); index += 3) {
                const std::uint32_t triangleIndex0 = triangleIndices[index];
                const std::uint32_t triangleIndex1 = triangleIndices[index + 1];
                const std::uint32_t triangleIndex2 = triangleIndices[index + 2];
                if (triangleIndex0 >= positions.size() || triangleIndex1 >= positions.size() || triangleIndex2 >= positions.size()) {
                    continue;
                }

                ::float3 projectedVertex0;
                ::float3 projectedVertex1;
                ::float3 projectedVertex2;
                if (!TryProjectToScreen(positions[triangleIndex0], worldViewProjection, ActiveViewport, projectedVertex0)
                    || !TryProjectToScreen(positions[triangleIndex1], worldViewProjection, ActiveViewport, projectedVertex1)
                    || !TryProjectToScreen(positions[triangleIndex2], worldViewProjection, ActiveViewport, projectedVertex2)) {
                    continue;
                }

                projectedTriangles.push_back(ProjectedTriangle {
                    projectedVertex0,
                    projectedVertex1,
                    projectedVertex2,
                    (projectedVertex0.Z + projectedVertex1.Z + projectedVertex2.Z) / 3.0f
                });
            }
        }

        std::stable_sort(projectedTriangles.begin(), projectedTriangles.end(), [](const ProjectedTriangle& left, const ProjectedTriangle& right) {
            return left.AverageDepth > right.AverageDepth;
        });

        for (const ProjectedTriangle& triangle : projectedTriangles) {
            QueuedMeshTriangles.push_back(triangle.Vertex0);
            QueuedMeshTriangles.push_back(triangle.Vertex1);
            QueuedMeshTriangles.push_back(triangle.Vertex2);
        }
    }

    /// Copies one runtime submesh array from the raw model asset into PS Vita-owned submesh objects.
    Array<::RuntimeSubmesh*>* PsVitaRenderManager3D::BuildRuntimeSubmeshes(::ModelAsset* data, const std::vector<std::uint32_t>& resolvedIndices) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        const int32_t positionCount = data->Positions == nullptr ? 0 : data->Positions->get_Length();
        const int32_t elementCount = resolvedIndices.empty() ? positionCount : static_cast<int32_t>(resolvedIndices.size());
        if (data->Submeshes == nullptr || data->Submeshes->get_Length() == 0) {
            if (elementCount == 0) {
                return Array<::RuntimeSubmesh*>::Empty();
            }

            std::vector<std::uint32_t> defaultTriangleIndices;
            defaultTriangleIndices.reserve(static_cast<std::size_t>(elementCount));
            if (resolvedIndices.empty()) {
                for (int32_t index = 0; index < elementCount; ++index) {
                    defaultTriangleIndices.push_back(static_cast<std::uint32_t>(index));
                }
            } else {
                defaultTriangleIndices = resolvedIndices;
            }

            auto* submeshes = new Array<::RuntimeSubmesh*>(1);
            (*submeshes)[0] = new rendering::PsVitaRuntimeSubmesh(String::Empty, 0, elementCount, std::move(defaultTriangleIndices));
            return submeshes;
        }

        auto* runtimeSubmeshes = new Array<::RuntimeSubmesh*>(data->Submeshes->get_Length());
        for (int32_t submeshIndex = 0; submeshIndex < data->Submeshes->get_Length(); ++submeshIndex) {
            ::ModelSubmeshAsset* authoredSubmesh = (*data->Submeshes)[submeshIndex];
            if (authoredSubmesh == nullptr) {
                throw new InvalidOperationException("Model submesh collections cannot contain null entries.");
            }
            if (authoredSubmesh->IndexStart < 0) {
                throw new InvalidOperationException("Model submesh index starts must be zero or greater.");
            }
            if (authoredSubmesh->IndexCount <= 0) {
                throw new InvalidOperationException("Model submesh index counts must be greater than zero.");
            }
            if (authoredSubmesh->IndexStart + authoredSubmesh->IndexCount > elementCount) {
                throw new InvalidOperationException("Model submesh ranges cannot exceed the resolved model element count.");
            }

            std::vector<std::uint32_t> triangleIndices;
            triangleIndices.reserve(static_cast<std::size_t>(authoredSubmesh->IndexCount));
            if (resolvedIndices.empty()) {
                for (int32_t index = 0; index < authoredSubmesh->IndexCount; ++index) {
                    triangleIndices.push_back(static_cast<std::uint32_t>(authoredSubmesh->IndexStart + index));
                }
            } else {
                for (int32_t index = 0; index < authoredSubmesh->IndexCount; ++index) {
                    triangleIndices.push_back(resolvedIndices[static_cast<std::size_t>(authoredSubmesh->IndexStart + index)]);
                }
            }

            (*runtimeSubmeshes)[submeshIndex] = new rendering::PsVitaRuntimeSubmesh(
                authoredSubmesh->get_MaterialSlotName(),
                authoredSubmesh->IndexStart,
                authoredSubmesh->IndexCount,
                std::move(triangleIndices));
        }

        return runtimeSubmeshes;
    }

    /// Builds the shared camera view-projection matrix used by the current mesh pass.
    ::float4x4 PsVitaRenderManager3D::BuildCameraViewProjection(::ICamera* camera, const ::float4& viewport) const {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }
        if (camera->get_Parent() == nullptr) {
            throw new InvalidOperationException("PS Vita 3D drawing requires cameras with parent entities.");
        }

        ::float3 cameraPosition = camera->get_Parent()->get_Position();
        ::float4 cameraOrientation = camera->get_Parent()->get_Orientation();
        ::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, 1.0f), cameraOrientation);
        ::float3 cameraUp = float4::RotateVector(::float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        ::float3 cameraTarget = cameraPosition + cameraForward;

        ::float4x4 view;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(cameraPosition, cameraTarget, cameraUp, view);

        ::float4x4 projection = CameraProjectionUtils::CreatePerspectiveProjection(
            camera,
            PsVitaPerspectiveFieldOfViewRadians,
            viewport.Z / viewport.W);

        ::float4x4 viewProjection;
        float4x4::Multiply__ref0_ref1_out2(view, projection, viewProjection);
        return viewProjection;
    }

    /// Builds the current drawable world transform from entity position, orientation, and scale.
    ::float4x4 PsVitaRenderManager3D::BuildWorldTransform(::Entity* entity) {
        if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        ::float4 orientation = entity->get_Orientation();
        ::float4x4 rotation;
        float4x4::CreateFromQuaternion__ref0_out1(orientation, rotation);

        ::float3 scale = entity->get_Scale();
        ::float4x4 size;
        float4x4::CreateScale__out3(scale.X, scale.Y, scale.Z, size);

        ::float4x4 rotationScale;
        float4x4::Multiply__ref0_ref1_out2(rotation, size, rotationScale);

        ::float3 position = entity->get_Position();
        ::float4x4 translation;
        float4x4::CreateTranslation__ref0_out1(position, translation);

        ::float4x4 world;
        float4x4::Multiply__ref0_ref1_out2(rotationScale, translation, world);
        return world;
    }

    /// Projects one model-space point through the supplied world-view-projection matrix into screen space.
    bool PsVitaRenderManager3D::TryProjectToScreen(
        const ::float3& point,
        const ::float4x4& worldViewProjection,
        const ::float4& viewport,
        ::float3& projectedPoint) {
        const float clipX = (point.X * worldViewProjection.M11)
            + (point.Y * worldViewProjection.M21)
            + (point.Z * worldViewProjection.M31)
            + worldViewProjection.M41;
        const float clipY = (point.X * worldViewProjection.M12)
            + (point.Y * worldViewProjection.M22)
            + (point.Z * worldViewProjection.M32)
            + worldViewProjection.M42;
        const float clipZ = (point.X * worldViewProjection.M13)
            + (point.Y * worldViewProjection.M23)
            + (point.Z * worldViewProjection.M33)
            + worldViewProjection.M43;
        const float clipW = (point.X * worldViewProjection.M14)
            + (point.Y * worldViewProjection.M24)
            + (point.Z * worldViewProjection.M34)
            + worldViewProjection.M44;

        if (clipW <= PsVitaMinimumProjectedW) {
            return false;
        }

        const float inverseW = 1.0f / clipW;
        const float normalizedX = clipX * inverseW;
        const float normalizedY = clipY * inverseW;
        const float normalizedZ = clipZ * inverseW;
        if (!std::isfinite(normalizedX) || !std::isfinite(normalizedY) || !std::isfinite(normalizedZ)) {
            return false;
        }

        projectedPoint.X = viewport.X + ((normalizedX + 1.0f) * 0.5f * viewport.Z);
        projectedPoint.Y = viewport.Y + ((1.0f - normalizedY) * 0.5f * viewport.W);
        projectedPoint.Z = std::clamp(normalizedZ, 0.0f, 1.0f);
        return std::isfinite(projectedPoint.X) && std::isfinite(projectedPoint.Y);
    }
}

#endif
