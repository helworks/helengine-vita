#include "platform/psvita/rendering/PsVitaTextureCache.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdio>
#include <stdexcept>
#include <vector>

#include "TextureAsset.hpp"
#include "TextureAssetColorFormat.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

namespace helengine::psvita::rendering {
    namespace {
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";

        /// Appends one local PS Vita texture-cache diagnostics line to the persisted boot trace.
        void AppendTextureTrace(const std::string& message) {
            std::FILE* file = std::fopen(BootTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::fputs(message.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }

        /// Packs one raw RGBA texel into the ABGR8888 layout expected by the PS Vita software bootstrap path.
        std::uint32_t PackRgbaToAbgr(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
            return (static_cast<std::uint32_t>(alpha) << 24)
                | (static_cast<std::uint32_t>(blue) << 16)
                | (static_cast<std::uint32_t>(green) << 8)
                | static_cast<std::uint32_t>(red);
        }

        /// Expands one packed 4-bit color channel into the 8-bit range used by the PS Vita bootstrap path.
        std::uint8_t ExpandFourBitChannel(std::uint16_t packedColor, int shift) {
            std::uint8_t channel = static_cast<std::uint8_t>((packedColor >> shift) & 15);
            return static_cast<std::uint8_t>((channel << 4) | channel);
        }

        /// Returns the expected color payload length for one authored texture asset.
        int GetExpectedColorLength(TextureAssetColorFormat colorFormat, int pixelCount) {
            switch (colorFormat) {
            case TextureAssetColorFormat::Rgba32:
                return pixelCount * 4;
            case TextureAssetColorFormat::Rgba4444:
                return pixelCount * 2;
            case TextureAssetColorFormat::Indexed4:
                return (pixelCount + 1) / 2;
            case TextureAssetColorFormat::Indexed8:
                return pixelCount;
            default:
                throw std::invalid_argument("PS Vita texture conversion received one unsupported texture color format.");
            }
        }

        /// Returns one ABGR8888 texel decoded from the indexed texture payload.
        std::uint32_t ReadPaletteColor(Array<std::uint8_t>* paletteColors, int paletteIndex) {
            if (paletteColors == nullptr || paletteColors->Data == nullptr) {
                throw std::invalid_argument("PS Vita indexed texture conversion requires palette colors.");
            }

            int paletteOffset = paletteIndex * 4;
            if ((paletteOffset + 3) >= paletteColors->Length) {
                throw std::invalid_argument("PS Vita indexed texture conversion received one palette index outside the authored palette bounds.");
            }

            return PackRgbaToAbgr(
                (*paletteColors)[paletteOffset],
                (*paletteColors)[paletteOffset + 1],
                (*paletteColors)[paletteOffset + 2],
                (*paletteColors)[paletteOffset + 3]);
        }
    }

    /// Builds one PS Vita runtime texture from the cooked texture asset, reusing a cached instance when possible.
    RuntimeTexture* PsVitaTextureCache::BuildTextureFromRaw(TextureAsset* data) {
        AppendTextureTrace("TextureCache: BuildTextureFromRaw begin");
        if (data == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture data is required.");
        }

        AppendTextureTrace("TextureCache: runtimeAssetId=" + std::to_string(data->get_RuntimeAssetId()));
        std::uint64_t cacheKey = data->get_RuntimeAssetId();
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end()) {
                AppendTextureTrace("TextureCache: cache hit");
                return cachedTextureIterator->second;
            }
        }

        AppendTextureTrace("TextureCache: cache miss");
        PsVitaRuntimeTexture* runtimeTexture = CreateTexture(data);
        if (cacheKey != 0u) {
            CachedTextures.emplace(cacheKey, runtimeTexture);
        }

        AppendTextureTrace("TextureCache: BuildTextureFromRaw success");
        return runtimeTexture;
    }

    /// Releases one PS Vita runtime texture and removes any matching cache entry.
    void PsVitaTextureCache::ReleaseTexture(PsVitaRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture release requires one texture instance.");
        }

        std::uint64_t cacheKey = texture->GetRuntimeAssetId();
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end() && cachedTextureIterator->second == texture) {
                CachedTextures.erase(cachedTextureIterator);
            }
        }
    }

    /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary.
    void PsVitaTextureCache::FlushReleasedTextures() {
    }

    /// Creates one uncached PS Vita runtime texture from the authored texture asset.
    PsVitaRuntimeTexture* PsVitaTextureCache::CreateTexture(TextureAsset* data) const {
        AppendTextureTrace("TextureCache: CreateTexture begin");
        if (data->Colors == nullptr || data->Colors->Data == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture colors are required.");
        }

        int width = static_cast<int>(data->Width);
        int height = static_cast<int>(data->Height);
        int pixelCount = width * height;
        AppendTextureTrace("TextureCache: metadata format=" + std::to_string(static_cast<int>(data->ColorFormat))
            + " width=" + std::to_string(width)
            + " height=" + std::to_string(height)
            + " colorsLength=" + std::to_string(data->Colors->Length)
            + " paletteLength=" + std::to_string(data->PaletteColors == nullptr ? -1 : data->PaletteColors->Length));
        if (width <= 0 || height <= 0 || pixelCount <= 0) {
            throw std::invalid_argument("PS Vita runtime texture dimensions must be positive.");
        }

        int expectedColorLength = GetExpectedColorLength(data->ColorFormat, pixelCount);
        AppendTextureTrace("TextureCache: expectedColorLength=" + std::to_string(expectedColorLength));
        if (data->Colors->Length != expectedColorLength) {
            throw std::invalid_argument("PS Vita runtime texture color payload length did not match the authored dimensions.");
        }

        std::vector<std::uint32_t> pixelsAbgr8888(static_cast<std::size_t>(pixelCount));
        switch (data->ColorFormat) {
        case TextureAssetColorFormat::Rgba32:
            AppendTextureTrace("TextureCache: decode Rgba32");
            for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
                int colorOffset = pixelIndex * 4;
                pixelsAbgr8888[static_cast<std::size_t>(pixelIndex)] = PackRgbaToAbgr(
                    (*data->Colors)[colorOffset],
                    (*data->Colors)[colorOffset + 1],
                    (*data->Colors)[colorOffset + 2],
                    (*data->Colors)[colorOffset + 3]);
            }
            break;
        case TextureAssetColorFormat::Rgba4444:
            AppendTextureTrace("TextureCache: decode Rgba4444");
            for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
                int colorOffset = pixelIndex * 2;
                std::uint16_t packedColor = static_cast<std::uint16_t>((*data->Colors)[colorOffset])
                    | (static_cast<std::uint16_t>((*data->Colors)[colorOffset + 1]) << 8);
                pixelsAbgr8888[static_cast<std::size_t>(pixelIndex)] = PackRgbaToAbgr(
                    ExpandFourBitChannel(packedColor, 0),
                    ExpandFourBitChannel(packedColor, 4),
                    ExpandFourBitChannel(packedColor, 8),
                    ExpandFourBitChannel(packedColor, 12));
            }
            break;
        case TextureAssetColorFormat::Indexed4:
            AppendTextureTrace("TextureCache: decode Indexed4");
            for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
                int colorOffset = pixelIndex / 2;
                std::uint8_t packedIndices = (*data->Colors)[colorOffset];
                int paletteIndex = (pixelIndex & 1) == 0
                    ? (packedIndices & 15)
                    : ((packedIndices >> 4) & 15);
                pixelsAbgr8888[static_cast<std::size_t>(pixelIndex)] = ReadPaletteColor(data->PaletteColors, paletteIndex);
            }
            break;
        case TextureAssetColorFormat::Indexed8:
            AppendTextureTrace("TextureCache: decode Indexed8");
            for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
                pixelsAbgr8888[static_cast<std::size_t>(pixelIndex)] = ReadPaletteColor(data->PaletteColors, (*data->Colors)[pixelIndex]);
            }
            break;
        default:
            throw std::invalid_argument("PS Vita runtime texture conversion received one unsupported texture color format.");
        }

        PsVitaRuntimeTexture* runtimeTexture = new PsVitaRuntimeTexture();
        runtimeTexture->set_Id(data->get_Id());
        runtimeTexture->set_Width(width);
        runtimeTexture->set_Height(height);
        runtimeTexture->set_IsEngineOwned(data->IsEngineOwned);
        runtimeTexture->SetRuntimeAssetId(data->get_RuntimeAssetId());
        runtimeTexture->SetPixelsAbgr8888(std::move(pixelsAbgr8888));
        AppendTextureTrace("TextureCache: CreateTexture success");
        return runtimeTexture;
    }
}

#endif
