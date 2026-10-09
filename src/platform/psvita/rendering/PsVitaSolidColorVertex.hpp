#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

namespace helengine::psvita::rendering {
    /// Stores one PS Vita solid-color triangle vertex with screen-space position, packed color, and composite render order.
    struct PsVitaSolidColorVertex final {
        float PositionX = 0.0f;
        float PositionY = 0.0f;
        std::uint32_t ColorAbgr = 0u;
        /// Preserves the camera and request ordinal when solid and textured command streams are merged.
        std::uint64_t RenderOrder = 0u;
    };
}

#endif
