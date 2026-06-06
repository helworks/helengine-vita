#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates an empty PS Vita runtime texture.
    PsVitaRuntimeTexture::PsVitaRuntimeTexture() = default;

    /// Gets the PS Vita-ready ABGR8888 texel pointer, or <c>nullptr</c> when the texture has no pixels.
    const std::uint32_t* PsVitaRuntimeTexture::GetPixelsAbgr8888() const {
        return PixelsAbgr8888.empty()
            ? nullptr
            : PixelsAbgr8888.data();
    }

    /// Gets the number of stored ABGR8888 texels.
    std::size_t PsVitaRuntimeTexture::GetPixelCount() const {
        return PixelsAbgr8888.size();
    }

    /// Gets whether the texture owns pixel data.
    bool PsVitaRuntimeTexture::HasPixels() const {
        return !PixelsAbgr8888.empty();
    }

    /// Gets the authored texture width in pixels carried by this runtime texture.
    std::uint32_t PsVitaRuntimeTexture::GetTextureWidthPixels() const {
        return static_cast<std::uint32_t>(const_cast<PsVitaRuntimeTexture*>(this)->get_Width());
    }

    /// Gets the authored texture height in pixels carried by this runtime texture.
    std::uint32_t PsVitaRuntimeTexture::GetTextureHeightPixels() const {
        return static_cast<std::uint32_t>(const_cast<PsVitaRuntimeTexture*>(this)->get_Height());
    }

    /// Replaces the owned ABGR8888 pixel buffer.
    void PsVitaRuntimeTexture::SetPixelsAbgr8888(std::vector<std::uint32_t>&& pixels) {
        PixelsAbgr8888 = std::move(pixels);
    }

    /// Gets the deterministic runtime asset id associated with this texture.
    std::uint64_t PsVitaRuntimeTexture::GetRuntimeAssetId() const {
        return RuntimeAssetId;
    }

    /// Assigns the deterministic runtime asset id associated with this texture.
    void PsVitaRuntimeTexture::SetRuntimeAssetId(std::uint64_t runtimeAssetId) {
        RuntimeAssetId = runtimeAssetId;
    }

    /// Gets the native PS Vita GPU texture currently attached to this runtime texture, or <c>nullptr</c> when no upload occurred yet.
    PsVitaGpuTexture* PsVitaRuntimeTexture::GetGpuTexture() const {
        return GpuTexture.get();
    }

    /// Gets whether the runtime texture already owns one native PS Vita GPU texture.
    bool PsVitaRuntimeTexture::HasGpuTexture() const {
        return GpuTexture != nullptr;
    }

    /// Attaches one native PS Vita GPU texture allocation to this runtime texture.
    void PsVitaRuntimeTexture::SetGpuTexture(std::unique_ptr<PsVitaGpuTexture>&& gpuTexture) {
        GpuTexture = std::move(gpuTexture);
    }

    /// Releases and returns the owned native PS Vita GPU texture allocation.
    std::unique_ptr<PsVitaGpuTexture> PsVitaRuntimeTexture::ReleaseGpuTexture() {
        return std::move(GpuTexture);
    }
}

#endif
