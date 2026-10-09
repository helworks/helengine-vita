#include "platform/psvita/rendering/PsVitaNativeTextureLayout.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    /// Reads an exact cooker output; palette files may intentionally be empty.
    std::vector<std::uint8_t> ReadFile(const std::string& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Missing interop fixture: " + path);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    /// Reads a portable little-endian metadata word independently of the runtime parser.
    std::uint32_t Read32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
        if (offset + 4u > bytes.size()) throw std::runtime_error("Truncated fixture metadata");
        return static_cast<std::uint32_t>(bytes[offset])
            | (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u)
            | (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u)
            | (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
    }

    /// Writes one malformed descriptor word without relying on managed codec helpers.
    void Write32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
        for (std::size_t byte = 0u; byte < 4u; byte++) bytes.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8u));
    }

    /// Fails the executable when a meaningful cross-language condition is violated.
    void Require(bool value, const std::string& message) {
        if (!value) throw std::runtime_error(message);
    }

    /// Checks a real cooker/import output and independent malformed variants against the production native parser.
    void CheckFixture(const std::string& prefix) {
        using namespace helengine::psvita::rendering;
        const auto colors = ReadFile(prefix + ".colors");
        const auto palette = ReadFile(prefix + ".palette");
        const auto metadata = ReadFile(prefix + ".meta");
        Require(metadata.size() == 20u, "Unexpected independent source metadata length");
        Require(Read32(metadata, 16u) == 19u, "Wrong Vita asset tag or conflicting platform tag");
        PsVitaNativeTextureLayout layout{};
        Require(TryParseNativeTexturePayload(colors.data(), colors.size(), palette.data(), palette.size(), layout), "Native parser rejected " + prefix);
        Require(layout.Width == Read32(metadata, 0u) && layout.Height == Read32(metadata, 4u), "Cook changed logical dimensions");
        Require(layout.Format == Read32(metadata, 8u), "Cook changed the requested SDK format code");
        Require(layout.AlphaPrecision == Read32(metadata, 12u), "Cook changed the requested alpha contract");
        Require(colors.size() == 64u + layout.TexelLength && palette.size() == layout.PaletteLength, "Native payload storage disagrees with descriptor");
        Require(Read32(colors, 0u) == 0x31544756u && Read32(colors, 4u) == 1u, "Wrong VGT1 descriptor framing");
        Require(layout.StorageWidth >= layout.Width && layout.StorageHeight >= layout.Height, "Native storage cropped logical pixels");
        for (std::size_t reserved : {52u, 56u, 60u}) {
            auto malformed = colors;
            Write32(malformed, reserved, 1u);
            Require(!TryParseNativeTexturePayload(malformed.data(), malformed.size(), palette.data(), palette.size(), layout), "Native parser accepted reserved VGT1 bits");
        }
        Require(!TryParseNativeTexturePayload(colors.data(), colors.size() - 1u, palette.data(), palette.size(), layout), "Native parser accepted truncated texels");
        auto malformed = colors;
        Write32(malformed, 8u, 0xFF000000u);
        Require(!TryParseNativeTexturePayload(malformed.data(), malformed.size(), palette.data(), palette.size(), layout), "Native parser accepted an unknown SDK format");
        Write32(malformed, 8u, Read32(colors, 8u));
        Write32(malformed, 12u, 0x40000000u);
        Require(!TryParseNativeTexturePayload(malformed.data(), malformed.size(), palette.data(), palette.size(), layout), "Native parser accepted unsupported layout");
        if (!palette.empty()) {
            Require(!TryParseNativeTexturePayload(colors.data(), colors.size(), palette.data(), palette.size() - 1u, layout), "Native parser accepted truncated palette");
        } else {
            const std::uint8_t unexpectedPalette[4] = {0u, 0u, 0u, 0u};
            Require(!TryParseNativeTexturePayload(colors.data(), colors.size(), unexpectedPalette, 4u, layout), "Native parser accepted a palette for a direct format");
        }
    }
}

/// Runs against every real cooked/imported format fixture in a fixed artifact directory.
int main(int argc, char** argv) {
    try {
        Require(argc == 3, "Expected fixture root and fixture count");
        const int count = std::stoi(argv[2]);
        Require(count >= 600, "Incomplete Vita format coverage");
        for (int index = 0; index < count; index++) {
            char name[16]{};
            std::snprintf(name, sizeof(name), "%04d", index);
            CheckFixture(std::string(argv[1]) + "/" + name);
        }
        std::cout << count << " actual managed/native Vita fixtures passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
