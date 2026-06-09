#include "platform/psvita/rendering/PsVitaTextureCache.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdio>
#include <stdexcept>
#include <vector>

#include "TextureAsset.hpp"
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

        /// Returns the expected RGBA32 payload length for one authored texture asset.
        int GetExpectedColorLength(int pixelCount) {
            return pixelCount * 4;
        }
    }

    /// Builds one PS Vita runtime texture from the cooked texture asset, reusing a cached instance when possible.
    RuntimeTexture* PsVitaTextureCache::BuildTextureFromRaw(TextureAsset* data) {
        AppendTextureTrace("TextureCache: BuildTextureFromRaw begin");
        if (data == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture data is required.");
        }

        AppendTextureTrace("TextureCache: cache miss");
        PsVitaRuntimeTexture* runtimeTexture = CreateTexture(data);
        AppendTextureTrace("TextureCache: BuildTextureFromRaw success");
        return runtimeTexture;
    }

    /// Releases one PS Vita runtime texture and removes any matching cache entry.
    void PsVitaTextureCache::ReleaseTexture(PsVitaRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture release requires one texture instance.");
        }

        for (auto iterator = CachedTextures.begin(); iterator != CachedTextures.end(); ++iterator) {
            if (iterator->second == texture) {
                CachedTextures.erase(iterator);
                break;
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
        AppendTextureTrace("TextureCache: metadata width=" + std::to_string(width)
            + " height=" + std::to_string(height)
            + " colorsLength=" + std::to_string(data->Colors->Length));
        if (width <= 0 || height <= 0 || pixelCount <= 0) {
            throw std::invalid_argument("PS Vita runtime texture dimensions must be positive.");
        }

        int expectedColorLength = GetExpectedColorLength(pixelCount);
        AppendTextureTrace("TextureCache: expectedColorLength=" + std::to_string(expectedColorLength));
        if (data->Colors->Length != expectedColorLength) {
            throw std::invalid_argument("PS Vita runtime texture color payload length did not match the authored dimensions.");
        }

        std::vector<std::uint32_t> pixelsAbgr8888(static_cast<std::size_t>(pixelCount));
        AppendTextureTrace("TextureCache: decode Rgba32");
        for (int pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            int colorOffset = pixelIndex * 4;
            pixelsAbgr8888[static_cast<std::size_t>(pixelIndex)] = PackRgbaToAbgr(
                (*data->Colors)[colorOffset],
                (*data->Colors)[colorOffset + 1],
                (*data->Colors)[colorOffset + 2],
                (*data->Colors)[colorOffset + 3]);
        }

        PsVitaRuntimeTexture* runtimeTexture = new PsVitaRuntimeTexture();
        runtimeTexture->set_Id(data->get_Id());
        runtimeTexture->set_Width(width);
        runtimeTexture->set_Height(height);
        runtimeTexture->SetPixelsAbgr8888(std::move(pixelsAbgr8888));
        AppendTextureTrace("TextureCache: CreateTexture success");
        return runtimeTexture;
    }
}

#endif
