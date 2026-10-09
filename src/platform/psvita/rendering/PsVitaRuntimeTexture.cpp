#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstring>
#include <stdexcept>
#include <vita2d.h>

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

    void PsVitaRuntimeTexture::SetNativePayload(const PsVitaNativeTextureLayout& layout,
        std::vector<std::uint8_t>&& pixels, std::vector<std::uint8_t>&& palette) {
        if (HasGpuTexture() || pixels.size() != layout.TexelLength || palette.size() != layout.PaletteLength) {
            throw std::invalid_argument("PS Vita native payload requires an unuploaded texture and exact validated lengths.");
        }
        NativeLayout = layout;
        NativePixels = std::move(pixels);
        NativePalette = std::move(palette);
    }

    bool PsVitaRuntimeTexture::HasNativePayload() const {
        return !NativePixels.empty();
    }

    const PsVitaNativeTextureLayout& PsVitaRuntimeTexture::GetNativeLayout() const {
        return NativeLayout;
    }

    const std::vector<std::uint8_t>& PsVitaRuntimeTexture::GetNativePixels() const {
        return NativePixels;
    }

    const std::vector<std::uint8_t>& PsVitaRuntimeTexture::GetNativePalette() const {
        return NativePalette;
    }

    void PsVitaRuntimeTexture::UpdateRgbaRegion(int x, int y, int width, int height,
        const std::uint8_t* rgba, std::size_t sourceLength, int sourceRowPitch, bool mainSceneOpen) {
        const std::uint32_t textureWidth = GetTextureWidthPixels();
        const std::uint32_t textureHeight = GetTextureHeightPixels();
        if (x < 0 || y < 0 || width <= 0 || height <= 0 || width > 4096 || height > 4096
            || rgba == nullptr || sourceRowPitch < width * 4
            || static_cast<std::uint64_t>(x) + static_cast<std::uint32_t>(width) > textureWidth
            || static_cast<std::uint64_t>(y) + static_cast<std::uint32_t>(height) > textureHeight
            || static_cast<std::uint64_t>(height - 1) * static_cast<std::uint32_t>(sourceRowPitch)
                + static_cast<std::uint64_t>(width) * 4u > sourceLength) {
            throw std::invalid_argument("PS Vita texture region dimensions, stride or source extent are invalid.");
        }
        if (HasNativePayload() && !SupportsNativeRgbaRegionUpdate(NativeLayout)) {
            throw std::invalid_argument("PS Vita packed, indexed and compressed native textures are immutable to RGBA region updates.");
        }
        if (!HasNativePayload() && PixelsAbgr8888.size() != static_cast<std::size_t>(textureWidth) * textureHeight) {
            throw std::invalid_argument("PS Vita texture region requires retained CPU pixels.");
        }

        void* gpuPixels = nullptr;
        std::size_t gpuPitch = 0;
        if (HasGpuTexture()) {
            gpuPixels = vita2d_texture_get_datap(GpuTexture->GetNativeTexture());
            gpuPitch = HasNativePayload() ? NativeLayout.PitchBytes
                : vita2d_texture_get_stride(GpuTexture->GetNativeTexture());
            if (gpuPixels == nullptr || gpuPitch < static_cast<std::size_t>(textureWidth) * 4u) {
                throw std::runtime_error("PS Vita texture region requires a valid uploaded pixel allocation.");
            }
            // Finish alone does not submit commands from an open scene. Submit
            // earlier draws first; later resume the same buffer without resetting
            // the transient vertex pool, clearing, or presenting an extra frame.
            if (mainSceneOpen) {
                vita2d_end_drawing();
            }
            vita2d_wait_rendering_done();
        }
        for (int row = 0; row < height; ++row) {
            const auto* source = rgba + static_cast<std::size_t>(row) * sourceRowPitch;
            const std::size_t destinationY = static_cast<std::size_t>(y + row);
            if (HasNativePayload()) {
                auto* destination = NativePixels.data() + destinationY * NativeLayout.PitchBytes
                    + static_cast<std::size_t>(x) * 4u;
                std::memcpy(destination, source, static_cast<std::size_t>(width) * 4u);
                for (int column = 0; column < width; ++column) {
                    const auto sourceAlpha = source[static_cast<std::size_t>(column) * 4u + 3u];
                    destination[static_cast<std::size_t>(column) * 4u + 3u] = NativeLayout.AlphaPrecision == 0 ? 255u
                        : NativeLayout.AlphaPrecision == 1 ? sourceAlpha >= 128u ? 255u : 0u
                        : NativeLayout.AlphaPrecision == 2 ? static_cast<std::uint8_t>(((sourceAlpha * 15u + 127u) / 255u) * 17u)
                        : sourceAlpha;
                }
                source = destination;
            }
            else {
                auto* destination = reinterpret_cast<std::uint8_t*>(PixelsAbgr8888.data()
                    + destinationY * textureWidth + static_cast<std::size_t>(x));
                std::memcpy(destination, source, static_cast<std::size_t>(width) * 4u);
            }
            if (gpuPixels != nullptr) {
                auto* destination = static_cast<std::uint8_t*>(gpuPixels) + destinationY * gpuPitch
                    + static_cast<std::size_t>(x) * 4u;
                std::memcpy(destination, source, static_cast<std::size_t>(width) * 4u);
            }
        }
        if (gpuPixels != nullptr && mainSceneOpen) {
            vita2d_start_drawing_advanced(nullptr, 0);
        }
    }
}

#endif
