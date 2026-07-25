#pragma once

#include <cstddef>

#include "float3.hpp"
#include "float2.hpp"

namespace helengine::psvita::rendering {
    /// Stores one interleaved position, normal, and optional UV vertex consumed by the forward GXM programs.
    struct PsVitaForwardLambertVertex final {
        /// Stores the model-space position at vertex stream offset zero.
        ::float3 Position;

        /// Stores the model-space normal immediately after the position.
        ::float3 Normal;

        /// Stores texture coordinates consumed by the textured Forward Standard Shader profile.
        ::float2 TexCoord;

        /// Stores the position attribute byte offset used by GXM patching.
        static constexpr unsigned short PositionOffset = 0u;

        /// Stores the normal attribute byte offset used by GXM patching.
        static constexpr unsigned short NormalOffset = static_cast<unsigned short>(sizeof(::float3));

        /// Stores the texture-coordinate attribute byte offset used by textured GXM patching.
        static constexpr unsigned short TexCoordOffset = static_cast<unsigned short>(sizeof(::float3) * 2u);

        /// Stores the interleaved stream stride used by GXM patching.
        static constexpr unsigned short StreamStride = static_cast<unsigned short>(sizeof(::float3) * 2u + sizeof(::float2));
    };
}
