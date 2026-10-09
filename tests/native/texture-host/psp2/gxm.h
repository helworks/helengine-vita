#pragma once
#include <cstddef>
#include <cstdint>
using SceSize = std::uint32_t;
using SceUID = int;
using SceGxmTextureFormat = std::uint32_t;
using SceGxmTextureFilter = std::uint32_t;
using SceGxmRenderTarget = void;
struct SceGxmColorSurface {};
struct SceGxmDepthStencilSurface {};
struct SceGxmContext {};
constexpr unsigned SCE_GXM_YUV_PROFILE_BT601_STANDARD = 0;
int sceGxmSetYuvProfile(SceGxmContext*, unsigned, unsigned);
struct SceGxmTexture {
    void* Data = nullptr;
    const void* Palette = nullptr;
    unsigned Width = 0, Height = 0, Type = 0, Format = 0;
    unsigned MinFilter = 0, MagFilter = 0;
};
struct SceGxmInitializeParams {
    unsigned flags;
    unsigned displayQueueMaxPendingCount;
    void (*displayQueueCallback)(const void*);
    unsigned displayQueueCallbackDataSize;
    unsigned parameterBufferSize;
};
constexpr unsigned SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT = 0x10;
constexpr unsigned SCE_GXM_TEXTURE_LINEAR = 0x60000000;
constexpr unsigned SCE_GXM_TEXTURE_SWIZZLED = 0;
constexpr unsigned SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY = 0xa0000000;
constexpr unsigned SCE_GXM_TEXTURE_TILED = 0x80000000;
constexpr unsigned SCE_GXM_TEXTURE_FILTER_LINEAR = 1;
constexpr unsigned SCE_GXM_TEXTURE_FILTER_POINT = 0;
constexpr unsigned SCE_GXM_MEMORY_ATTRIB_READ = 1;
int sceGxmMapMemory(void*, SceSize, unsigned);
int sceGxmUnmapMemory(void*);
int sceGxmTextureInitLinear(SceGxmTexture*, const void*, SceGxmTextureFormat, unsigned, unsigned, unsigned);
int sceGxmTextureInitSwizzled(SceGxmTexture*, const void*, SceGxmTextureFormat, unsigned, unsigned, unsigned);
int sceGxmTextureInitSwizzledArbitrary(SceGxmTexture*, const void*, SceGxmTextureFormat, unsigned, unsigned, unsigned);
int sceGxmTextureInitTiled(SceGxmTexture*, const void*, SceGxmTextureFormat, unsigned, unsigned, unsigned);
int sceGxmTextureSetPalette(SceGxmTexture*, const void*);
int sceGxmTextureSetMinFilter(SceGxmTexture*, unsigned);
int sceGxmTextureSetMagFilter(SceGxmTexture*, unsigned);
int sceGxmSetFragmentTexture(SceGxmContext*, unsigned, const SceGxmTexture*);
int sceGxmDisplayQueueFinish();
