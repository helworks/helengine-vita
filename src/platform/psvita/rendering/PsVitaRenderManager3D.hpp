#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

class Entity;
class ICamera;
class IDrawable3D;
class MaterialAsset;
class MeshComponent;
class ModelAsset;
class RuntimeMaterial;
class RuntimeModel;
template<typename T>
class Array;

#include "IRenderVisitor3D.hpp"
#include "RenderManager3D.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"

namespace helengine::psvita::rendering {
    class PsVitaGxmRenderer;
    class PsVitaRuntimeModel;
    class PsVitaRuntimeSubmesh;
}

namespace helengine::psvita {
    /// Provides the first PS Vita native 3D mesh bridge so runtime models can render as solid white GPU geometry.
    class PsVitaRenderManager3D final : public ::RenderManager3D, public ::IRenderVisitor3D {
    public:
        /// Creates the PS Vita 3D renderer with the Vita display size.
        PsVitaRenderManager3D();

        /// Assigns the native PS Vita GXM renderer that will receive white mesh triangle batches.
        void SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer);

        /// Traverses camera-owned 3D queues, submits white mesh geometry, and forwards ordered 2D queues to the Vita 2D renderer.
        void Draw() override;

        /// Builds one concrete runtime material from one packaged cooked platform material asset.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath);

        /// Builds one concrete runtime material from one deserialized material asset payload.
        ::RuntimeMaterial* BuildMaterialFromCooked(::MaterialAsset* materialAsset);

        /// Builds one concrete PS Vita runtime model from one packaged cooked model asset.
        ::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath);

        /// Builds one concrete PS Vita runtime model from raw data.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;

        /// Visits one ordered 3D drawable and renders supported mesh content through the white triangle path.
        void Visit(::IDrawable3D* drawable) override;

    private:
        /// Draws one camera's ordered 3D queue and forwards its 2D queue to the overlay renderer.
        void DrawCamera(::ICamera* camera);

        /// Draws one mesh component with one concrete PS Vita runtime model.
        void DrawRuntimeModel(::MeshComponent* meshComponent, rendering::PsVitaRuntimeModel* runtimeModel);

        /// Attempts to draw one runtime model through the programmable solid-color GXM mesh path.
        bool TryDrawRuntimeModelWithSolidColorPath(
            const ::float4x4& worldViewProjection,
            rendering::PsVitaRuntimeModel* runtimeModel);

        /// Copies one runtime submesh array from the raw model asset into PS Vita-owned submesh objects.
        Array<rendering::PsVitaRuntimeSubmesh*>* BuildRuntimeSubmeshes(::ModelAsset* data, const std::vector<std::uint32_t>& resolvedIndices);

        /// Builds the shared camera view-projection matrix used by the current mesh pass.
        ::float4x4 BuildCameraViewProjection(::ICamera* camera, const ::float4& viewport) const;

        /// Builds the current drawable world transform from entity position, orientation, and scale.
        static ::float4x4 BuildWorldTransform(::Entity* entity);

        /// Projects one model-space point through the supplied world-view-projection matrix into screen space.
        static bool TryProjectToScreen(
            const ::float3& point,
            const ::float4x4& worldViewProjection,
            const ::float4& viewport,
            ::float3& projectedPoint);

        /// Stores the native PS Vita GXM renderer that receives white mesh triangle batches.
        rendering::PsVitaGxmRenderer* GxmRenderer = nullptr;

        /// Stores the active camera while the ordered 3D queue is being visited.
        ::ICamera* ActiveCamera = nullptr;

        /// Stores the resolved screen-space viewport for the active camera.
        ::float4 ActiveViewport;

        /// Stores the active camera view-projection matrix for the current 3D queue traversal.
        ::float4x4 ActiveViewProjection;

        /// Stores the projected white mesh triangles waiting for GPU submission.
        std::vector<::float3> QueuedMeshTriangles;
    };
}

#endif
