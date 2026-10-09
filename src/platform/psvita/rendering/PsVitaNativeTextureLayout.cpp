#include "platform/psvita/rendering/PsVitaNativeTextureLayout.hpp"

#include <algorithm>
#include <limits>

namespace helengine::psvita::rendering {
    namespace {
        /// Reads an unaligned little-endian VGT1 word independently of the host byte order.
        std::uint32_t ReadWord(const std::uint8_t* bytes) {
            return bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16)
                | (std::uint32_t(bytes[3]) << 24);
        }

        /// Checks exact SDK swizzle families; no reserved bits or unlisted base codes are admitted.
        bool IsSdkFormat(std::uint32_t format) {
            if ((format & 0x00ff8fffu) != 0) return false;
            const std::uint32_t code = format >> 24;
            const std::uint32_t swizzle = (format >> 12) & 7u;
            switch (code) {
                case 3: case 0x84: return swizzle == 0;
                case 5: case 6: case 0x14: case 0x15: case 0x19: case 0x1a:
                case 0x98: case 0x99: return swizzle <= 1;
                case 7: case 8: case 0x0f: case 0x10: case 0x11: case 0x1e: case 0x1f:
                case 0x8a: case 0x8b: return swizzle <= 5;
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x85: case 0x86: case 0x87:
                    return swizzle == 0 || swizzle == 4;
                case 0x90: case 0x91: return swizzle <= 3;
                case 0: case 1: case 2: case 4: case 9: case 10: case 11: case 12: case 13: case 14:
                case 0x12: case 0x13: case 0x17: case 0x18: case 0x1b: case 0x1c: case 0x1d:
                case 0x88: case 0x89: case 0x92: case 0x94: case 0x95: case 0x9a: return true;
                default: return false;
            }
        }

        /// Returns the physical bit rate declared by the pinned GXM SDK format enum.
        std::uint32_t BitsPerPixel(std::uint32_t code) {
            if (code == 0 || code == 1 || code == 0x95) return 8;
            if ((code >= 2 && code <= 11) || code == 0x92) return 16;
            if ((code >= 12 && code <= 21) || (code >= 23 && code <= 26) || code == 0x9a) return 32;
            if (code >= 27 && code <= 31) return 64;
            if (code == 0x80 || code == 0x82) return 2;
            if (code == 0x81 || code == 0x83 || code == 0x84 || code == 0x85
                || code == 0x88 || code == 0x89 || code == 0x94) return 4;
            if (code == 0x86 || code == 0x87 || code == 0x8a || code == 0x8b) return 8;
            if (code == 0x90 || code == 0x91) return 12;
            return 24;
        }

        /// Returns the number of physical components consumed by the SDK channel-swizzle family.
        std::uint32_t ComponentCount(std::uint32_t code) {
            if (code == 2 || code == 3 || code == 4 || code == 12 || code == 13 || code == 14
                || code == 27 || code == 28 || code == 29 || (code >= 0x80 && code <= 0x87)
                || code == 0x94 || code == 0x95 || code == 0x9a) return 4;
            if (code == 5 || code == 6 || code == 25 || code == 26 || code == 0x98 || code == 0x99) return 3;
            if (code == 7 || code == 8 || code == 15 || code == 16 || code == 17 || code == 21
                || code == 30 || code == 31 || code == 0x8a || code == 0x8b) return 2;
            return 1;
        }

        /// Resolves RGBA sampler channels to physical components, with four denoting zero and five denoting one.
        std::uint32_t SwizzleComponent(std::uint32_t code, std::uint32_t swizzle, std::uint32_t channel) {
            if ((code >= 0x90 && code <= 0x92) || code == 0x84) return channel == 3 ? 5 : channel;
            const auto count = ComponentCount(code);
            if (count == 4) {
                if (swizzle == 0 || swizzle == 4) return channel == 3 && swizzle == 4 ? 5 : channel;
                if (swizzle == 1 || swizzle == 5) return channel == 3 ? (swizzle == 5 ? 5 : 3) : 2 - channel;
                if (swizzle == 2 || swizzle == 6) return channel == 3 && swizzle == 6 ? 5 : 3 - channel;
                return channel == 3 && swizzle == 7 ? 5 : channel == 0 ? 1 : channel == 1 ? 2 : channel == 2 ? 3 : 0;
            }
            if (count == 3) return channel == 3 ? 5 : swizzle == 0 ? channel : 2 - channel;
            if (count == 2) {
                if (swizzle == 0 || swizzle == 1) return channel < 2 ? channel : channel == 2 || swizzle == 1 ? 4 : 5;
                if (swizzle == 2) return channel == 3 ? 1 : 0;
                if (swizzle == 3) return channel == 3 ? 0 : 1;
                if (swizzle == 4) return channel % 2;
                return channel == 0 ? 1 : channel == 1 ? 0 : 4;
            }
            if (swizzle == 0) return channel == 0 ? 0 : channel == 3 ? 5 : 4;
            if (swizzle == 1) return channel == 0 ? 0 : 4;
            if (swizzle == 2) return channel == 0 ? 0 : 5;
            if (swizzle == 3) return 0;
            if (swizzle == 4 || swizzle == 5) return channel == 3 ? swizzle == 4 ? 4 : 5 : 0;
            return channel == 3 ? 0 : swizzle == 6 ? 4 : 5;
        }

        /// Validates intrinsic and coupled alpha policies without silently normalizing metadata.
        bool SupportsAlpha(std::uint32_t format, std::uint32_t alpha) {
            const auto code = format >> 24;
            const auto swizzle = (format >> 12) & 7u;
            const auto component = SwizzleComponent(code, swizzle, 3);
            const bool rawOnly = code == 0x17 || code == 0x18 || code == 0x1f || code == 0x14 || code == 0x15;
            const std::uint32_t bits = component >= 4 || rawOnly ? 0 : code == 4 || code == 0x85 ? 1
                : code == 2 || code == 0x86 ? 4 : code == 14 || code == 0x9a ? 2 : 8;
            const std::uint32_t maximum = bits == 0 ? 0 : bits == 1 ? 1 : bits == 2 ? 4 : bits == 4 ? 2 : 3;
            const bool coupled = component < 4 && (SwizzleComponent(code, swizzle, 0) == component
                || SwizzleComponent(code, swizzle, 1) == component || SwizzleComponent(code, swizzle, 2) == component);
            if (coupled) return alpha == maximum;
            if (bits == 2) return alpha == 0 || alpha == 1 || alpha == 4;
            return alpha <= maximum;
        }

        /// Rounds a bounded positive dimension to its next power of two.
        std::uint32_t PowerOfTwo(std::uint32_t value) {
            std::uint32_t result = 1;
            while (result < value) result <<= 1;
            return result;
        }
    }

    bool TryBuildNativeTextureLayout(std::uint32_t format, std::uint32_t type,
        std::uint32_t width, std::uint32_t height, std::uint32_t alpha, PsVitaNativeTextureLayout& layout) {
        if (!IsSdkFormat(format) || !SupportsAlpha(format, alpha) || width == 0 || height == 0
            || width > 4096 || height > 4096) return false;
        const auto code = format >> 24;
        const auto bits = BitsPerPixel(code);
        const bool pvrtc = code >= 0x80 && code <= 0x83;
        const bool compressed = code >= 0x80 && code <= 0x8b;
        const bool yuv = code >= 0x90 && code <= 0x92;
        const auto blockHeight = compressed ? 4u : 1u;
        const auto blockWidth = pvrtc ? bits == 2 ? 8u : 4u : compressed ? 4u : 1u;
        if (type != 0u && type != 0x60000000u && type != 0x80000000u && type != 0xa0000000u) return false;
        if ((pvrtc && type != 0u && type != 0xa0000000u) || (yuv && type != 0x60000000u)
            || (compressed && type == 0x80000000u)) return false;

        PsVitaNativeTextureLayout value;
        value.Format = format; value.Type = type; value.Width = width; value.Height = height;
        if (type == 0x60000000u) {
            value.StorageWidth = (width + 7u) & ~7u;
            value.StorageHeight = (height + blockHeight - 1u) / blockHeight * blockHeight;
            if (code == 0x90 || code == 0x91) value.StorageHeight = (height + 1u) & ~1u;
        }
        else if (type == 0x80000000u) {
            value.StorageWidth = (width + 31u) & ~31u;
            value.StorageHeight = (height + 31u) & ~31u;
        }
        else {
            value.StorageWidth = PowerOfTwo(std::max(width, pvrtc ? bits == 2 ? 16u : 8u : compressed ? 4u : 1u));
            value.StorageHeight = PowerOfTwo(std::max(height, pvrtc ? 8u : blockHeight));
            if (type == 0u && (value.StorageWidth != width || value.StorageHeight != height)) return false;
        }
        const std::uint64_t byteLength = (std::uint64_t(value.StorageWidth) * value.StorageHeight * bits + 7u) / 8u;
        if (byteLength == 0 || byteLength > std::numeric_limits<std::uint32_t>::max()) return false;
        value.TexelLength = static_cast<std::uint32_t>(byteLength);
        if (type == 0x60000000u) {
            value.PitchBytes = compressed ? value.StorageWidth / blockWidth * (blockWidth * blockHeight * bits / 8u)
                : code == 0x90 || code == 0x91 ? value.StorageWidth : value.StorageWidth * bits / 8u;
        }
        value.PaletteEntries = code == 0x94 ? 16u : code == 0x95 ? 256u : 0u;
        value.PaletteLength = value.PaletteEntries * 4u;
        value.AlphaPrecision = alpha;
        layout = value;
        return true;
    }

    bool TryParseNativeTexturePayload(const std::uint8_t* colors, std::size_t colorLength,
        const std::uint8_t* palette, std::size_t paletteLength, PsVitaNativeTextureLayout& layout) {
        if (colors == nullptr || colorLength < 64 || ReadWord(colors) != 0x31544756u || ReadWord(colors + 4) != 1
            || ReadWord(colors + 52) != 0 || ReadWord(colors + 56) != 0 || ReadWord(colors + 60) != 0) return false;
        PsVitaNativeTextureLayout value;
        if (!TryBuildNativeTextureLayout(ReadWord(colors + 8), ReadWord(colors + 12),
            ReadWord(colors + 16), ReadWord(colors + 20), ReadWord(colors + 48), value)) return false;
        if (ReadWord(colors + 24) != value.StorageWidth || ReadWord(colors + 28) != value.StorageHeight
            || ReadWord(colors + 32) != value.PitchBytes || ReadWord(colors + 36) != value.TexelLength
            || ReadWord(colors + 40) != value.PaletteEntries || ReadWord(colors + 44) != value.PaletteLength
            || colorLength != 64u + static_cast<std::size_t>(value.TexelLength)
            || paletteLength != value.PaletteLength || (paletteLength != 0 && palette == nullptr)) return false;
        // Full P4/P8 palettes represent every possible stored index, including
        // padding texels; exact capacities prevent any out-of-range lookup.
        layout = value;
        return true;
    }

    bool SupportsNativeRgbaRegionUpdate(const PsVitaNativeTextureLayout& layout) {
        return layout.Format == 0x0c000000u && layout.Type == 0x60000000u;
    }

    bool RequiresNativeTexturePointSampling(const PsVitaNativeTextureLayout& layout) {
        const std::uint32_t baseFormat = layout.Format >> 24;
        // A conservative sampler policy for integer data; it does not make
        // integer payloads compatible with the fixed normalized RGBA shaders.
        return baseFormat == 0x17u || baseFormat == 0x18u || baseFormat == 0x1fu;
    }
}
