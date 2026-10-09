#pragma once

#include <cstddef>
#include <cstdint>
#include <psp2/gxm.h>

#include "platform/psvita/rendering/PsVitaNativeTextureLayout.hpp"

struct vita2d_texture;

namespace helengine::psvita::rendering {
    /// Binds a texture while preserving GXM descriptor and sampler errors instead of relying on vita2d's unchecked bind.
    void BindPsVitaFragmentTexture(SceGxmContext* context, std::uint32_t textureIndex, const SceGxmTexture* texture);

    /// Owns a vita2d-compatible descriptor backed by exact GXM-mapped native pixel and palette allocations.
    class PsVitaNativeTextureUpload final {
    public:
        /// Creates an empty allocation owner.
        PsVitaNativeTextureUpload() = default;
        /// Unmaps and frees the owned data after its caller has completed the GPU fence.
        ~PsVitaNativeTextureUpload();
        /// Prevents copies from acquiring duplicate ownership of kernel memory-block handles.
        PsVitaNativeTextureUpload(const PsVitaNativeTextureUpload&) = delete;
        /// Prevents assignment from acquiring duplicate ownership of native allocations.
        PsVitaNativeTextureUpload& operator=(const PsVitaNativeTextureUpload&) = delete;
        /// Creates the descriptor and copies exact validated native bytes; exceptions retain no partial allocations.
        void Initialize(const PsVitaNativeTextureLayout& layout, const std::uint8_t* pixels,
            std::size_t pixelLength, const std::uint8_t* palette, std::size_t paletteLength);
        /// Gets the descriptor accepted by vita2d draw calls and GXM 3D binding.
        vita2d_texture* GetTexture() const;
    private:
        /// Releases both mapped buffers and their descriptor without calling vita2d's allocator-specific destructor.
        void Reset();
        /// Stores the descriptor whose public GXM texture carries the actual native layout.
        vita2d_texture* Texture = nullptr;
        /// Stores whether the owned pixel memory block has a successful GXM mapping.
        bool PixelsMapped = false;
        /// Stores whether the owned palette memory block has a successful GXM mapping.
        bool PaletteMapped = false;
        /// Stores the pixel allocation address independently of descriptor initialization success.
        void* Pixels = nullptr;
        /// Stores the palette allocation address independently of descriptor initialization success.
        void* Palette = nullptr;
    };
}
