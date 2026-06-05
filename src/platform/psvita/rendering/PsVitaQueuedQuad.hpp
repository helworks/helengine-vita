#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

#include "platform/psvita/rendering/PsVitaTexturedQuadVertex.hpp"

namespace helengine::psvita::rendering {
    class PsVitaRuntimeTexture;

    /// Stores one queued 2D quad command that can later be sorted and submitted through the PS Vita GXM renderer.
    struct PsVitaQueuedQuad final {
        /// Stores the PS Vita runtime texture used by this quad.
        PsVitaRuntimeTexture* Texture;

        /// Stores the 2D render-order bucket carried by the originating drawable.
        std::uint8_t RenderOrder;

        /// Stores the four textured vertices that describe the quad.
        PsVitaTexturedQuadVertex Vertices[4];
    };
}

#endif
