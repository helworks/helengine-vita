#include "platform/psvita/rendering/PsVitaRenderManager3D.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "EditorAssetBinarySerializer.hpp"
#include "EngineBinaryHeader.hpp"
#include "EngineBinaryHeaderSerializer.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "MaterialAsset.hpp"
#include "MaterialBlendMode.hpp"
#include "MaterialCullMode.hpp"
#include "MaterialRenderState.hpp"
#include "MeshComponent.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ObjectManager.hpp"
#include "RuntimeMaterial.hpp"
#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp"
#include "platform/psvita/rendering/PsVitaCompiledShaderRuntimeMaterial.hpp"
#include "platform/psvita/rendering/PsVitaPackedModelReader.hpp"
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
    constexpr float PsVitaMinimumNearPlaneDistance = 0.01f;
    constexpr float PsVitaMinimumPlaneSeparation = 0.01f;
    constexpr float PsVitaDefaultNearPlaneDistance = 0.1f;
    constexpr float PsVitaDefaultFarPlaneDistance = 100.0f;
    constexpr float PsVitaMinimumProjectedW = 0.0001f;
    constexpr const char* PsVitaBootTracePath = "ux0:/data/helengine_psvita_boot.log";
    int PsVitaCameraDiagnosticSamplesRemaining = 4;
    bool PsVitaLoggedProjectionDiagnostics = false;
    int PsVitaProjectionDiagnosticSamplesRemaining = 12;
    unsigned int PsVitaLoggedMeshDiagnosticsCount = 0u;

    /// Stores one projected triangle and its average depth so the temporary white mesh pass can draw farther triangles first without a material/depth system.
    struct ProjectedTriangle final {
        ::float3 Vertex0;
        ::float3 Vertex1;
        ::float3 Vertex2;
        float AverageDepth;
    };

    /// Returns whether the authored normalized viewport uses stacked dual-screen vertical units.
    bool UsesStackedDualScreenViewportUnits(const ::float4& viewport, double targetWidth, double targetHeight) {
        const double expectedStackedHeight = targetWidth * 1.5d;
        if (std::abs(targetHeight - expectedStackedHeight) > 0.5d) {
            return false;
        }

        return viewport.Y >= 0.0f && (viewport.Y + viewport.W) <= 2.0f;
    }

    /// Resolves one authored viewport into pixel-space coordinates for the active PS Vita target.
    ::float4 ResolveViewport(const ::float4& viewport, double targetWidth, double targetHeight) {
        if (targetWidth <= 0.0) {
            throw new ArgumentOutOfRangeException("targetWidth", "Target width must be greater than zero.");
        }
        if (targetHeight <= 0.0) {
            throw new ArgumentOutOfRangeException("targetHeight", "Target height must be greater than zero.");
        }

        double offsetX = viewport.X;
        double offsetY = viewport.Y;
        double width = viewport.Z;
        double height = viewport.W;
        if (width <= 1.0 && height <= 1.0) {
            offsetX *= targetWidth;
            width *= targetWidth;
            if (UsesStackedDualScreenViewportUnits(viewport, targetWidth, targetHeight)) {
                const double screenHeight = targetHeight * 0.5d;
                offsetY *= screenHeight;
                height *= screenHeight;
            } else {
                offsetY *= targetHeight;
                height *= targetHeight;
            }
        }

        return ::float4(
            static_cast<float>(offsetX),
            static_cast<float>(offsetY),
            static_cast<float>(width),
            static_cast<float>(height));
    }

    /// Clamps one near clip plane for the temporary Vita perspective projection path.
    float ClampNearPlaneDistance(float nearPlaneDistance, float farPlaneDistance) {
        const float minimumFarPlaneDistance = std::max(PsVitaMinimumNearPlaneDistance + PsVitaMinimumPlaneSeparation, farPlaneDistance);
        return std::min(
            std::max(PsVitaMinimumNearPlaneDistance, nearPlaneDistance),
            minimumFarPlaneDistance - PsVitaMinimumPlaneSeparation);
    }

    /// Clamps one far clip plane for the temporary Vita perspective projection path.
    float ClampFarPlaneDistance(float nearPlaneDistance, float farPlaneDistance) {
        const float minimumNearPlaneDistance = std::max(PsVitaMinimumNearPlaneDistance, nearPlaneDistance);
        return std::max(minimumNearPlaneDistance + PsVitaMinimumPlaneSeparation, farPlaneDistance);
    }

    /// Builds the temporary Vita perspective projection until generated camera clip planes are surfaced in the native core.
    ::float4x4 CreatePerspectiveProjection(float fieldOfView, float aspectRatio) {
        const float nearPlaneDistance = ClampNearPlaneDistance(PsVitaDefaultNearPlaneDistance, PsVitaDefaultFarPlaneDistance);
        const float farPlaneDistance = ClampFarPlaneDistance(nearPlaneDistance, PsVitaDefaultFarPlaneDistance);

        ::float4x4 projection;
        float4x4::CreatePerspectiveFieldOfView(fieldOfView, aspectRatio, nearPlaneDistance, farPlaneDistance, projection);
        return projection;
    }
}

namespace helengine::psvita {
    /// Creates the PS Vita 3D renderer with the Vita display size.
    PsVitaRenderManager3D::PsVitaRenderManager3D() {
        set_MainWindowSize(new ::int2(PsVitaScreenWidth, PsVitaScreenHeight));
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
            ::ICamera* camera = (*cameras)[cameraIndex];
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

        rendering::PsVitaCompiledShaderMaterial compiledShaderMaterial;
        if (rendering::PsVitaCompiledShaderMaterialReader::TryRead(cookedAssetPath, compiledShaderMaterial)) {
            return BuildCompiledShaderRuntimeMaterial(compiledShaderMaterial);
        }

        ::FileStream* stream = nullptr;
        ::EngineBinaryHeader* header = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            header = ::EngineBinaryHeaderSerializer::Read(stream);
            asset = ::EditorAssetBinarySerializer::Deserialize(stream, header);
            header = nullptr;
            delete stream;
            stream = nullptr;

            ::MaterialAsset* cookedMaterialAsset = he_cpp_try_cast<MaterialAsset>(asset);
            if (cookedMaterialAsset == nullptr) {
                throw new InvalidOperationException("PS Vita cooked material payloads must deserialize as MaterialAsset.");
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

    /// Builds one concrete runtime material from one deserialized material asset payload.
    ::RuntimeMaterial* PsVitaRenderManager3D::BuildMaterialFromCooked(::MaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw new ArgumentNullException("materialAsset");
        }
        if (materialAsset->RenderState == nullptr) {
            throw new InvalidOperationException("PS Vita cooked material payloads must include a render state.");
        }

        auto* runtimeMaterial = new ::RuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset->get_Id());
        runtimeMaterial->SetRenderState(materialAsset->RenderState);
        return runtimeMaterial;
    }

    /// Builds one Vita-specific runtime material from one cooked compiled-shader material payload.
    ::RuntimeMaterial* PsVitaRenderManager3D::BuildCompiledShaderRuntimeMaterial(const rendering::PsVitaCompiledShaderMaterial& materialAsset) {
        auto* runtimeMaterial = new rendering::PsVitaCompiledShaderRuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset.ShaderAssetId);
        runtimeMaterial->SetShaderAssetId(materialAsset.ShaderAssetId);
        runtimeMaterial->SetVertexProgramName(materialAsset.VertexProgramName);
        runtimeMaterial->SetPixelProgramName(materialAsset.PixelProgramName);
        runtimeMaterial->SetVariantName(materialAsset.VariantName);
        runtimeMaterial->SetBaseColorAbgr(materialAsset.BaseColorAbgr);
        return runtimeMaterial;
    }

    /// Builds one concrete PS Vita runtime model from one packaged cooked model asset.
    ::RuntimeModel* PsVitaRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Cooked model asset path must be provided.", "cookedAssetPath");
        }

        ::RuntimeModel* packedRuntimeModel = rendering::PsVitaPackedModelReader::TryRead(cookedAssetPath);
        if (packedRuntimeModel != nullptr) {
            return packedRuntimeModel;
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
        if (data->Positions == nullptr || data->Positions->Length == 0) {
            throw new ArgumentException("Model data must include positions.");
        }

        std::vector<::float3> copiedPositions;
        copiedPositions.reserve(static_cast<std::size_t>(data->Positions->Length));
        for (int32_t positionIndex = 0; positionIndex < data->Positions->Length; ++positionIndex) {
            copiedPositions.push_back((*data->Positions)[positionIndex]);
        }

        ::ModelAssetIndexData* indexData = ::ModelAssetIndexData::Resolve(data);
        std::vector<std::uint32_t> resolvedIndices;
        resolvedIndices.reserve(static_cast<std::size_t>(std::max(0, indexData->IndexCount)));
        if (indexData->Uses32BitIndices && indexData->Indices32 != nullptr) {
            for (int32_t index = 0; index < indexData->Indices32->Length; ++index) {
                resolvedIndices.push_back((*indexData->Indices32)[index]);
            }
        } else if (indexData->Indices16 != nullptr) {
            for (int32_t index = 0; index < indexData->Indices16->Length; ++index) {
                resolvedIndices.push_back(static_cast<std::uint32_t>((*indexData->Indices16)[index]));
            }
        }
        delete indexData;

        auto* runtimeModel = new rendering::PsVitaRuntimeModel(std::move(copiedPositions));
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

        ::int2* mainWindowSize = get_MainWindowSize();
        if (mainWindowSize == nullptr) {
            ActiveCamera = nullptr;
            return;
        }
        ActiveCamera = camera;
        ActiveViewport = ResolveViewport(
            camera->get_Viewport(),
            static_cast<double>(mainWindowSize->X),
            static_cast<double>(mainWindowSize->Y));
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
        float4x4::Multiply(world, ActiveViewProjection, worldViewProjection);

        Array<rendering::PsVitaRuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
        if (submeshes == nullptr || submeshes->Length == 0) {
            return;
        }
        if (TryDrawRuntimeModelWithSolidColorPath(worldViewProjection, meshComponent, runtimeModel)) {
            return;
        }

        std::size_t attemptedTriangleCount = 0u;
        std::vector<ProjectedTriangle> projectedTriangles;
        ::float3 firstProjectedVertex0;
        ::float3 firstProjectedVertex1;
        ::float3 firstProjectedVertex2;
        bool hasFirstProjectedTriangle = false;
        for (int32_t submeshIndex = 0; submeshIndex < submeshes->Length; ++submeshIndex) {
            rendering::PsVitaRuntimeSubmesh* submesh = (*submeshes)[submeshIndex];
            if (submesh == nullptr) {
                continue;
            }

            const std::vector<std::uint32_t>& triangleIndices = submesh->GetTriangleIndices();
            for (std::size_t index = 0; index + 2 < triangleIndices.size(); index += 3) {
                attemptedTriangleCount++;
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

                if (!hasFirstProjectedTriangle) {
                    firstProjectedVertex0 = projectedVertex0;
                    firstProjectedVertex1 = projectedVertex1;
                    firstProjectedVertex2 = projectedVertex2;
                    hasFirstProjectedTriangle = true;
                }

                projectedTriangles.push_back(ProjectedTriangle {
                    projectedVertex0,
                    projectedVertex1,
                    projectedVertex2,
                    (projectedVertex0.Z + projectedVertex1.Z + projectedVertex2.Z) / 3.0f
                });
            }
        }

        if (PsVitaLoggedMeshDiagnosticsCount < 2u) {
            std::FILE* file = std::fopen(PsVitaBootTracePath, "a");
            if (file != nullptr) {
                ::float3 meshCenter = meshComponent->get_Parent()->get_Position();
                ::float3 cameraPosition;
                ::float4 cameraOrientation;
                ::float3 negativeZForward;
                ::float3 positiveZForward;
                float negativeZAlignment = 0.0f;
                float positiveZAlignment = 0.0f;
                if (ActiveCamera != nullptr && ActiveCamera->get_Parent() != nullptr) {
                    cameraPosition = ActiveCamera->get_Parent()->get_Position();
                    cameraOrientation = ActiveCamera->get_Parent()->get_Orientation();
                    negativeZForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);
                    positiveZForward = float4::RotateVector(::float3(0.0f, 0.0f, 1.0f), cameraOrientation);
                    ::float3 toMesh = float3::Normalize(meshCenter - cameraPosition);
                    negativeZAlignment = float3::Dot(negativeZForward, toMesh);
                    positiveZAlignment = float3::Dot(positiveZForward, toMesh);
                }
                ::float3 projectedOrigin;
                const bool projectedOriginVisible = TryProjectToScreen(::float3(0.0f, 0.0f, 0.0f), worldViewProjection, ActiveViewport, projectedOrigin);

                char buffer[512];
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Render3DProjection: meshIndex=%u viewport=(%.2f,%.2f,%.2f,%.2f) meshCenter=(%.2f,%.2f,%.2f) camera=(%.2f,%.2f,%.2f) alignNegZ=%.4f alignPosZ=%.4f originVisible=%u originScreen=(%.2f,%.2f,%.4f) attempted=%u projected=%u queuedVertices=%u firstTriangle=(%.2f,%.2f,%.4f)|(%.2f,%.2f,%.4f)|(%.2f,%.2f,%.4f)",
                    PsVitaLoggedMeshDiagnosticsCount,
                    ActiveViewport.X,
                    ActiveViewport.Y,
                    ActiveViewport.Z,
                    ActiveViewport.W,
                    meshCenter.X,
                    meshCenter.Y,
                    meshCenter.Z,
                    cameraPosition.X,
                    cameraPosition.Y,
                    cameraPosition.Z,
                    negativeZAlignment,
                    positiveZAlignment,
                    projectedOriginVisible ? 1u : 0u,
                    projectedOriginVisible ? projectedOrigin.X : -1.0f,
                    projectedOriginVisible ? projectedOrigin.Y : -1.0f,
                    projectedOriginVisible ? projectedOrigin.Z : -1.0f,
                    static_cast<unsigned int>(attemptedTriangleCount),
                    static_cast<unsigned int>(projectedTriangles.size()),
                    static_cast<unsigned int>(projectedTriangles.size() * 3u),
                    hasFirstProjectedTriangle ? firstProjectedVertex0.X : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex0.Y : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex0.Z : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex1.X : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex1.Y : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex1.Z : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex2.X : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex2.Y : -1.0f,
                    hasFirstProjectedTriangle ? firstProjectedVertex2.Z : -1.0f);
                std::fputs(buffer, file);
                std::fputc('\n', file);
                std::fclose(file);
            }

            PsVitaLoggedProjectionDiagnostics = true;
            PsVitaLoggedMeshDiagnosticsCount++;
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

    /// Attempts to draw one runtime model through the programmable solid-color GXM mesh path.
    bool PsVitaRenderManager3D::TryDrawRuntimeModelWithSolidColorPath(
        const ::float4x4& worldViewProjection,
        ::MeshComponent* meshComponent,
        rendering::PsVitaRuntimeModel* runtimeModel) {
        if (GxmRenderer == nullptr || meshComponent == nullptr || runtimeModel == nullptr) {
            return false;
        }

        const std::vector<::float3>& positions = runtimeModel->GetPositions();
        if (positions.empty()) {
            return false;
        }

        Array<rendering::PsVitaRuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
        if (submeshes == nullptr || submeshes->Length == 0) {
            return false;
        }

        bool drewAnySubmesh = false;
        for (int32_t submeshIndex = 0; submeshIndex < submeshes->Length; ++submeshIndex) {
            rendering::PsVitaRuntimeSubmesh* submesh = (*submeshes)[submeshIndex];
            if (submesh == nullptr) {
                return false;
            }

            const std::vector<std::uint32_t>& triangleIndices = submesh->GetTriangleIndices();
            if (triangleIndices.empty()) {
                continue;
            }

            std::uint32_t baseColorAbgr = ResolveSolidColorSubmeshColor(meshComponent, submeshIndex);
            if (!GxmRenderer->DrawSolidColorMesh(
                worldViewProjection,
                positions.data(),
                static_cast<int32_t>(positions.size()),
                triangleIndices.data(),
                static_cast<int32_t>(triangleIndices.size()),
                baseColorAbgr)) {
                return false;
            }

            drewAnySubmesh = true;
        }

        return drewAnySubmesh;
    }

    /// Resolves the solid-color mesh base color that should be used for one runtime submesh draw.
    std::uint32_t PsVitaRenderManager3D::ResolveSolidColorSubmeshColor(::MeshComponent* meshComponent, int32_t submeshIndex) {
        if (meshComponent == nullptr) {
            return 0xFFFFFFFFu;
        }

        ::RuntimeMaterial* material = meshComponent->get_Material();
        if (material == nullptr) {
            return 0xFFFFFFFFu;
        }

        rendering::PsVitaCompiledShaderRuntimeMaterial* compiledShaderMaterial = dynamic_cast<rendering::PsVitaCompiledShaderRuntimeMaterial*>(material);

        return compiledShaderMaterial == nullptr
            ? 0xFFFFFFFFu
            : compiledShaderMaterial->GetBaseColorAbgr();
    }

    /// Copies one runtime submesh array from the raw model asset into PS Vita-owned submesh objects.
    Array<rendering::PsVitaRuntimeSubmesh*>* PsVitaRenderManager3D::BuildRuntimeSubmeshes(::ModelAsset* data, const std::vector<std::uint32_t>& resolvedIndices) {
        if (data == nullptr) {
            throw new ArgumentNullException("data");
        }

        const int32_t positionCount = data->Positions == nullptr ? 0 : data->Positions->Length;
        const int32_t elementCount = resolvedIndices.empty() ? positionCount : static_cast<int32_t>(resolvedIndices.size());
        if (elementCount == 0) {
            return Array<rendering::PsVitaRuntimeSubmesh*>::Empty();
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

        auto* runtimeSubmeshes = new Array<rendering::PsVitaRuntimeSubmesh*>(1);
        (*runtimeSubmeshes)[0] = new rendering::PsVitaRuntimeSubmesh(String::Empty, 0, elementCount, std::move(defaultTriangleIndices));
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
        ::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        ::float3 cameraUp = float4::RotateVector(::float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        ::float3 cameraTarget = cameraPosition + cameraForward;

        ::float4x4 view;
        float4x4::CreateLookAt(cameraPosition, cameraTarget, cameraUp, view);

        ::float4x4 projection = CreatePerspectiveProjection(PsVitaPerspectiveFieldOfViewRadians, viewport.Z / viewport.W);

        ::float4x4 viewProjection;
        float4x4::Multiply(view, projection, viewProjection);
        if (PsVitaCameraDiagnosticSamplesRemaining > 0) {
            PsVitaCameraDiagnosticSamplesRemaining--;
            std::FILE* file = std::fopen(PsVitaBootTracePath, "a");
            if (file != nullptr) {
                char buffer[768];
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Render3DCamera: position=(%.4f,%.4f,%.4f) forward=(%.4f,%.4f,%.4f) up=(%.4f,%.4f,%.4f) target=(%.4f,%.4f,%.4f) viewProjRow1=(%.4f,%.4f,%.4f,%.4f) viewProjRow2=(%.4f,%.4f,%.4f,%.4f) viewProjRow3=(%.4f,%.4f,%.4f,%.4f) viewProjRow4=(%.4f,%.4f,%.4f,%.4f)",
                    cameraPosition.X,
                    cameraPosition.Y,
                    cameraPosition.Z,
                    cameraForward.X,
                    cameraForward.Y,
                    cameraForward.Z,
                    cameraUp.X,
                    cameraUp.Y,
                    cameraUp.Z,
                    cameraTarget.X,
                    cameraTarget.Y,
                    cameraTarget.Z,
                    viewProjection.M11,
                    viewProjection.M12,
                    viewProjection.M13,
                    viewProjection.M14,
                    viewProjection.M21,
                    viewProjection.M22,
                    viewProjection.M23,
                    viewProjection.M24,
                    viewProjection.M31,
                    viewProjection.M32,
                    viewProjection.M33,
                    viewProjection.M34,
                    viewProjection.M41,
                    viewProjection.M42,
                    viewProjection.M43,
                    viewProjection.M44);
                std::fputs(buffer, file);
                std::fputc('\n', file);
                std::fclose(file);
            }
        }
        return viewProjection;
    }

    /// Builds the current drawable world transform from entity position, orientation, and scale.
    ::float4x4 PsVitaRenderManager3D::BuildWorldTransform(::Entity* entity) {
        if (entity == nullptr) {
            throw new ArgumentNullException("entity");
        }

        ::float4 orientation = entity->get_Orientation();
        ::float4x4 rotation;
        float4x4::CreateFromQuaternion(orientation, rotation);

        ::float3 scale = entity->get_Scale();
        ::float4x4 size;
        float4x4::CreateScale(scale.X, scale.Y, scale.Z, size);

        ::float4x4 rotationScale;
        float4x4::Multiply(rotation, size, rotationScale);

        ::float3 position = entity->get_Position();
        ::float4x4 translation;
        float4x4::CreateTranslation(position, translation);

        ::float4x4 world;
        float4x4::Multiply(rotationScale, translation, world);
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
        const bool shouldLogProjectionSample = PsVitaProjectionDiagnosticSamplesRemaining > 0;
        if (shouldLogProjectionSample) {
            PsVitaProjectionDiagnosticSamplesRemaining--;
        }

        if (clipW <= PsVitaMinimumProjectedW) {
            if (shouldLogProjectionSample) {
                std::FILE* file = std::fopen(PsVitaBootTracePath, "a");
                if (file != nullptr) {
                    char buffer[512];
                    std::snprintf(
                        buffer,
                        sizeof(buffer),
                        "Render3DProjectionSample: reject=clipW point=(%.4f,%.4f,%.4f) clip=(%.4f,%.4f,%.4f,%.4f)",
                        point.X,
                        point.Y,
                        point.Z,
                        clipX,
                        clipY,
                        clipZ,
                        clipW);
                    std::fputs(buffer, file);
                    std::fputc('\n', file);
                    std::fclose(file);
                }
            }
            return false;
        }

        const float inverseW = 1.0f / clipW;
        const float normalizedX = clipX * inverseW;
        const float normalizedY = clipY * inverseW;
        const float normalizedZ = clipZ * inverseW;
        if (!std::isfinite(normalizedX) || !std::isfinite(normalizedY) || !std::isfinite(normalizedZ)) {
            if (shouldLogProjectionSample) {
                std::FILE* file = std::fopen(PsVitaBootTracePath, "a");
                if (file != nullptr) {
                    char buffer[512];
                    std::snprintf(
                        buffer,
                        sizeof(buffer),
                        "Render3DProjectionSample: reject=nonfinite point=(%.4f,%.4f,%.4f) clip=(%.4f,%.4f,%.4f,%.4f) ndc=(%.4f,%.4f,%.4f)",
                        point.X,
                        point.Y,
                        point.Z,
                        clipX,
                        clipY,
                        clipZ,
                        clipW,
                        normalizedX,
                        normalizedY,
                        normalizedZ);
                    std::fputs(buffer, file);
                    std::fputc('\n', file);
                    std::fclose(file);
                }
            }
            return false;
        }

        projectedPoint.X = viewport.X + ((normalizedX + 1.0f) * 0.5f * viewport.Z);
        projectedPoint.Y = viewport.Y + ((1.0f - normalizedY) * 0.5f * viewport.W);
        projectedPoint.Z = std::clamp(normalizedZ, 0.0f, 1.0f);
        if (shouldLogProjectionSample) {
            std::FILE* file = std::fopen(PsVitaBootTracePath, "a");
            if (file != nullptr) {
                char buffer[512];
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "Render3DProjectionSample: accept point=(%.4f,%.4f,%.4f) clip=(%.4f,%.4f,%.4f,%.4f) ndc=(%.4f,%.4f,%.4f) screen=(%.2f,%.2f,%.4f)",
                    point.X,
                    point.Y,
                    point.Z,
                    clipX,
                    clipY,
                    clipZ,
                    clipW,
                    normalizedX,
                    normalizedY,
                    normalizedZ,
                    projectedPoint.X,
                    projectedPoint.Y,
                    projectedPoint.Z);
                std::fputs(buffer, file);
                std::fputc('\n', file);
                std::fclose(file);
            }
        }
        return std::isfinite(projectedPoint.X) && std::isfinite(projectedPoint.Y);
    }
}

#endif
