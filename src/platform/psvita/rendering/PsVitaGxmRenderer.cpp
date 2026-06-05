#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"

#include <memory>

#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one uninitialized PS Vita GXM renderer foundation.
    PsVitaGxmRenderer::PsVitaGxmRenderer()
        : Initialized(false)
        , ActiveClearColorAbgr(0u)
        , SubmittedQuadCount(0u) {
    }

    /// Initializes the native renderer state needed before the first submitted 2D frame.
    bool PsVitaGxmRenderer::Initialize() {
        Initialized = true;
        SubmittedQuadCount = 0u;
        return true;
    }

    /// Shuts down the native renderer state and releases any owned frame resources.
    void PsVitaGxmRenderer::Shutdown() {
        Initialized = false;
        SubmittedQuadCount = 0u;
        ActiveClearColorAbgr = 0u;
    }

    /// Gets whether the renderer has completed its initialization path.
    bool PsVitaGxmRenderer::IsInitialized() const {
        return Initialized;
    }

    /// Begins one new frame and records the requested clear color for later native submission.
    void PsVitaGxmRenderer::BeginFrame(std::uint32_t clearColorAbgr) {
        ActiveClearColorAbgr = clearColorAbgr;
        SubmittedQuadCount = 0u;
    }

    /// Records one batch of textured quads for later native submission.
    void PsVitaGxmRenderer::SubmitQuads(const std::vector<PsVitaQueuedQuad>& queuedQuads) {
        for (const PsVitaQueuedQuad& queuedQuad : queuedQuads) {
            if (queuedQuad.Texture != nullptr && !queuedQuad.Texture->HasGpuTexture()) {
                std::unique_ptr<PsVitaGpuTexture> uploadedTexture = std::make_unique<PsVitaGpuTexture>();
                queuedQuad.Texture->SetGpuTexture(std::move(uploadedTexture));
            }
        }

        SubmittedQuadCount = queuedQuads.size();
    }

    /// Presents the current frame through the PS Vita display path.
    void PsVitaGxmRenderer::PresentFrame() {
    }

    /// Gets the number of quads most recently recorded for the current frame.
    std::size_t PsVitaGxmRenderer::GetSubmittedQuadCount() const {
        return SubmittedQuadCount;
    }
}

#endif
