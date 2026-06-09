#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <vector>

#include "float3.hpp"
#include "float4x4.hpp"
#include "platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"
#include "platform/psvita/rendering/PsVitaQueuedQuad.hpp"
#include "platform/psvita/rendering/PsVitaSolidColorVertex.hpp"

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

        /// Records one batch of textured quads for later native submission.
        void SubmitQuads(const std::vector<PsVitaQueuedQuad>& queuedQuads);

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

        /// Presents the current frame through the PS Vita display path.
        void PresentFrame();

        /// Gets the number of quads most recently recorded for the current frame.
        std::size_t GetSubmittedQuadCount() const;

    private:
        /// Lazily uploads one runtime texture into a native PS Vita texture allocation before the first draw that references it.
        void EnsureUploaded(PsVitaRuntimeTexture* runtimeTexture);

        /// Submits one queued quad through the GPU-backed textured-triangle path.
        void SubmitQuad(const PsVitaQueuedQuad& queuedQuad);

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

        /// Stores the runtime-compiled GXM shader state used by the first programmable solid-color mesh path.
        PsVitaGxmSolidColorProgram SolidColorProgram;
    };
}

#endif
