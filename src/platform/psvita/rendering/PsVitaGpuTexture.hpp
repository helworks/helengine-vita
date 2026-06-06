#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

struct vita2d_texture;

namespace helengine::psvita::rendering {
    /// Stores one native PS Vita GPU texture allocation that backs one runtime texture inside the GPU-backed 2D renderer.
    class PsVitaGpuTexture final {
    public:
        /// Creates one empty PS Vita GPU texture record with no uploaded native allocation.
        PsVitaGpuTexture();

        /// Releases the owned native PS Vita texture allocation when one upload occurred.
        ~PsVitaGpuTexture();

        /// Gets the uploaded texture width in pixels.
        std::uint32_t GetWidth() const;

        /// Gets the uploaded texture height in pixels.
        std::uint32_t GetHeight() const;

        /// Gets whether the texture currently represents one uploaded native texture allocation.
        bool IsUploaded() const;

        /// Gets the owned native PS Vita texture allocation, or <c>nullptr</c> when the texture was not uploaded yet.
        vita2d_texture* GetNativeTexture() const;

        /// Attaches one uploaded native PS Vita texture allocation to this runtime texture wrapper.
        void SetNativeTexture(vita2d_texture* nativeTexture, std::uint32_t width, std::uint32_t height);

    private:
        /// Stores the owned native PS Vita texture allocation.
        vita2d_texture* NativeTexture;

        /// Stores the uploaded texture width in pixels.
        std::uint32_t Width;

        /// Stores the uploaded texture height in pixels.
        std::uint32_t Height;

        /// Stores whether the texture has entered the uploaded state.
        bool Uploaded;
    };
}

#endif
