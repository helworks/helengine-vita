#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"
#include "RuntimeTexture.hpp"

namespace helengine::psvita::rendering {
    /// Stores PS Vita-ready ABGR8888 texture pixels that can later be uploaded into the native Vita renderer.
    class PsVitaRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates an empty PS Vita runtime texture.
        PsVitaRuntimeTexture();

        /// Gets the PS Vita-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
        const std::uint32_t* GetPixelsAbgr8888() const;

        /// Gets the number of stored ABGR8888 texels.
        std::size_t GetPixelCount() const;

        /// Gets whether the texture owns pixel data.
        bool HasPixels() const;

        /// Gets the authored texture width in pixels carried by this runtime texture.
        std::uint32_t GetTextureWidthPixels() const;

        /// Gets the authored texture height in pixels carried by this runtime texture.
        std::uint32_t GetTextureHeightPixels() const;

        /// Replaces the owned ABGR8888 pixel buffer.
        void SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels);

        /// Gets the deterministic runtime asset id associated with this texture.
        std::uint64_t GetRuntimeAssetId() const;

        /// Assigns the deterministic runtime asset id associated with this texture.
        void SetRuntimeAssetId(std::uint64_t runtimeAssetId);

        /// Gets the native PS Vita GPU texture currently attached to this runtime texture, or <c>nullptr</c> when no upload occurred yet.
        PsVitaGpuTexture* GetGpuTexture() const;

        /// Gets whether the runtime texture already owns one native PS Vita GPU texture.
        bool HasGpuTexture() const;

        /// Attaches one native PS Vita GPU texture allocation to this runtime texture.
        void SetGpuTexture(std::unique_ptr<PsVitaGpuTexture>&& gpuTexture);

        /// Releases and returns the owned native PS Vita GPU texture allocation.
        std::unique_ptr<PsVitaGpuTexture> ReleaseGpuTexture();

    private:
        /// Stores PS Vita-ready ABGR8888 texels in row-major order.
        std::vector<std::uint32_t> PixelsAbgr8888;

        /// Stores the deterministic runtime asset id for cache reuse.
        std::uint64_t RuntimeAssetId = 0u;

        /// Stores the uploaded native PS Vita GPU texture once one GXM upload occurs.
        std::unique_ptr<PsVitaGpuTexture> GpuTexture;
    };
}

#endif
