# PS Vita Colored Cube Material Colors

## Goal

Make the PS Vita emulator-safe CPU-projected 3D fallback preserve authored material base colors so a simple colored-cube scene renders with distinct colors. This milestone does not add texture sampling and does not enable the unstable runtime-compiled GXM mesh shader path.

## Current root cause

The Vita builder preserves standard material colors in the cooked `ShaderMaterialAsset` `BaseColorBuffer` constant buffer. The Vita runtime currently constructs a generic `RuntimeMaterial` for that payload, so `ResolveSolidColorSubmeshColor` cannot retrieve the authored color and returns opaque white. The CPU fallback already supports packed per-vertex colors through `PsVitaSolidColorVertex` and `vita2d_draw_array`.

## Design

The runtime material loading path will identify standard cooked `ShaderMaterialAsset` payloads before reducing them to the generic `MaterialAsset` interface. It will construct the Vita-specific runtime material representation and extract the `BaseColorBuffer` float4 into packed ABGR. The existing dedicated compiled-shader payload path will retain its current packed-color handling.

The CPU fallback will continue to:

- transform and project model triangles on the CPU;
- apply the existing material culling and painter sorting;
- calculate Lambert lighting from the resolved base color;
- submit colored screen-space triangles through `vita2d`.

The programmable `PsVitaGxmSolidColorProgram` path remains disabled. Texture loading, UV handling, and shader activation are explicitly out of scope.

## Data flow

```text
cooked ShaderMaterialAsset
        |
        v
read BaseColorBuffer float4
        |
        v
PsVitaCompiledShaderRuntimeMaterial.BaseColorAbgr
        |
        v
ResolveSolidColorSubmeshColor
        |
        v
BuildLambertVertexColor
        |
        v
vita2d colored triangle submission
```

## Error handling

The loader will reject malformed or missing base-color data rather than silently inventing a new material color. A valid material with no authored base-color buffer will use the established opaque-white default only where the existing cooked-material contract explicitly permits that default. Invalid buffer length or non-finite color data will produce a descriptive load failure.

## Tests

Add tests before implementation that establish:

1. A standard Vita `ShaderMaterialAsset` cook contains the authored base-color buffer.
2. The native material-loading source extracts that buffer for the Vita runtime material instead of discarding it.
3. The CPU fallback resolves the extracted packed color and passes it into colored vertex construction.
4. The programmable shader path remains disabled for this milestone.

Run the focused Vita builder tests and the smallest available Vita native build validation after implementation.

## Scope boundary

This change targets colored solid geometry only. It must not modify Vita texture upload, textured quad rendering, runtime shader compilation, camera projection, culling, or unrelated existing worktree changes.
