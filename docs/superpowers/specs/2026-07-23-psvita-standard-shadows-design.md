# PS Vita Standard Shader Shadows Design

## Goal

Add the first shadow tier through the shared Standard Shader while preserving platform-independent materials. Standard Shader owns its forward, shadow-receiving forward, and depth-only variants. Shader-capable platforms compile those declared variants for their own backends; the PS Vita backend owns only its target artifacts and render passes. Materials continue to express only `CastsShadows` and `ReceivesShadows`.

## Scope

The first tier supports one shadow-enabled directional light, one shadow map, and opaque Standard-material meshes. It uses a 256 by 256 depth map with a hard depth comparison and configurable depth bias on PS Vita. The feature must run through the real Vita shader compiler and use artifact-backed GXM programs at runtime.

For now, every shader-capable target in scope (Windows, PS Vita, and Wii U) compiles all Standard Shader variants. Platforms without shader support do not compile shader artifacts. There is deliberately no per-platform shadow-capability matrix in this tier.

The tier deliberately excludes point-light shadows, spot-light shadows, cascades, soft PCF filtering, transparent shadow casters, and CPU lighting fallbacks.

## Architecture

Standard Shader remains the single authoring source and declares three named variants:

- `ForwardStandard`: the existing unshadowed forward path.
- `ForwardStandardShadowed`: the forward path with light-space position output and shadow-map attenuation.
- `ShadowDepth`: a depth-only caster path.

The shared shader compilation pipeline exposes each declared variant to Windows, PS Vita, and Wii U. Each target compiler produces artifacts in its own native format from the same semantic contract. The native Vita renderer owns a shadow-frame resource containing the depth target, selected directional light data, light view-projection matrix, and depth-bias values. It selects at most one enabled directional light that requests shadows. It then performs the following sequence per camera frame:

1. Render eligible opaque meshes whose materials cast shadows into the shadow depth target.
2. Restore the main render target.
3. Draw each Standard mesh using `ForwardStandardShadowed` only when the selected light exists and the material receives shadows; otherwise use `ForwardStandard`.

`CastsShadows` controls depth-pass participation. `ReceivesShadows` controls forward-variant selection. Those flags keep their current serialized material meaning for every platform.

## Shader and Data Contract

The shadowed Standard vertex artifact consumes the current mesh streams plus the light-space transform. It outputs shadow coordinates for the fragment artifact. The shadowed fragment artifact consumes the existing diffuse texture and one depth texture, compares the fragment depth against the sampled map using the supplied bias, and attenuates the direct directional term only. Ambient lighting remains unshadowed.

The depth artifact needs position and the light view-projection matrix only. It must not depend on the diffuse texture or the main camera matrix.

All shadow program names, required uniforms, and required texture bindings are validated by each target compiler and by the relevant runtime artifact reader. A missing artifact or binding is a build/runtime error, never a fallback to CPU shading.

## Resource and Performance Limits

The first tier uses one 256 by 256 shadow map and one directional light. It uses a hard depth test rather than PCF to limit texture work. A later quality tier may raise the map resolution or introduce a small PCF kernel after real-hardware frame-time measurements show sufficient headroom.

## Error Handling

No enabled shadow light means the renderer uses the existing unshadowed Standard artifact. An enabled shadow light with an invalid shadow artifact, invalid uniform, invalid texture binding, or failed render-target allocation fails explicitly with a diagnostic that identifies the resource or artifact.

## Verification

Automated tests cover artifact naming and required bindings, render-pass selection from material flags, one-light selection, and depth-pass caster filtering. A focused scene contains a directional light, a casting mesh, and a receiving plane. Validation runs in Vita3K and on real Vita, recording both visual correctness and frame rate.
