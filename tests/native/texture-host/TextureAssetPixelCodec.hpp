#pragma once
#include <stdexcept>
#include "TextureAsset.hpp"
class TextureAssetPixelCodec {
public:
    static Array<std::uint8_t>* DecodeToRgba32(TextureAsset*) {
        throw std::invalid_argument("Only the independent legacy RGBA and native fixtures participate in this host test.");
    }
};
