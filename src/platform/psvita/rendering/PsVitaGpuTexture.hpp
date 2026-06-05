#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

namespace helengine::psvita::rendering {
    /// Stores the future native PS Vita GPU texture ownership that will back one runtime texture inside the GXM 2D renderer.
    class PsVitaGpuTexture final {
    public:
        /// Creates one empty PS Vita GPU texture record with no uploaded native allocation.
        PsVitaGpuTexture();

        /// Gets the uploaded texture width in pixels.
        std::uint32_t GetWidth() const;

        /// Gets the uploaded texture height in pixels.
        std::uint32_t GetHeight() const;

        /// Gets whether the texture currently represents one uploaded native texture allocation.
        bool IsUploaded() const;

    private:
        /// Stores the uploaded texture width in pixels.
        std::uint32_t Width;

        /// Stores the uploaded texture height in pixels.
        std::uint32_t Height;

        /// Stores whether the texture has entered the uploaded state.
        bool Uploaded;
    };
}

#endif
