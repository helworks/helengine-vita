#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

namespace helengine::psvita::rendering {
    /// Stores one packed PS Vita 2D textured-quad vertex used by both sprite and text submission.
    struct PsVitaTexturedQuadVertex final {
        /// Stores the horizontal screen-space position.
        float PositionX;

        /// Stores the vertical screen-space position.
        float PositionY;

        /// Stores the horizontal texture coordinate.
        float TextureU;

        /// Stores the vertical texture coordinate.
        float TextureV;

        /// Stores the packed ABGR tint color.
        std::uint32_t ColorAbgr;
    };
}

#endif
