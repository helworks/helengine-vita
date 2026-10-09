# PS Vita native texture formats

The catalog covers all 312 named selections in the installed GXM SDK, including
aliases: 278 distinct hardware words over 51 physical base formats.
The native texture contract uses the GXM format code, including its component
swizzle, rather than converting every texture to RGBA32. Image and font-atlas
defaults remain `Rgba32/A8` with their existing maximum resolutions of 512
and 0 respectively.

## Serialized contract

`TextureAssetColorFormat.VitaNative` (19) stores a VGT1 descriptor followed by
GPU texels in `Colors`. `PaletteColors` contains the native palette when the
format is indexed. Descriptor words, component words and palette entries are
little endian. The descriptor has sixteen uint32 words:

| Offset | Value |
| --- | --- |
| 0 | Magic `0x31544756` (`VGT1`) |
| 4 | Version 1 |
| 8 | Full `SceGxmTextureFormat` |
| 12 | GXM texture layout type |
| 16, 20 | Logical width and height |
| 24, 28 | Storage width and height |
| 32 | Row pitch in bytes, zero for non-linear storage |
| 36 | Texel payload length, excluding this descriptor |
| 40, 44 | Palette entry count and byte length |
| 48 | Texture asset alpha precision |
| 52, 56, 60 | Reserved, zero |

The shared Core catalog distinguishes RGBA image cooking, native byte import
and preview support. Formats whose integer or specialized sampling semantics
cannot be represented by an RGBA image retain their native data through the
raw import API. Unsupported image cooking or preview requests fail explicitly.
There are 286 image-cookable names and 26 names requiring raw native data.
The latter cover shader integers, mixed bump channels and packed depth/stencil
views. Aliases retain the same hardware word; their original spelling belongs
to cook settings rather than the binary GPU descriptor.

Single-level 2D dimensions are limited to 1..4096. Linear storage aligns rows
to eight texels; tiled component storage uses 32x32 tiles. Swizzled storage
uses Y-first Morton addressing and power-of-two extents. Arbitrary swizzled
storage keeps logical dimensions separate from those allocation extents.
PVRTC storage completes its minimum word grid. The catalog rejects unsupported
format/layout combinations.
P4 and P8 use low-first native indices and complete 16-entry or 256-entry
32-bit palettes. Indexed image cooking uses the selected shared quantizer.
PVRTC image cooking currently emits word-average endpoints, which preserves
native storage but gives a coarse image approximation. Its decoder also handles
imported modulation, transparency and PVRTC II local palette modes.

YUV `CSC0` and `CSC1` select GXM conversion profile banks. Version 1 configures
both banks to `SCE_GXM_YUV_PROFILE_BT601_STANDARD`, so the cooker, preview and
runtime share BT.601 limited-range conversion. A CSC1 name does not imply a
BT.709 conversion. Custom profile metadata and mip chains are outside VGT1 v1.

## Runtime contract

Native GPU texels and palettes use owned, mapped allocations. Both 2D sprites
and 3D materials retain the complete GXM descriptor, including its layout and
swizzle. The native path does not expand compressed or indexed assets to
RGBA32 before upload.

Raw integer textures retain integer sampling semantics and require a compatible
native shader; importing their bytes does not make them RGBA UI textures.
The uploader selects point filtering for `U32`, `S32` and `U32U32` as its
sampling policy. Image formats retain linear filtering. Texture binding failures
are reported explicitly in both renderers.

The GXM initialization bridge enables extended texture formats while retaining
libvita2d's initialization parameters. Deferred texture releases use the
existing rendering and display queue fences.

RGBA region updates preserve the legacy RGBA32 path and support native linear
`Vita.U8U8U8U8_ABGR`. Other native encodings reject RGBA region updates before
mutating GPU data. Dynamic font atlases should use an update-capable encoding;
static cooked atlases can use other native sampling formats.
An update during an open frame ends that scene and waits for its rendering
before writing. Drawing resumes on the same backbuffer without clearing it,
swapping it or resetting libvita2d's transient vertex pool.

## References and validation

Primary contracts are the [VitaSDK GXM API](https://github.com/vitasdk/vita-headers/blob/master/include/psp2/gxm.h),
the [libvita2d implementation](https://github.com/xerpi/libvita2d), and
the [Vita3K texture implementation](https://github.com/Vita3K/Vita3K/blob/master/vita3k/gxm/src/textures.cpp).
The installed SDK headers and validation artifacts remain in the fixed
directory `C:/dev/helworks/builds/helengine-psvita/texture-formats`.

Managed codec and builder tests, independent native parser checks, compression
reference checks and compilation with the Vita SDK are separate from visual
verification. This implementation task does not include Vita3K or hardware
rendering.
