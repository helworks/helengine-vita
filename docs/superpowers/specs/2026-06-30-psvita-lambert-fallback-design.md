# PS Vita Lambert Fallback Design

## Goal

Improve the current PS Vita and Vita3K-safe 3D fallback from flat white triangles to simple Lambert-lit triangles, using existing scene light data and keeping the runtime shader compiler path disabled for emulator stability.

## Context

Current state is split in two:

- the emulator-safe fallback path works and reaches `RunMainLoop`, but it renders CPU-projected white triangles with no lighting
- the runtime `libshacccg` path now loads in Vita3K, but the emulator crashes immediately after `SceShaccCg` module startup with `PC = 0x0`

That means the next useful step is not “make Vita shaders general right now.” The next useful step is to improve the known-good fallback path so scenes gain depth cues while the shader-export/runtime-compile story remains unresolved.

Agreed constraints:

- start with Lambert only
- use existing scene light data
- per-vertex quality is acceptable
- keep work incremental
- emulator workflow must remain stable

## Problem Statement

The fallback path is currently good enough to prove geometry and camera correctness, but not good enough to judge scene readability. Flat white cubes hide authored light direction, base-color contrast, and surface orientation.

We need one stopgap lighting pass that:

- preserves the current emulator-safe CPU projection path
- consumes existing runtime light state instead of inventing a Vita-only light model
- remains narrow enough that it can be replaced later by a real GPU material/shader path

## Approaches Considered

### 1. CPU Lambert on the existing projected-triangle fallback path

Use the current `PsVitaRenderManager3D` CPU projection path, extend the runtime model to carry normals, sample directional and ambient light from the existing object manager, compute per-vertex Lambert colors, and submit colored projected triangles through the existing solid-color GPU path.

Pros:

- keeps Vita3K stable because it does not exercise `libshacccg`
- smallest code change that materially improves readability
- reuses current camera, transform, and draw wiring

Cons:

- still CPU-side 3D
- no textures
- no specular, shadows, or full material evaluation

### 2. Re-enable the programmable GXM path and add Lambert there

Push forward on the runtime-compiled solid-color shader path, add light uniforms, and try to make Vita3K tolerate `libshacccg`.

Pros:

- closer to the eventual real GPU path
- avoids CPU projection cost if it works

Cons:

- blocked in practice by Vita3K crashing immediately after `SceShaccCg` module startup
- debugging that crash is a separate subsystem problem
- high risk for an iteration whose immediate goal is “readable emulator rendering”

### 3. Build a real shader-cache/export system first

Design the general PS Vita shader asset flow now, including source packaging, runtime compile/cache keys, and emulator cache reuse.

Pros:

- directly aligned with the long-term architecture

Cons:

- much larger scope
- does not improve short-term Vita3K readability unless the cache/runtime path is also implemented
- not appropriate for the requested “small steps” stopgap

## Decision

Implement approach 1 now: CPU Lambert lighting on top of the existing emulator-safe fallback path.

This is the smallest change that:

- keeps Vita3K usable
- uses real scene lights
- makes 3D scenes readable enough to continue platform bring-up

## Architecture

### Runtime model data

`PsVitaRuntimeModel` currently carries copied positions plus Vita-owned submeshes. The Lambert fallback should extend that runtime copy to also carry authored normals from `ModelAsset->Normals`.

If normals are missing or do not match the position count, the lighting path should fall back to one face normal per triangle so the scene remains visible instead of failing.

### Light selection

The first stopgap should consume only the existing directional and ambient light data already registered on the runtime object manager.

Lighting policy:

- directional light: use the first active directional light in `Core::Instance->ObjectManager->DirectionalLights`
- ambient light: accumulate ambient contribution from `AmbientLights`
- no point lights in this slice
- no spot lights in this slice
- if no usable light exists, preserve visibility by using the base color unchanged instead of rendering black

Directional light direction should follow the same authored basis conventions already used elsewhere in the engine: derive the forward vector from the light entity orientation instead of inventing a Vita-specific convention.

### Material color policy

The first Lambert step should not attempt a full material system.

Color policy:

- if the runtime material is `PsVitaCompiledShaderRuntimeMaterial`, use its stored `BaseColorAbgr`
- otherwise fall back to white for now

This keeps the feature small and useful for the current compiled-shader-authored cube scenes, without dragging generic material constant-buffer parsing into the first slice.

### Draw path

The current fallback path in `PsVitaRenderManager3D` already:

- builds `worldViewProjection`
- projects triangles into screen space
- queues white mesh vertices

The Lambert stopgap should keep that structure, but replace the white queue with colored solid-color vertices:

1. transform positions into world space
2. resolve a normal for each vertex
3. compute Lambert intensity from directional and ambient light
4. multiply the material base color by that intensity
5. queue `PsVitaSolidColorVertex` entries
6. flush through the existing `PsVitaGxmRenderer::SubmitSolidColorTriangles(...)`

No new programmable shader path is required for this slice.

## File-Level Direction

Expected code changes:

- `src/platform/psvita/rendering/PsVitaRuntimeModel.hpp`
- `src/platform/psvita/rendering/PsVitaRuntimeModel.cpp`
- `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp`
- `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`

Expected test changes:

- create one new cross-repo source audit in `C:\dev\helworks\helengine\engine\helengine.editor.tests\PsVitaLambertFallbackSourceTests.cs`

That test location is intentional because the current `helengine.psvita.builder.tests` project has unrelated compile drift and should not block this rendering slice.

## Non-Goals

- no textured Lambert in this slice
- no point or spot lights
- no specular lighting
- no shadows
- no runtime `libshacccg` usage
- no general PS Vita shader asset/export system
- no generated-core rewrites

## Validation Plan

Validation order:

1. source audit proving normals are copied into the Vita runtime model and that `PsVitaRenderManager3D` resolves directional/ambient light and queues colored solid-color vertices
2. normal city PS Vita build
3. Vita3K launch with the shader compiler path still disabled
4. visual confirmation that the cube reads as lit rather than flat white

Success means:

- Vita3K still boots reliably
- the visible cube shows directional shading
- scenes with no usable lights stay visible instead of turning black

## Risks

- authored normals may be missing or mismatched for some assets
- using only the first directional light may be visibly incomplete in some scenes
- some non-compiled materials will still render white until a later base-color extraction slice

## Mitigations

- use face-normal fallback when authored normals are absent
- add ambient contribution so the stopgap is readable even with weak directional lights
- keep base-color handling deliberately narrow and document the white fallback for non-compiled materials

## Decision Summary

Do not spend the next slice on `libshacccg` crash work. Improve the stable fallback renderer instead: per-vertex Lambert lighting, existing directional plus ambient lights, authored normals when available, and no textures yet.
