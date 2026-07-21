# PS Vita GPU Forward-Lambert Renderer Design

## Goal

Move forward Lambert lighting from the CPU fallback into a native Vita GXM vertex/fragment shader path, using the shader artifact pipeline and preserving a clear CPU/GPU responsibility boundary.

## Context

The existing CPU-side Lambert path is not a viable solution for the engine's rendering problems. The Vita SGX543MP4+ is a programmable, tile-based GPU. The renderer should use the GPU for per-vertex or per-fragment lighting while keeping CPU work focused on scene preparation, batching, culling, resource binding, and draw submission.

The first implementation must be deliberately small. It should establish the correct data flow and measurable GPU execution before adding material features or multiple light types.

## Decisions

### First material variant

The first GPU material is an opaque forward-Lambert variant with:

- Position input.
- Normal input.
- Optional diffuse texture in a later incremental variant.
- World-view-projection transform.
- World-space normal transform or a documented rigid-transform restriction for the first milestone.
- One directional light direction and color.
- Ambient term and material base color.
- Opaque color output compatible with the current GXM render target.

The initial lighting equation is:

```text
color = baseColor * (ambient + lightColor * max(dot(normal, -lightDirection), 0))
```

All values are supplied through explicit uniform buffers. No per-vertex lighting result is calculated on the CPU.

### CPU responsibilities

The CPU computes or prepares object transforms, camera matrices, light constants, material constants, culling results, state-sort keys, and packed vertex streams. It does not iterate all mesh vertices to calculate Lambert intensity.

### GPU responsibilities

The vertex shader transforms positions and normals. The fragment shader performs the Lambert dot product and material/light combination. If the first milestone uses per-vertex lighting for cost reasons, that is an explicit shader variant—not a return to CPU lighting—and the fragment shader remains responsible for material composition.

### Renderer integration

The shader and material selection is data-driven through the cooked PS Vita material payload. `PsVitaRenderManager3D` chooses a runtime material/program, binds the correct vertex stream and uniform data, and submits opaque draws through the existing GXM scene. UI and 2D paths remain separate.

## Components

### Shader sources and variants

Add one shared source definition for the forward Lambert vertex and fragment stages with stable entry points and parameter names. Keep variant declarations explicit so the builder can request `ForwardLambertOpaque` rather than infer behavior from missing fields.

### Runtime program wrapper

Create a focused Vita GXM program wrapper for the Lambert pair. It owns loaded artifact memory, patcher registrations, resolved parameter handles, patched program objects, and cleanup. It does not own scene traversal or material parsing.

### Uniform binder

Add a small related type responsible for writing the world-view-projection matrix, normal transform, base color, ambient term, light direction, and light color into the shader's uniform buffers. It must validate required parameter handles during initialization.

### Material selection

Extend the cooked-material reader/runtime material representation with the Lambert variant and its stage artifact references. Missing required fields are errors; no silent default shader is created for a material that explicitly requests the Lambert path.

## Data flow

```text
Scene/material data
    -> CPU culling, sorting, transform/light preparation
    -> cooked ForwardLambert material
    -> artifact-backed GXM program wrapper
    -> uniform binder
    -> packed vertex stream
    -> one opaque GXM draw
    -> GPU vertex transform and Lambert shading
```

## Performance goals

- Eliminate CPU per-vertex Lambert evaluation from the production Vita path.
- Avoid shader compilation during application startup.
- Keep the first variant opaque and batchable.
- Preserve one large opaque scene where possible.
- Measure draw count, scene breaks, CPU submission time, and GPU frame timing separately so shader cost is not confused with submission cost.

## Failure behavior

- Missing shader artifacts, missing required parameters, incompatible vertex formats, and patcher failures fail program initialization.
- The renderer reports the material/program failure with the variant and artifact identity.
- A fallback is allowed only when explicitly selected by a development configuration or when the material itself requests a fallback variant.
- The shader wrapper releases patcher references, registrations, mapped memory, and host memory in reverse ownership order.

## Validation

The renderer is complete for the first milestone when:

1. The solid-color artifact path remains functional without runtime compilation.
2. A test mesh with known normals renders visibly different intensities for opposing light directions.
3. CPU instrumentation confirms Lambert intensity is not calculated per vertex.
4. The same cooked material can be loaded repeatedly without leaking GXM or host allocations.
5. Vita3K/source audits cover the program wrapper, uniform contract, CMake wiring, and fallback policy.
6. A real Vita validates output, performance counters, and artifact compatibility.

## Out of scope

- Shadows, skinning, normal mapping, multiple dynamic lights, specular BRDFs, deferred rendering, and transparent lighting.
- Global renderer rewrites unrelated to the forward opaque path.
- Treating Vita3K behavior as proof of real-hardware performance.

