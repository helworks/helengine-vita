#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class RuntimeTexture;
class TextureAsset;

#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"

namespace helengine::psvita::rendering {
    class PsVitaRuntimeTexture;

    /// Builds and caches PS Vita runtime textures from authored texture assets.
    class PsVitaTextureCache {
    public:
        /// Builds one PS Vita runtime texture from the cooked texture asset, reusing a cached instance when possible.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data);

        /// Releases one PS Vita runtime texture and removes any matching cache entry.
        void ReleaseTexture(PsVitaRuntimeTexture* texture);

        /// Marks that one presented PS Vita frame completed so deferred GPU texture aging can advance at the next explicit flush call.
        void NotifyFramePresented();

        /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary.
        void FlushReleasedTextures();

    private:
        /// Creates one uncached PS Vita runtime texture from the authored texture asset.
        PsVitaRuntimeTexture* CreateTexture(TextureAsset* data) const;

        /// Stores one detached PS Vita GPU texture together with the released runtime texture wrapper that should be destroyed at the same boundary.
        struct ReleasedGpuTextureEntry {
            /// Stores the released PS Vita runtime texture wrapper until the renderer reaches the same safe destruction boundary as the detached native texture.
            PsVitaRuntimeTexture* RuntimeTexture = nullptr;

            /// Stores the detached native PS Vita GPU texture allocation awaiting final destruction.
            std::unique_ptr<PsVitaGpuTexture> GpuTexture;

            /// Stores how many explicit flush boundaries must elapse before the detached native texture can be destroyed safely.
            std::uint32_t FlushBoundariesRemaining = 3u;

        };

        /// Stores runtime textures cached by deterministic runtime asset id.
        std::unordered_map<std::uint64_t, PsVitaRuntimeTexture*> CachedTextures;

        /// Stores uploaded GPU textures detached from released runtime textures until the renderer reaches the next explicit frame-boundary flush.
        std::vector<ReleasedGpuTextureEntry> ReleasedGpuTextures;

        /// Stores whether one presented PS Vita frame completed since the last deferred-release flush attempt.
        bool PresentedFramePendingFlush = false;

    };
}

#endif
