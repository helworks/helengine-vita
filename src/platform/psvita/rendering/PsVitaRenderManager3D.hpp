#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <vector>

class Entity;
class AmbientLightComponent;
class DirectionalLightComponent;
class ICamera;
class IDrawable3D;
class MaterialAsset;
class MeshComponent;
class ModelAsset;
class RuntimeMaterial;
class RuntimeModel;
class ShaderMaterialAsset;
template<typename T>
class Array;

#include "IRenderVisitor3D.hpp"
#include "MaterialCullMode.hpp"
#include "RenderManager3D.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"
#include "platform/psvita/rendering/PsVitaSolidColorVertex.hpp"

namespace helengine::psvita::rendering {
    struct PsVitaCompiledShaderMaterial;
    class PsVitaCompiledShaderRuntimeMaterial;
    class PsVitaGxmRenderer;
    class PsVitaRuntimeModel;
    class PsVitaRuntimeSubmesh;
}

namespace helengine::psvita {
    /// Provides the first PS Vita native 3D mesh bridge so runtime models can render through the emulator-safe Lambert fallback path.
    class PsVitaRenderManager3D final : public ::RenderManager3D, public ::IRenderVisitor3D {
    public:
        /// Creates the PS Vita 3D renderer with the Vita display size.
        PsVitaRenderManager3D();

        /// Assigns the native PS Vita GXM renderer that will receive projected Lambert-lit triangle batches.
        void SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer);

        /// Traverses camera-owned 3D queues, submits Lambert-lit mesh geometry, and forwards ordered 2D queues to the Vita 2D renderer.
        void Draw() override;

        /// Builds one concrete runtime material from one packaged cooked platform material asset.
        ::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds one concrete runtime material from one deserialized material asset payload.
        ::RuntimeMaterial* BuildMaterialFromCooked(::MaterialAsset* materialAsset);

        /// Builds one concrete runtime material from one deserialized shader material asset payload.
        ::RuntimeMaterial* BuildMaterialFromCooked(::ShaderMaterialAsset* materialAsset);

        /// Builds one concrete PS Vita runtime model from one packaged cooked model asset.
        ::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds one concrete PS Vita runtime model from raw data.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;

        /// Visits one ordered 3D drawable and renders supported mesh content through the Lambert fallback path.
        void Visit(::IDrawable3D* drawable) override;

    private:
        /// Draws one camera's ordered 3D queue and forwards its 2D queue to the overlay renderer.
        void DrawCamera(::ICamera* camera);

        /// Draws one mesh component with one concrete PS Vita runtime model.
        void DrawRuntimeModel(::MeshComponent* meshComponent, rendering::PsVitaRuntimeModel* runtimeModel);

        /// Attempts to draw one runtime model through the programmable solid-color GXM mesh path.
        bool TryDrawRuntimeModelWithSolidColorPath(
            const ::float4x4& worldViewProjection,
            ::MeshComponent* meshComponent,
            rendering::PsVitaRuntimeModel* runtimeModel);

        /// Resolves the first active runtime directional light that should drive the Lambert fallback path.
        static ::DirectionalLightComponent* ResolveActiveDirectionalLight();

        /// Resolves one normalized world-space light direction from the supplied runtime directional light.
        static ::float3 ResolveDirectionalLightDirection(::DirectionalLightComponent* lightComponent);

        /// Resolves the accumulated ambient light color from the runtime object manager.
        static ::float3 ResolveAmbientLightColor();

        /// Resolves the Lambert fallback base color that should be used for one runtime submesh draw.
        static std::uint32_t ResolveLambertBaseColor(::MeshComponent* meshComponent, int32_t submeshIndex);

        /// Resolves the cull mode that should be applied to one runtime submesh in the Lambert fallback path.
        static ::MaterialCullMode ResolveSubmeshCullMode(::MeshComponent* meshComponent, int32_t submeshIndex);

        /// Returns whether one projected triangle should be discarded before painter sorting based on material cull mode and screen-space winding.
        static bool ShouldCullProjectedTriangle(
            ::MaterialCullMode cullMode,
            const ::float3& projectedVertex0,
            const ::float3& projectedVertex1,
            const ::float3& projectedVertex2);

        /// Builds one packed ABGR vertex color from the supplied base color and Lambert lighting inputs.
        static std::uint32_t BuildLambertVertexColor(
            std::uint32_t baseColorAbgr,
            const ::float3& worldNormal,
            const ::float3& lightDirection,
            const ::float3& directionalLightColor,
            const ::float3& ambientLightColor,
            bool hasDirectionalLight);

        /// Builds one Vita-specific runtime material from one cooked compiled-shader material payload.
        static ::RuntimeMaterial* BuildCompiledShaderRuntimeMaterial(const rendering::PsVitaCompiledShaderMaterial& materialAsset);

        /// Resolves the runtime material that should drive one runtime submesh draw.
        static ::RuntimeMaterial* ResolveSubmeshMaterial(::MeshComponent* meshComponent, int32_t submeshIndex);

        /// Resolves the solid-color mesh base color that should be used for one runtime submesh draw.
        static std::uint32_t ResolveSolidColorSubmeshColor(::MeshComponent* meshComponent, int32_t submeshIndex);

        /// Resolves one cooked material's standard base-color constant buffer into packed Vita color.
        static std::uint32_t ResolveCookedMaterialBaseColorAbgr(::ShaderMaterialAsset* materialAsset);

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

        /// Stores the native PS Vita GXM renderer that receives projected Lambert-lit triangle batches.
        rendering::PsVitaGxmRenderer* GxmRenderer = nullptr;

        /// Stores the active camera while the ordered 3D queue is being visited.
        ::ICamera* ActiveCamera = nullptr;

        /// Stores the resolved screen-space viewport for the active camera.
        ::float4 ActiveViewport;

        /// Stores the active camera view-projection matrix for the current 3D queue traversal.
        ::float4x4 ActiveViewProjection;

        /// Stores the projected Lambert-lit mesh triangles waiting for GPU submission.
        std::vector<rendering::PsVitaSolidColorVertex> QueuedMeshTriangles;
    };
}

#endif
