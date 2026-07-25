#pragma once

#include <cstdint>

namespace helengine::psvita::shaders {
    /// Stores the fixed magic prefix used by serialized PS Vita shader artifacts.
    inline constexpr std::uint8_t PsVitaShaderArtifactMagic[4] = { 'P', 'V', 'S', 'A' };

    /// Stores the current serialized PS Vita shader artifact format version.
    inline constexpr std::uint32_t PsVitaShaderArtifactVersion = 1u;
}
