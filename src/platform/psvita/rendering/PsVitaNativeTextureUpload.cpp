#include "platform/psvita/rendering/PsVitaNativeTextureUpload.hpp"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <vita2d.h>

#include "platform/psvita/rendering/PsVitaGxmMemoryBlockSize.hpp"
#include "platform/psvita/rendering/PsVitaGxmInitialization.hpp"

namespace helengine::psvita::rendering {
    void BindPsVitaFragmentTexture(SceGxmContext* context, std::uint32_t textureIndex, const SceGxmTexture* texture) {
        const int result = sceGxmSetFragmentTexture(context, textureIndex, texture);
        if (result < 0) {
            throw std::runtime_error("PS Vita fragment texture binding failed. sceGxmSetFragmentTexture="
                + std::to_string(result));
        }
    }

    namespace {
        /// Allocates a page-aligned Vita user block; the caller retains its UID even when acquiring the base fails.
        void* AllocateTextureBlock(std::size_t bytes, SceUID& uid) {
            const std::size_t rounded = CalculatePsVitaGxmMemoryBlockSize(bytes,
                static_cast<std::size_t>(std::numeric_limits<SceSize>::max()));
            if (rounded == 0) {
                throw std::invalid_argument("PS Vita native texture allocation size is invalid.");
            }
            uid = sceKernelAllocMemBlock("HelengineNativeTexture", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                static_cast<SceSize>(rounded), nullptr);
            if (uid < 0) {
                throw std::runtime_error("PS Vita native texture memory allocation failed.");
            }
            void* address = nullptr;
            if (sceKernelGetMemBlockBase(uid, &address) < 0 || address == nullptr) {
                throw std::runtime_error("PS Vita native texture memory base acquisition failed.");
            }
            std::memset(address, 0, rounded);
            return address;
        }

        /// Maps one previously allocated block with the page-rounded extent required by GXM.
        void MapTextureBlock(void* address, std::size_t bytes) {
            const std::size_t rounded = CalculatePsVitaGxmMemoryBlockSize(bytes,
                static_cast<std::size_t>(std::numeric_limits<SceSize>::max()));
            if (sceGxmMapMemory(address, static_cast<SceSize>(rounded), SCE_GXM_MEMORY_ATTRIB_READ) < 0) {
                throw std::runtime_error("PS Vita native texture GXM mapping failed.");
            }
        }
    }

    PsVitaNativeTextureUpload::~PsVitaNativeTextureUpload() {
        Reset();
    }

    void PsVitaNativeTextureUpload::Initialize(const PsVitaNativeTextureLayout& layout,
        const std::uint8_t* pixels, std::size_t pixelLength,
        const std::uint8_t* palette, std::size_t paletteLength) {
        if (Texture != nullptr) {
            throw std::logic_error("PS Vita native texture upload cannot replace a live allocation.");
        }
        if (pixels == nullptr || pixelLength != layout.TexelLength
            || paletteLength != layout.PaletteLength || (paletteLength != 0 && palette == nullptr)) {
            throw std::invalid_argument("PS Vita native texture upload requires its exact validated payload.");
        }

        if ((layout.Format & 0xff000000u) == 0x84000000u && !ArePsVitaExtendedTextureFormatsEnabled()) {
            throw std::runtime_error("PS Vita ETC1 upload requires an explicitly initialized extended GXM context.");
        }

        Texture = static_cast<vita2d_texture*>(std::calloc(1, sizeof(vita2d_texture)));
        if (Texture == nullptr) {
            throw std::bad_alloc();
        }
        Texture->data_UID = -1;
        Texture->palette_UID = -1;
        try {
            Pixels = AllocateTextureBlock(pixelLength, Texture->data_UID);
            MapTextureBlock(Pixels, pixelLength);
            PixelsMapped = true;
            std::memcpy(Pixels, pixels, pixelLength);

            const auto format = static_cast<SceGxmTextureFormat>(layout.Format);
            int result = -1;
            switch (layout.Type) {
                case SCE_GXM_TEXTURE_LINEAR:
                    result = sceGxmTextureInitLinear(&Texture->gxm_tex, Pixels, format, layout.Width, layout.Height, 0);
                    break;
                case SCE_GXM_TEXTURE_SWIZZLED:
                    result = sceGxmTextureInitSwizzled(&Texture->gxm_tex, Pixels, format,
                        layout.StorageWidth, layout.StorageHeight, 0);
                    break;
                case SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY:
                    result = sceGxmTextureInitSwizzledArbitrary(&Texture->gxm_tex, Pixels, format,
                        layout.Width, layout.Height, 0);
                    break;
                case SCE_GXM_TEXTURE_TILED:
                    result = sceGxmTextureInitTiled(&Texture->gxm_tex, Pixels, format, layout.Width, layout.Height, 0);
                    break;
                default:
                    throw std::invalid_argument("PS Vita native texture type is unsupported.");
            }
            if (result < 0) {
                throw std::runtime_error("PS Vita GXM rejected the native texture descriptor.");
            }
            if (paletteLength != 0) {
                Palette = AllocateTextureBlock(paletteLength, Texture->palette_UID);
                MapTextureBlock(Palette, paletteLength);
                PaletteMapped = true;
                std::memcpy(Palette, palette, paletteLength);
                if (sceGxmTextureSetPalette(&Texture->gxm_tex, Palette) < 0) {
                    throw std::runtime_error("PS Vita GXM rejected the native texture palette.");
                }
            }
            const SceGxmTextureFilter filter = RequiresNativeTexturePointSampling(layout)
                ? SCE_GXM_TEXTURE_FILTER_POINT : SCE_GXM_TEXTURE_FILTER_LINEAR;
            if (sceGxmTextureSetMinFilter(&Texture->gxm_tex, filter) < 0
                || sceGxmTextureSetMagFilter(&Texture->gxm_tex, filter) < 0) {
                throw std::runtime_error("PS Vita GXM rejected the native texture filters.");
            }
        }
        catch (...) {
            Reset();
            throw;
        }
    }

    vita2d_texture* PsVitaNativeTextureUpload::GetTexture() const {
        return Texture;
    }

    void PsVitaNativeTextureUpload::Reset() {
        if (PaletteMapped) {
            sceGxmUnmapMemory(Palette);
        }
        if (PixelsMapped) {
            sceGxmUnmapMemory(Pixels);
        }
        if (Texture != nullptr) {
            if (Texture->palette_UID >= 0) {
                sceKernelFreeMemBlock(Texture->palette_UID);
            }
            if (Texture->data_UID >= 0) {
                sceKernelFreeMemBlock(Texture->data_UID);
            }
            std::free(Texture);
        }
        Texture = nullptr;
        Pixels = nullptr;
        Palette = nullptr;
        PixelsMapped = false;
        PaletteMapped = false;
    }
}
