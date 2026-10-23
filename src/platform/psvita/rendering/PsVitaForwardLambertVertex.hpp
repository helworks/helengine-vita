#pragma once

#include <cstddef>

namespace helengine::psvita::rendering {
    /// Stores one interleaved position, normal, and optional UV vertex consumed by the forward GXM programs.
    struct PsVitaForwardLambertVertex final {
        /// Stores the model-space position at vertex stream offset zero.
        float Position[3];

        /// Stores the model-space normal immediately after the position.
        float Normal[3];

        /// Stores texture coordinates consumed by the textured Forward Standard Shader profile.
        float TexCoord[2];

        /// Stores the position attribute byte offset used by GXM patching.
        static constexpr unsigned short PositionOffset = 0u;

        /// Stores the normal attribute byte offset used by GXM patching.
        static constexpr unsigned short NormalOffset = static_cast<unsigned short>(sizeof(float) * 3u);

        /// Stores the texture-coordinate attribute byte offset used by textured GXM patching.
        static constexpr unsigned short TexCoordOffset = static_cast<unsigned short>(sizeof(float) * 6u);

        /// Stores the interleaved stream stride used by GXM patching.
        static constexpr unsigned short StreamStride = static_cast<unsigned short>(sizeof(float) * 8u);
    };

    static_assert(sizeof(float) == 4u);
    static_assert(offsetof(PsVitaForwardLambertVertex, Normal) == 12u);
    static_assert(offsetof(PsVitaForwardLambertVertex, TexCoord) == 24u);
    static_assert(sizeof(PsVitaForwardLambertVertex) == 32u);
}
