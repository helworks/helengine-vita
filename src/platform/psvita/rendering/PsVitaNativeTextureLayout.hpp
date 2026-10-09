#pragma once

#include <cstddef>
#include <cstdint>

namespace helengine::psvita::rendering {
    /// Describes the validated single-level VGT1 upload without requiring GXM SDK headers.
    struct PsVitaNativeTextureLayout {
        /// Stores the complete SDK texture format, including its channel swizzle.
        std::uint32_t Format = 0;
        /// Stores the actual GXM texture type used to initialize the sampler.
        std::uint32_t Type = 0;
        /// Stores the visible dimensions of the authored texture.
        std::uint32_t Width = 0, Height = 0;
        /// Stores the padded physical dimensions represented by the native payload.
        std::uint32_t StorageWidth = 0, StorageHeight = 0;
        /// Stores the linear row pitch, or zero for non-linear texture types.
        std::uint32_t PitchBytes = 0;
        /// Stores the exact pixel payload size, excluding the VGT1 header.
        std::uint32_t TexelLength = 0;
        /// Stores the complete hardware palette entry count, or zero for direct formats.
        std::uint32_t PaletteEntries = 0;
        /// Stores the exact native ABGR32 palette byte count.
        std::uint32_t PaletteLength = 0;
        /// Stores the engine alpha policy serialized in the VGT1 header.
        std::uint32_t AlphaPrecision = 0;
    };

    /// Validates one complete VGT1 payload, including exact lengths, padding indices and reserved words.
    bool TryParseNativeTexturePayload(const std::uint8_t* colors, std::size_t colorLength,
        const std::uint8_t* palette, std::size_t paletteLength, PsVitaNativeTextureLayout& layout);

    /// Computes the canonical byte layout for one exact SDK format and supported single-level texture type.
    bool TryBuildNativeTextureLayout(std::uint32_t format, std::uint32_t type,
        std::uint32_t width, std::uint32_t height, std::uint32_t alphaPrecision,
        PsVitaNativeTextureLayout& layout);

    /// Gets whether the payload permits the generic RGBA8 region update without conversion or palette mutation.
    bool SupportsNativeRgbaRegionUpdate(const PsVitaNativeTextureLayout& layout);

    /// Selects point sampling for raw integer payloads without assuming normalized image-shader compatibility.
    bool RequiresNativeTexturePointSampling(const PsVitaNativeTextureLayout& layout);
}
