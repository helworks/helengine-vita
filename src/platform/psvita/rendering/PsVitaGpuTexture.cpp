#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <vita2d.h>

namespace helengine::psvita::rendering {
    /// Creates one empty PS Vita GPU texture record with no uploaded native allocation.
    PsVitaGpuTexture::PsVitaGpuTexture()
        : NativeTexture(nullptr)
        , Width(0u)
        , Height(0u)
        , Uploaded(false) {
    }

    /// Releases the owned native PS Vita texture allocation when one upload occurred.
    PsVitaGpuTexture::~PsVitaGpuTexture() {
        if (NativeTexture != nullptr) {
            vita2d_free_texture(NativeTexture);
            NativeTexture = nullptr;
        }
    }

    /// Gets the uploaded texture width in pixels.
    std::uint32_t PsVitaGpuTexture::GetWidth() const {
        return Width;
    }

    /// Gets the uploaded texture height in pixels.
    std::uint32_t PsVitaGpuTexture::GetHeight() const {
        return Height;
    }

    /// Gets whether the texture currently represents one uploaded native texture allocation.
    bool PsVitaGpuTexture::IsUploaded() const {
        return Uploaded;
    }

    /// Gets the owned native PS Vita texture allocation, or <c>nullptr</c> when the texture was not uploaded yet.
    vita2d_texture* PsVitaGpuTexture::GetNativeTexture() const {
        return NativeTexture;
    }

    /// Attaches one uploaded native PS Vita texture allocation to this runtime texture wrapper.
    void PsVitaGpuTexture::SetNativeTexture(vita2d_texture* nativeTexture, std::uint32_t width, std::uint32_t height) {
        if (NativeTexture != nullptr && NativeTexture != nativeTexture) {
            vita2d_free_texture(NativeTexture);
        }

        NativeTexture = nativeTexture;
        Width = width;
        Height = height;
        Uploaded = nativeTexture != nullptr;
    }
}

#endif
