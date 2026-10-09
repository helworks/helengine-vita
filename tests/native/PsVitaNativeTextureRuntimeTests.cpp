#include "platform/psvita/rendering/PsVitaNativeTextureUpload.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"
#include "platform/psvita/rendering/PsVitaTextureCache.hpp"
#include "platform/psvita/rendering/PsVitaGxmInitialization.hpp"
#include "TextureAsset.hpp"
#include <psp2/kernel/sysmem.h>
#include <vita2d.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace helengine::psvita::rendering;

/// Stores independent allocation spies so failures and completed retirement can be observed.
static std::map<int, std::vector<std::uint8_t>> Blocks;
static int NextUid = 1, MapCalls = 0, UnmapCalls = 0, FailMapAt = 0, WaitCalls = 0, PaletteCalls = 0;
static SceGxmInitializeParams CapturedInitialize {};
static const SceGxmInitializeParams* CapturedInitializePointer = nullptr;
static SceGxmContext Context;
static int ProfileCalls = 0, FailProfileIndex = -1;
static std::vector<unsigned> SceneEvents;
static bool SceneOpen = false;
/// Records explicit fragment binds, including failures that vita2d otherwise ignores.
static int BindCalls = 0, BindResult = 0;

int sceKernelAllocMemBlock(const char*, unsigned, SceSize size, const void*) {
    assert(size % 4096 == 0);
    int uid = NextUid++;
    Blocks.emplace(uid, std::vector<std::uint8_t>(size, 0));
    return uid;
}
int sceKernelGetMemBlockBase(int uid, void** base) { *base = Blocks.at(uid).data(); return 0; }
int sceKernelFreeMemBlock(int uid) { assert(Blocks.erase(uid) == 1); return 0; }
int sceGxmMapMemory(void*, SceSize size, unsigned) { assert(size % 4096 == 0); return ++MapCalls == FailMapAt ? -1 : 0; }
int sceGxmUnmapMemory(void*) { ++UnmapCalls; return 0; }
int sceGxmTextureInitLinear(SceGxmTexture* t, const void* p, unsigned f, unsigned w, unsigned h, unsigned) {
    t->Data = const_cast<void*>(p); t->Format = f; t->Width = w; t->Height = h; t->Type = SCE_GXM_TEXTURE_LINEAR; return 0;
}
int sceGxmTextureInitSwizzled(SceGxmTexture* t, const void* p, unsigned f, unsigned w, unsigned h, unsigned mip) {
    sceGxmTextureInitLinear(t,p,f,w,h,mip); t->Type = SCE_GXM_TEXTURE_SWIZZLED; return 0;
}
int sceGxmTextureInitSwizzledArbitrary(SceGxmTexture* t, const void* p, unsigned f, unsigned w, unsigned h, unsigned mip) {
    sceGxmTextureInitLinear(t,p,f,w,h,mip); t->Type = SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY; return 0;
}
int sceGxmTextureInitTiled(SceGxmTexture* t, const void* p, unsigned f, unsigned w, unsigned h, unsigned mip) {
    sceGxmTextureInitLinear(t,p,f,w,h,mip); t->Type = SCE_GXM_TEXTURE_TILED; return 0;
}
int sceGxmTextureSetPalette(SceGxmTexture* texture, const void* palette) { texture->Palette = palette; ++PaletteCalls; return 0; }
int sceGxmTextureSetMinFilter(SceGxmTexture* texture, unsigned filter) { texture->MinFilter = filter; return 0; }
int sceGxmTextureSetMagFilter(SceGxmTexture* texture, unsigned filter) { texture->MagFilter = filter; return 0; }
int sceGxmSetFragmentTexture(SceGxmContext* context, unsigned index, const SceGxmTexture* texture) {
    assert(context == &Context && index == 0 && texture != nullptr);
    ++BindCalls; return BindResult;
}
void vita2d_free_texture(vita2d_texture* texture) { delete texture; }
void* vita2d_texture_get_datap(const vita2d_texture* texture) { return texture->gxm_tex.Data; }
unsigned vita2d_texture_get_stride(const vita2d_texture* texture) { return ((texture->gxm_tex.Width + 7) & ~7u) * 4; }
void vita2d_wait_rendering_done() { assert(!SceneOpen); ++WaitCalls; SceneEvents.push_back(2); }
void vita2d_end_drawing() { assert(SceneOpen); SceneOpen = false; SceneEvents.push_back(1); }
void vita2d_start_drawing_advanced(vita2d_texture* target, unsigned flags) {
    assert(!SceneOpen && target == nullptr && flags == 0);
    SceneOpen = true; SceneEvents.push_back(3);
}
int sceGxmDisplayQueueFinish() { return 0; }
SceGxmContext* vita2d_get_context() { return &Context; }
int sceGxmSetYuvProfile(SceGxmContext* context, unsigned index, unsigned profile) {
    assert(context == &Context && profile == SCE_GXM_YUV_PROFILE_BT601_STANDARD);
    assert(index == static_cast<unsigned>(ProfileCalls++ % 2));
    return static_cast<int>(index) == FailProfileIndex ? -1 : 0;
}
extern "C" int __real_sceGxmInitialize(const SceGxmInitializeParams* params) {
    CapturedInitializePointer = params;
    if (params != nullptr) CapturedInitialize = *params;
    return 123;
}
extern "C" int __wrap_sceGxmInitialize(const SceGxmInitializeParams*);

/// Uploads a validated fixture into its real production owner and attaches it to the runtime texture.
static void UploadRuntime(PsVitaRuntimeTexture& texture) {
    const auto& layout = texture.GetNativeLayout();
    auto upload = std::make_unique<PsVitaNativeTextureUpload>();
    upload->Initialize(layout, texture.GetNativePixels().data(), texture.GetNativePixels().size(),
        texture.GetNativePalette().data(), texture.GetNativePalette().size());
    auto gpu = std::make_unique<PsVitaGpuTexture>();
    gpu->SetNativeUpload(std::move(upload), layout.Width, layout.Height);
    texture.SetGpuTexture(std::move(gpu));
}

/// Verifies actual production mapped uploads, palette ownership, region synchronization and retirement against SDK spies.
int main() {
    SceGxmInitializeParams original {0x200, 3, nullptr, 72, 4096};
    assert(__wrap_sceGxmInitialize(&original) == 123);
    assert(original.flags == 0x200 && CapturedInitialize.flags == 0x210);
    assert(CapturedInitialize.displayQueueMaxPendingCount == 3 && CapturedInitialize.displayQueueCallbackDataSize == 72);
    assert(CapturedInitialize.parameterBufferSize == 4096);
    assert(__wrap_sceGxmInitialize(nullptr) == 123 && CapturedInitializePointer == nullptr);
    assert(ConfigurePsVitaNativeYuvProfiles() && ProfileCalls == 2);
    FailProfileIndex = 1;
    assert(!ConfigurePsVitaNativeYuvProfiles() && ProfileCalls == 4);
    FailProfileIndex = -1;

    PsVitaNativeTextureLayout layout;
    assert(TryBuildNativeTextureLayout(0x95000000, 0x80000000, 3, 5, 3, layout));
    std::vector<std::uint8_t> pixels(layout.TexelLength, 7), palette(layout.PaletteLength, 191);
    {
        PsVitaNativeTextureUpload upload;
        upload.Initialize(layout, pixels.data(), pixels.size(), palette.data(), palette.size());
        const auto* native = upload.GetTexture();
        assert(native->gxm_tex.Type == SCE_GXM_TEXTURE_TILED && native->gxm_tex.Width == 3 && native->gxm_tex.Height == 5);
        assert(native->gxm_tex.Format == 0x95000000 && native->gxm_tex.Palette != nullptr && PaletteCalls == 1);
        assert(native->gxm_tex.MinFilter == SCE_GXM_TEXTURE_FILTER_LINEAR && native->gxm_tex.MagFilter == SCE_GXM_TEXTURE_FILTER_LINEAR);
        assert(std::memcmp(native->gxm_tex.Data, pixels.data(), pixels.size()) == 0);
        assert(std::memcmp(native->gxm_tex.Palette, palette.data(), palette.size()) == 0);
        pixels[0] = 88; palette[0] = 99;
        assert(static_cast<const std::uint8_t*>(native->gxm_tex.Data)[0] == 7);
        assert(static_cast<const std::uint8_t*>(native->gxm_tex.Palette)[0] == 191);
        assert(Blocks.size() == 2);
    }
    assert(Blocks.empty() && UnmapCalls == 2);
    FailMapAt = MapCalls + 2;
    try {
        PsVitaNativeTextureUpload upload;
        upload.Initialize(layout, pixels.data(), pixels.size(), palette.data(), palette.size());
        assert(false);
    } catch (const std::runtime_error&) {}
    assert(Blocks.empty() && UnmapCalls == 3);
    FailMapAt = 0;

    for (unsigned type : {0x60000000u, 0x80000000u, 0xa0000000u, 0u}) {
        assert(TryBuildNativeTextureLayout(0x0c000000, type, type == 0 ? 8 : 3, type == 0 ? 8 : 5, 3, layout));
        const std::vector<std::uint8_t> bytes(layout.TexelLength, 123);
        PsVitaNativeTextureUpload upload;
        upload.Initialize(layout, bytes.data(), bytes.size(), nullptr, 0);
        assert(upload.GetTexture()->gxm_tex.Type == type);
        assert(upload.GetTexture()->gxm_tex.Width == layout.Width && upload.GetTexture()->gxm_tex.Height == layout.Height);
        assert(std::memcmp(upload.GetTexture()->gxm_tex.Data, bytes.data(), bytes.size()) == 0);
    }
    assert(Blocks.empty());

    for (unsigned format : {0x17000000u, 0x18000000u, 0x1f000000u}) {
        assert(TryBuildNativeTextureLayout(format, 0x60000000u, 3, 5, 0, layout));
        assert(RequiresNativeTexturePointSampling(layout));
        const std::vector<std::uint8_t> bytes(layout.TexelLength, 73);
        PsVitaNativeTextureUpload upload;
        upload.Initialize(layout, bytes.data(), bytes.size(), nullptr, 0);
        const auto* native = upload.GetTexture();
        assert(native->gxm_tex.MinFilter == SCE_GXM_TEXTURE_FILTER_POINT && native->gxm_tex.MagFilter == SCE_GXM_TEXTURE_FILTER_POINT);
        BindPsVitaFragmentTexture(&Context, 0, &native->gxm_tex);
        BindResult = -91;
        try { BindPsVitaFragmentTexture(&Context, 0, &native->gxm_tex); assert(false); }
        catch (const std::runtime_error& error) { assert(std::strstr(error.what(), "-91") != nullptr); }
        BindResult = 0;
    }
    assert(Blocks.empty() && BindCalls == 6);

    assert(TryBuildNativeTextureLayout(0x0c000000, 0x60000000, 3, 5, 3, layout));
    {
        PsVitaRuntimeTexture texture;
        texture.set_Width(3); texture.set_Height(5);
        texture.SetNativePayload(layout, std::vector<std::uint8_t>(layout.TexelLength, 0), {});
        UploadRuntime(texture);
        const auto* uploaded = texture.GetGpuTexture()->GetNativeTexture();
        const auto* address = vita2d_texture_get_datap(uploaded);
        const int allocationCount = NextUid;
        const std::uint8_t rows[] = {1,2,3,4,5,6,7,8,91,92,93,94,9,10,11,12,13,14,15,16};
        const int waitsBefore = WaitCalls;
        SceneEvents.clear(); SceneOpen = true;
        texture.UpdateRgbaRegion(1, 2, 2, 2, rows, sizeof(rows), 12, true);
        assert(SceneOpen && SceneEvents == std::vector<unsigned>({1,2,3}));
        assert(WaitCalls == waitsBefore + 1 && NextUid == allocationCount && vita2d_texture_get_datap(uploaded) == address);
        assert(std::memcmp(texture.GetNativePixels().data() + 2 * 32 + 4, rows, 8) == 0);
        assert(std::memcmp(texture.GetNativePixels().data() + 3 * 32 + 4, rows + 12, 8) == 0);
        assert(std::memcmp(address, texture.GetNativePixels().data(), layout.TexelLength) == 0);
        const auto baseline = texture.GetNativePixels();
        const auto eventsBefore = SceneEvents;
        try { texture.UpdateRgbaRegion(2, 0, 2, 1, rows, sizeof(rows), 8, true); assert(false); }
        catch (const std::invalid_argument&) {}
        assert(WaitCalls == waitsBefore + 1 && texture.GetNativePixels() == baseline);
        assert(SceneOpen && SceneEvents == eventsBefore);
        SceneOpen = false;
    }
    assert(Blocks.empty());

    for (unsigned alpha : {0u, 1u, 2u}) {
        assert(TryBuildNativeTextureLayout(0x0c000000, 0x60000000, 1, 1, alpha, layout));
        PsVitaRuntimeTexture texture;
        texture.set_Width(1); texture.set_Height(1);
        texture.SetNativePayload(layout, std::vector<std::uint8_t>(layout.TexelLength, 0), {});
        const std::uint8_t rgba[] {2,4,6,128};
        texture.UpdateRgbaRegion(0,0,1,1,rgba,sizeof(rgba),4);
        assert(texture.GetNativePixels()[3] == (alpha == 2 ? 136 : 255));
        assert(rgba[3] == 128);
    }
    for (unsigned format : {0x95000000u, 0x85000000u}) {
        assert(TryBuildNativeTextureLayout(format, 0x60000000, 3, 5, format == 0x95000000 ? 3 : 1, layout));
        PsVitaRuntimeTexture texture;
        texture.set_Width(3); texture.set_Height(5);
        texture.SetNativePayload(layout, std::vector<std::uint8_t>(layout.TexelLength, 7),
            std::vector<std::uint8_t>(layout.PaletteLength, 99));
        UploadRuntime(texture);
        const auto baseline = texture.GetNativePixels();
        const int waitsBefore = WaitCalls;
        std::uint8_t row[4] {255,255,255,255};
        SceneOpen = true;
        const auto eventsBefore = SceneEvents;
        try { texture.UpdateRgbaRegion(0,0,1,1,row,sizeof(row),4,true); assert(false); }
        catch (const std::invalid_argument&) {}
        assert(WaitCalls == waitsBefore && texture.GetNativePixels() == baseline);
        assert(SceneOpen && SceneEvents == eventsBefore);
        SceneOpen = false;
    }
    assert(Blocks.empty());

    PsVitaTextureCache cache;
    TextureAsset asset;
    Array<std::uint8_t> rgba(4);
    rgba[0] = 1; rgba[1] = 2; rgba[2] = 3; rgba[3] = 73;
    asset.Width = 1; asset.Height = 1; asset.Colors = &rgba;
    auto* raw = static_cast<PsVitaRuntimeTexture*>(cache.BuildTextureFromRaw(&asset));
    assert(raw->GetPixelsAbgr8888()[0] == 0x49030201u);
    cache.ReleaseTexture(raw);
    cache.ReleaseTexture(raw);
    assert(cache.IsPendingRelease(raw));
    for (int frame = 0; frame < 3; ++frame) { cache.NotifyFramePresented(); cache.FlushReleasedTextures(); assert(cache.IsPendingRelease(raw)); }
    cache.NotifyFramePresented(); cache.FlushReleasedTextures();
    assert(!cache.IsPendingRelease(raw));

    const std::uint32_t words[] {0x31544756, 1, 0x0c000000, 0x60000000, 3, 5, 8, 5, 32, 160, 0, 0, 3, 0, 0, 0};
    Array<std::uint8_t> nativeColors(224);
    for (std::size_t word = 0; word < 16; ++word) {
        for (unsigned byte = 0; byte < 4; ++byte) nativeColors[static_cast<int>(word * 4 + byte)] = words[word] >> (byte * 8);
    }
    nativeColors[64] = 11;
    TextureAsset nativeAsset;
    nativeAsset.Width = 3; nativeAsset.Height = 5; nativeAsset.Colors = &nativeColors;
    nativeAsset.ColorFormat = TextureAssetColorFormat::VitaNative; nativeAsset.AlphaPrecision = 3;
    auto* native = static_cast<PsVitaRuntimeTexture*>(cache.BuildTextureFromRaw(&nativeAsset));
    assert(native->HasNativePayload() && native->GetNativePixels().size() == 160);
    nativeColors[64] = 22;
    assert(native->GetNativePixels()[0] == 11);
    UploadRuntime(*native);
    assert(Blocks.size() == 1);
    const int waitsBeforeRetirement = WaitCalls;
    cache.ReleaseTexture(native);
    assert(Blocks.size() == 1);
    for (int frame = 0; frame < 3; ++frame) { cache.NotifyFramePresented(); cache.FlushReleasedTextures(); assert(Blocks.size() == 1); }
    cache.NotifyFramePresented(); cache.FlushReleasedTextures();
    assert(Blocks.empty() && WaitCalls == waitsBeforeRetirement + 4);

    asset.RuntimeId = 55;
    auto* resident = static_cast<PsVitaRuntimeTexture*>(cache.BuildTextureFromRaw(&asset));
    assert(cache.BuildTextureFromRaw(&asset) == resident);
    cache.ReleaseTexture(resident);
    assert(!cache.IsPendingRelease(resident));
    delete resident;
    assert(Blocks.empty());
    std::puts("PS Vita production upload/update/lifetime/initialization spies passed.");
}
