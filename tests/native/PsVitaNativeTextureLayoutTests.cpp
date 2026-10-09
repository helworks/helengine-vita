#include "platform/psvita/rendering/PsVitaNativeTextureLayout.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace helengine::psvita::rendering;

/// Serializes literal VGT1 words for parser tests without depending on the production serializer.
static void WriteWord(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t word) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(word >> (8 * i));
}

/// Builds a literal three-by-five ABGR8888 scanline payload with a thirty-two-byte row pitch.
static std::vector<std::uint8_t> MakeAbgrPayload() {
    std::vector<std::uint8_t> bytes(64 + 160, 0);
    const std::uint32_t words[] = {0x31544756, 1, 0x0c000000, 0x60000000,
        3, 5, 8, 5, 32, 160, 0, 0, 3, 0, 0, 0};
    for (std::size_t i = 0; i < 16; ++i) WriteWord(bytes, i * 4, words[i]);
    bytes[64] = 255; bytes[67] = 128;
    return bytes;
}

/// Verifies literal native layout, exact packed extents, strict reserved codes and parser metadata checks.
int main() {
    PsVitaNativeTextureLayout layout;
    auto bytes = MakeAbgrPayload();
    assert(TryParseNativeTexturePayload(bytes.data(), bytes.size(), nullptr, 0, layout));
    assert(layout.Width == 3 && layout.Height == 5 && layout.StorageWidth == 8 && layout.StorageHeight == 5);
    assert(layout.PitchBytes == 32 && layout.TexelLength == 160 && SupportsNativeRgbaRegionUpdate(layout));
    assert(!TryParseNativeTexturePayload(bytes.data(), bytes.size() - 1, nullptr, 0, layout));
    for (std::size_t offset : {0u, 4u, 24u, 28u, 32u, 36u, 40u, 44u, 52u, 56u, 60u}) {
        auto malformed = bytes;
        malformed[offset] ^= 1;
        assert(!TryParseNativeTexturePayload(malformed.data(), malformed.size(), nullptr, 0, layout));
    }
    assert(!TryBuildNativeTextureLayout(0x16000000, 0x60000000, 8, 8, 0, layout));
    assert(!TryBuildNativeTextureLayout(0x03001000, 0x60000000, 8, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x0c000001, 0x60000000, 8, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x0c000000, 0x40000000, 8, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x0c000000, 0x60000000, 0, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x0c000000, 0x60000000, 4097, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x0c000000, 0x60000000, 8, 8, 5, layout));

    assert(TryBuildNativeTextureLayout(0x85000000, 0x60000000, 3, 5, 1, layout));
    assert(layout.StorageWidth == 8 && layout.StorageHeight == 8 && layout.PitchBytes == 16 && layout.TexelLength == 32);
    assert(TryBuildNativeTextureLayout(0x86000000, 0xa0000000, 9, 9, 2, layout));
    assert(layout.StorageWidth == 16 && layout.StorageHeight == 16 && layout.TexelLength == 256 && layout.PitchBytes == 0);
    assert(!TryBuildNativeTextureLayout(0x85000000, 0x80000000, 8, 8, 1, layout));
    assert(TryBuildNativeTextureLayout(0x80000000, 0xa0000000, 3, 5, 3, layout));
    assert(layout.StorageWidth == 16 && layout.StorageHeight == 8 && layout.TexelLength == 32);
    assert(TryBuildNativeTextureLayout(0x81000000, 0xa0000000, 3, 5, 3, layout));
    assert(layout.StorageWidth == 8 && layout.StorageHeight == 8 && layout.TexelLength == 32);
    assert(!TryBuildNativeTextureLayout(0x80000000, 0x60000000, 16, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x81000000, 0, 3, 5, 3, layout));
    assert(TryBuildNativeTextureLayout(0x90000000, 0x60000000, 3, 5, 0, layout));
    assert(layout.StorageWidth == 8 && layout.StorageHeight == 6 && layout.PitchBytes == 8 && layout.TexelLength == 72);
    assert(!TryBuildNativeTextureLayout(0x90000000, 0xa0000000, 3, 5, 0, layout));
    assert(TryBuildNativeTextureLayout(0x94000000, 0xa0000000, 1, 1, 3, layout));
    assert(layout.TexelLength == 1 && layout.PaletteEntries == 16 && layout.PaletteLength == 64);
    assert(TryBuildNativeTextureLayout(0x95000000, 0x80000000, 3, 5, 3, layout));
    assert(layout.StorageWidth == 32 && layout.StorageHeight == 32 && layout.TexelLength == 1024 && layout.PaletteLength == 1024);
    assert(!SupportsNativeRgbaRegionUpdate(layout));
    assert(TryBuildNativeTextureLayout(0x00003000, 0x60000000, 8, 8, 3, layout));
    assert(!TryBuildNativeTextureLayout(0x00003000, 0x60000000, 8, 8, 0, layout));
    assert(TryBuildNativeTextureLayout(0x17000000, 0x60000000, 8, 8, 0, layout));
    assert(!TryBuildNativeTextureLayout(0x17000000, 0x60000000, 8, 8, 3, layout));
    std::puts("PS Vita native layout/parser golden tests passed.");
}
