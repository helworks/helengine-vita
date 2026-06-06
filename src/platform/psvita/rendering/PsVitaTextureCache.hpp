#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <unordered_map>

class RuntimeTexture;
class TextureAsset;

namespace helengine::psvita::rendering {
    class PsVitaRuntimeTexture;

    /// Builds and caches PS Vita runtime textures from authored texture assets.
    class PsVitaTextureCache {
    public:
        /// Builds one PS Vita runtime texture from the cooked texture asset, reusing a cached instance when possible.
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data);

        /// Releases one PS Vita runtime texture and removes any matching cache entry.
        void ReleaseTexture(PsVitaRuntimeTexture* texture);

        /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary.
        void FlushReleasedTextures();

    private:
        /// Creates one uncached PS Vita runtime texture from the authored texture asset.
        PsVitaRuntimeTexture* CreateTexture(TextureAsset* data) const;

        /// Stores runtime textures cached by deterministic runtime asset id.
        std::unordered_map<std::uint64_t, PsVitaRuntimeTexture*> CachedTextures;
    };
}

#endif
