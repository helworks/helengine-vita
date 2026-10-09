#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "TextureAssetColorFormat.hpp"
template<typename T> class Array {
public:
    explicit Array(int length): Storage(length), Data(Storage.data()), Length(length) {}
    T& operator[](int index) { return Data[index]; }
private:
    std::vector<T> Storage;
public:
    T* Data;
    int Length;
};
class TextureAsset {
public:
    std::uint32_t Width = 0, Height = 0;
    Array<std::uint8_t>* Colors = nullptr;
    Array<std::uint8_t>* PaletteColors = nullptr;
    TextureAssetColorFormat ColorFormat = TextureAssetColorFormat::Rgba32;
    unsigned AlphaPrecision = 0;
    std::uint64_t RuntimeId = 0;
    std::string get_Id() { return "host-texture"; }
    std::uint64_t get_RuntimeAssetId() { return RuntimeId; }
};
