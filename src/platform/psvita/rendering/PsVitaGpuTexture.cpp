#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one empty PS Vita GPU texture record with no uploaded native allocation.
    PsVitaGpuTexture::PsVitaGpuTexture()
        : Width(0u)
        , Height(0u)
        , Uploaded(false) {
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
}

#endif
