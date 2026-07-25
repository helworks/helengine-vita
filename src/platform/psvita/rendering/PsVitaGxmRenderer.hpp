#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <vita2d.h>

#include "float3.hpp"
#include "float2.hpp"
#include "float4x4.hpp"
#include "platform/psvita/rendering/PsVitaForwardLambertUniformBinder.hpp"
#include "platform/psvita/rendering/PsVitaForwardLambertVertex.hpp"
#include "platform/psvita/rendering/PsVitaGxmForwardLambertProgram.hpp"
#include "platform/psvita/rendering/PsVitaGxmShadowDepthProgram.hpp"
#include "platform/psvita/rendering/PsVitaGxmShadowMap.hpp"
#include "platform/psvita/rendering/PsVitaGxmShadowSubmissionMemory.hpp"
#include "platform/psvita/rendering/PsVitaGxmShadowSubmissionMemory.hpp"
#include "platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"
#include "platform/psvita/rendering/PsVitaQueuedQuad.hpp"
#include "platform/psvita/rendering/PsVitaSolidColorVertex.hpp"
#include "platform/psvita/shaders/PsVitaShaderBundleReader.hpp"

namespace helengine::psvita::rendering {
    class PsVitaRuntimeTexture;

    /// Owns the native PS Vita GXM frame lifecycle that will submit textured sprite and text quads.
    class PsVitaGxmRenderer final {
    public:
        /// Creates one uninitialized PS Vita GXM renderer foundation.
        PsVitaGxmRenderer();

        /// Initializes the native renderer state needed before the first submitted 2D frame.
        bool Initialize();

        /// Shuts down the native renderer state and releases any owned frame resources.
        void Shutdown();

        /// Gets whether the renderer has completed its initialization path.
        bool IsInitialized() const;

        /// Begins one new frame and records the requested clear color for later native submission.
        void BeginFrame(std::uint32_t clearColorAbgr);

        /// Begins the Vita2D-owned offscreen shadow depth pass before the main frame is opened.
        bool BeginShadowDepthPass();

        /// Ends the active offscreen shadow depth pass before main-frame rendering resumes.
        void EndShadowDepthPass();

        /// Draws one indexed mesh through the artifact-backed ShadowDepth program during the active shadow pass.
        bool DrawShadowDepthMesh(
            const ::float4x4& lightViewProjection,
            const ::float3* positions,
            int32_t vertexCount,
            const std::uint32_t* indices,
            int32_t indexCount,
            const std::string& shaderAssetId,
            const std::string& vertexProgramName,
            const std::string& pixelProgramName);

        /// Records one batch of textured quads for later native submission.
        void SubmitQuads(const std::vector<PsVitaQueuedQuad>& queuedQuads);

        /// Submits one projected 3D triangle through the existing native textured-triangle path.
        void SubmitTexturedTriangle(const PsVitaQueuedQuad& triangle);

        /// Records one batch of solid-color triangles for later native submission.
        void SubmitSolidColorTriangles(const std::vector<PsVitaSolidColorVertex>& vertices);

        /// Records one batch of already projected 3D mesh triangles as solid white GPU geometry.
        void SubmitSolidWhiteMeshTriangles(const std::vector<::float3>& vertices);

        /// Draws one indexed runtime mesh through the first programmable solid-color GXM path.
        bool DrawSolidColorMesh(
            const ::float4x4& worldViewProjection,
            const ::float3* positions,
            int32_t positionCount,
            const std::uint32_t* indices,
            int32_t indexCount,
            std::uint32_t colorAbgr);

        /// Draws one indexed runtime mesh through the artifact-backed GPU forward-Lambert path.
        bool DrawForwardLambertMesh(
            const ::float4x4& worldViewProjection,
            const ::float4x4& normalTransform,
            const ::float3* positions,
            const ::float3* normals,
            int32_t vertexCount,
            const std::uint32_t* indices,
            int32_t indexCount,
            std::uint32_t colorAbgr,
            const std::string& shaderAssetId,
            const std::string& vertexProgramName,
            const std::string& pixelProgramName,
            const std::string& variantName,
            const ::float3& lightDirection,
            const ::float3& lightColor,
            const ::float3& ambientColor);

        /// Draws one indexed runtime mesh through the textured artifact-backed Forward Standard Shader profile.
        bool DrawForwardStandardMesh(
            const ::float4x4& worldViewProjection,
            const ::float4x4& normalTransform,
            const ::float4x4& lightViewProjection,
            const ::float3* positions,
            const ::float3* normals,
            const ::float2* texCoords,
            int32_t vertexCount,
            const std::uint32_t* indices,
            int32_t indexCount,
            std::uint32_t colorAbgr,
            const std::string& shaderAssetId,
            const std::string& vertexProgramName,
            const std::string& pixelProgramName,
            const std::string& variantName,
            const ::float3& lightDirection,
            const ::float3& lightColor,
            const ::float3& ambientColor,
            PsVitaRuntimeTexture* diffuseTexture);

        /// Presents the current frame through the PS Vita display path.
        void PresentFrame();

        /// Gets the number of quads most recently recorded for the current frame.
        std::size_t GetSubmittedQuadCount() const;

    private:
        /// Returns the padded native Vita texture dimension used to avoid odd-sized runtime uploads that destabilize Vita3K's texture destroy path.
        static std::uint32_t CalculatePaddedTextureDimension(std::uint32_t textureDimension);

        /// Lazily uploads one runtime texture into a native PS Vita texture allocation before the first draw that references it.
        void EnsureUploaded(PsVitaRuntimeTexture* runtimeTexture);

        /// Returns the persistent opaque white texture used when one Standard material has no authored diffuse texture asset.
        vita2d_texture* GetOrCreateStandardWhiteTexture();

        /// Waits for completed rendering before releasing shadow-caster allocations retained across the offscreen/main-frame boundary.
        void ReleaseCompletedShadowSubmissionMemory();

        /// Submits one queued quad through the GPU-backed textured-triangle path.
        void SubmitQuad(const PsVitaQueuedQuad& queuedQuad);

        /// Interpolates one textured vertex for the scale-stable triangle subdivision path.
        static PsVitaTexturedQuadVertex InterpolateTexturedVertex(const PsVitaTexturedQuadVertex& left, const PsVitaTexturedQuadVertex& right);

        /// Uploads one world-view-projection matrix into the runtime-compiled solid-color vertex shader uniform buffer.
        void UploadSolidColorWorldViewProjection(
            SceGxmContext* context,
            const SceGxmProgramParameter* parameter,
            const ::float4x4& worldViewProjection);

        /// Uploads one packed ABGR solid color into the runtime-compiled solid-color fragment shader uniform buffer.
        void UploadSolidColorBaseColor(
            SceGxmContext* context,
            const SceGxmProgramParameter* parameter,
            std::uint32_t colorAbgr);

        /// Stores whether the renderer completed its initialization path.
        bool Initialized;

        /// Stores whether one frame is currently open for drawing.
        bool FrameBegun;

        /// Stores the clear color requested for the current frame.
        std::uint32_t ActiveClearColorAbgr;

        /// Stores the number of submitted quads for the current frame.
        std::size_t SubmittedQuadCount;

        /// Stores the artifact-backed GXM shader state used by the GPU forward-Lambert mesh path.
        PsVitaGxmSolidColorProgram SolidColorProgram;

        /// Stores the artifact-backed GXM forward-Lambert shader state.
        PsVitaGxmForwardLambertProgram ForwardLambertProgram;

        /// Stores the artifact-backed GXM state used by the textured Forward Standard Shader profile.
        PsVitaGxmForwardLambertProgram ForwardStandardProgram;

        /// Stores the artifact-backed GXM state used by the depth-only shadow caster pass.
        PsVitaGxmShadowDepthProgram ShadowDepthProgram;

        /// Stores the Vita2D-owned offscreen texture that receives shadow-caster depth.
        PsVitaGxmShadowMap ShadowMap;

        /// Stores GPU-visible caster geometry until the frame that references it has completed.
        std::vector<std::unique_ptr<PsVitaGxmShadowSubmissionMemory>> ShadowSubmissionMemory;

        /// Stores the complete cooked bundle that owns every material-addressable program pair.
        shaders::PsVitaShaderBundleReader ShaderBundle;

        /// Stores whether the cooked shader bundle has been loaded for this renderer lifetime.
        bool ShaderBundleLoaded;

        /// Stores the current untextured program lookup key.
        std::string ForwardLambertProgramKey;

        /// Stores the current textured program lookup key.
        std::string ForwardStandardProgramKey;

        /// Stores the current depth-only program lookup key.
        std::string ShadowDepthProgramKey;

        /// Stores the persistent opaque white texture used to satisfy the Standard Shader diffuse sampler when a material is colour-only.
        vita2d_texture* StandardWhiteTexture;
    };
}

#endif
