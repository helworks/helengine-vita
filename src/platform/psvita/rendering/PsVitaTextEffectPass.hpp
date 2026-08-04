#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

#include "byte4.hpp"
#include "float2.hpp"

namespace helengine::psvita::rendering {
    /// Stores one pixel-offset and color pair used to render a single text glyph effect pass.
    struct PsVitaTextEffectPass final {
        /// Stores the pixel offset applied to the glyph position for this pass.
        ::float2 Offset;

        /// Stores the glyph color applied by this pass.
        ::byte4 Color;
    };
}

#endif
