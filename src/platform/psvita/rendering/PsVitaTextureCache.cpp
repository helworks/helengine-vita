#include "platform/psvita/rendering/PsVitaTextureCache.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdio>
#include <stdexcept>
#include <vector>

#include <vita2d.h>

#include "TextureAsset.hpp"
#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

namespace helengine::psvita::rendering {
    namespace {
        /// Limits how many matured native Vita textures are destroyed during one frame-boundary flush so each released texture ages across additional presented frames.
        constexpr std::size_t MaxReleasedGpuTextureFreesPerFlush = 1u;
    }

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
        if (data == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture data is required.");
        }

        const std::uint64_t cacheKey = data->get_RuntimeAssetId();
        AppendTextureTrace("TextureCache: BuildTextureFromRaw begin id="
            + data->get_Id()
            + " runtimeAssetId="
            + std::to_string(cacheKey)
            + " width="
            + std::to_string(data->Width)
            + " height="
            + std::to_string(data->Height));
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end()) {
                AppendTextureTrace("TextureCache: cache hit runtimeTexture="
                    + std::to_string(reinterpret_cast<std::uintptr_t>(cachedTextureIterator->second))
                    + " runtimeAssetId="
                    + std::to_string(cacheKey));
                return cachedTextureIterator->second;
            }
        }

        AppendTextureTrace("TextureCache: cache miss");
        PsVitaRuntimeTexture* runtimeTexture = CreateTexture(data);
        if (cacheKey != 0u) {
            CachedTextures.emplace(cacheKey, runtimeTexture);
        }
        AppendTextureTrace("TextureCache: BuildTextureFromRaw success runtimeTexture="
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " id="
            + runtimeTexture->get_Id());
        return runtimeTexture;
    }

    /// Releases one PS Vita runtime texture and removes any matching cache entry.
    void PsVitaTextureCache::ReleaseTexture(PsVitaRuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PS Vita runtime texture release requires one texture instance.");
        }

        const std::uint64_t cacheKey = texture->GetRuntimeAssetId();
        if (cacheKey != 0u) {
            auto cachedTextureIterator = CachedTextures.find(cacheKey);
            if (cachedTextureIterator != CachedTextures.end() && cachedTextureIterator->second == texture) {
                AppendTextureTrace("TextureCache: ReleaseTexture retained cached runtimeTexture="
                    + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
                    + " id="
                    + texture->get_Id()
                    + " runtimeAssetId="
                    + std::to_string(cacheKey));
                return;
            }
        }

        std::unique_ptr<PsVitaGpuTexture> releasedGpuTexture = texture->ReleaseGpuTexture();
        AppendTextureTrace("TextureCache: ReleaseTexture runtimeTexture="
            + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
            + " id="
            + texture->get_Id()
            + " runtimeAssetId="
            + std::to_string(cacheKey)
            + " width="
            + std::to_string(texture->get_Width())
            + " height="
            + std::to_string(texture->get_Height())
            + " hasGpuTexture="
            + std::string(releasedGpuTexture != nullptr ? "true" : "false"));
        if (releasedGpuTexture != nullptr) {
            AppendTextureTrace("TextureCache: queued released gpu texture runtimeTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(texture))
                + " gpuTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedGpuTexture.get()))
                + " nativeTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedGpuTexture->GetNativeTexture()))
                + " id="
                + texture->get_Id());
        }

        ReleasedGpuTextureEntry releasedEntry{};
        releasedEntry.RuntimeTexture = texture;
        releasedEntry.GpuTexture = std::move(releasedGpuTexture);
        ReleasedGpuTextures.push_back(std::move(releasedEntry));
    }

    /// Marks that one presented PS Vita frame completed so the next explicit flush can safely age deferred native texture releases.
    void PsVitaTextureCache::NotifyFramePresented() {
        PresentedFramePendingFlush = true;
    }

    /// Finalizes deferred PS Vita runtime texture deletions once the renderer reaches the explicit frame-boundary safe point.
    void PsVitaTextureCache::FlushReleasedTextures() {
        if (ReleasedGpuTextures.empty()) {
            return;
        }
        if (!PresentedFramePendingFlush) {
            AppendTextureTrace("TextureCache: FlushReleasedTextures skipped pendingPresentedFrame=false");
            return;
        }

        PresentedFramePendingFlush = false;

        AppendTextureTrace("TextureCache: FlushReleasedTextures begin count=" + std::to_string(ReleasedGpuTextures.size()));
        AppendTextureTrace("TextureCache: FlushReleasedTextures before wait");
        vita2d_wait_rendering_done();
        AppendTextureTrace("TextureCache: FlushReleasedTextures after wait");
        AppendTextureTrace("TextureCache: FlushReleasedTextures before display queue finish");
        sceGxmDisplayQueueFinish();
        AppendTextureTrace("TextureCache: FlushReleasedTextures after display queue finish");

        std::vector<ReleasedGpuTextureEntry> releasedTextures = std::move(ReleasedGpuTextures);
        std::vector<ReleasedGpuTextureEntry> readyReleasedTextures;
        ReleasedGpuTextures.clear();
        for (std::size_t releasedTextureIndex = 0; releasedTextureIndex < releasedTextures.size(); releasedTextureIndex++) {
            ReleasedGpuTextureEntry& releasedEntry = releasedTextures[releasedTextureIndex];
            if (releasedEntry.FlushBoundariesRemaining > 0u) {
                releasedEntry.FlushBoundariesRemaining--;
                AppendTextureTrace("TextureCache: FlushReleasedTextures defer runtimeTexture="
                    + std::to_string(reinterpret_cast<std::uintptr_t>(releasedEntry.RuntimeTexture))
                    + " remainingBoundaries="
                    + std::to_string(releasedEntry.FlushBoundariesRemaining));
                ReleasedGpuTextures.push_back(std::move(releasedEntry));
                continue;
            }

            if (readyReleasedTextures.size() >= MaxReleasedGpuTextureFreesPerFlush) {
                AppendTextureTrace("TextureCache: FlushReleasedTextures defer matured runtimeTexture="
                    + std::to_string(reinterpret_cast<std::uintptr_t>(releasedEntry.RuntimeTexture))
                    + " dueToPerFlushLimit=true");
                ReleasedGpuTextures.push_back(std::move(releasedEntry));
                continue;
            }

            readyReleasedTextures.push_back(std::move(releasedEntry));
        }

        if (readyReleasedTextures.empty()) {
            AppendTextureTrace("TextureCache: FlushReleasedTextures no entries ready");
            return;
        }

        for (std::size_t releasedTextureIndex = 0; releasedTextureIndex < readyReleasedTextures.size(); releasedTextureIndex++) {
            ReleasedGpuTextureEntry& releasedEntry = readyReleasedTextures[releasedTextureIndex];
            std::unique_ptr<PsVitaGpuTexture> releasedGpuTexture = std::move(releasedEntry.GpuTexture);
            AppendTextureTrace("TextureCache: FlushReleasedTextures free gpuTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedGpuTexture.get()))
                + " nativeTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedGpuTexture != nullptr ? releasedGpuTexture->GetNativeTexture() : nullptr))
                + " runtimeTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedEntry.RuntimeTexture))
                + " remaining="
                + std::to_string(readyReleasedTextures.size() - releasedTextureIndex - 1u));
            AppendTextureTrace("TextureCache: FlushReleasedTextures before gpu reset");
            releasedGpuTexture.reset();
            AppendTextureTrace("TextureCache: FlushReleasedTextures after gpu reset");
        }

        AppendTextureTrace("TextureCache: FlushReleasedTextures gpu phase complete");
        for (std::size_t releasedTextureIndex = 0; releasedTextureIndex < readyReleasedTextures.size(); releasedTextureIndex++) {
            ReleasedGpuTextureEntry& releasedEntry = readyReleasedTextures[releasedTextureIndex];
            if (releasedEntry.RuntimeTexture == nullptr) {
                continue;
            }

            AppendTextureTrace("TextureCache: FlushReleasedTextures before runtime dispose runtimeTexture="
                + std::to_string(reinterpret_cast<std::uintptr_t>(releasedEntry.RuntimeTexture))
                + " remaining="
                + std::to_string(readyReleasedTextures.size() - releasedTextureIndex - 1u));
            releasedEntry.RuntimeTexture->Dispose();
            AppendTextureTrace("TextureCache: FlushReleasedTextures after runtime dispose");
            AppendTextureTrace("TextureCache: FlushReleasedTextures before runtime delete");
            delete releasedEntry.RuntimeTexture;
            releasedEntry.RuntimeTexture = nullptr;
            AppendTextureTrace("TextureCache: FlushReleasedTextures after runtime delete");
            AppendTextureTrace("TextureCache: FlushReleasedTextures freed one");
        }

        AppendTextureTrace("TextureCache: FlushReleasedTextures complete");
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
        runtimeTexture->SetRuntimeAssetId(data->get_RuntimeAssetId());
        runtimeTexture->set_Width(width);
        runtimeTexture->set_Height(height);
        runtimeTexture->SetPixelsAbgr8888(std::move(pixelsAbgr8888));
        AppendTextureTrace("TextureCache: CreateTexture success runtimeTexture="
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " id="
            + runtimeTexture->get_Id());
        return runtimeTexture;
    }
}

#endif
