# PS Vita White Cube Rendering Design

## Goal

Render the `cube_test` scene on PS Vita by implementing the smallest useful GPU-backed 3D path: static mesh geometry drawn as solid white triangles, with no material or texture support required for this pass.

## Context

The current PS Vita runtime no longer fails on packaging, generated numeric remaps, `NativeMemory`, or the recent 2D renderer/header/link issues. The remaining runtime blocker is scene loading for `cube_test`.

Current evidence shows:

- `cube_test` loads successfully until scene deserialization reaches `helengine.MeshComponent`
- the PS Vita 3D renderer still has no runtime model implementation
- `PsVitaRenderManager3D::BuildModelFromRaw(...)` currently returns `nullptr`
- the current GPU renderer only exposes 2D textured quads and 2D solid-color triangle submission

That means the next required step is not another build-system fix. It is a real 3D runtime implementation step.

## Problem Statement

We need a PS Vita 3D rendering path that is sufficient for `cube_test`, but we do not need a full general material system yet.

For this pass, the correct target is:

- static mesh loading succeeds
- runtime models are created on Vita
- visible mesh triangles are transformed and submitted to the GPU
- every rendered triangle uses one forced white color

This pass explicitly does not need:

- textured materials
- authored material evaluation
- shadows
- lighting
- batching sophistication beyond what is needed for one cube scene
- a generalized production-complete 3D pipeline

## Recommended Approach

### 1. Add a minimal Vita runtime model bridge

Introduce one PS Vita-owned runtime model representation that copies the generated `RuntimeModel` / `RuntimeSubmesh` data needed for drawing:

- positions
- triangle indices
- bounds or derived counts as needed

This representation should be specific to the Vita platform implementation, but it must consume the existing generated-core model contract instead of inventing a separate asset format.

### 2. Extend the Vita 3D render manager

`PsVitaRenderManager3D` should stop returning `nullptr` from `BuildModelFromRaw(...)`.

Instead it should:

- validate the incoming `ModelAsset`
- build one PS Vita runtime model object
- return that runtime model to the generated scene/runtime layer

Its draw path should also stop acting as a pure 2D forwarder. It should:

- iterate visible cameras
- gather `MeshComponent` drawables reachable from the runtime object model
- submit mesh geometry to the GPU through the Vita renderer

### 3. Extend the Vita GPU renderer with a minimal 3D triangle path

`PsVitaGxmRenderer` already owns frame begin/present and 2D submission.

Add one narrow 3D submission path that can:

- accept already-triangulated mesh geometry
- draw solid white triangles on the GPU
- operate without a material system

This should be shaped as a reusable renderer function, not as one-off cube_test logic in the scene loader.

### 4. Force a white fallback material policy for this pass

To keep the scope controlled, the Vita 3D path should intentionally ignore authored materials and use one explicit white fallback surface for all mesh triangles.

That behavior should be obvious in source and covered by source audits so later material work can replace it deliberately instead of inheriting accidental behavior.

## Non-Goals

- no texture-backed 3D material support
- no lighting model
- no shadow rendering
- no post-processing
- no generated-file patching
- no scene-specific hacks keyed directly to `cube_test`

## File-Level Direction

Expected main change areas:

- `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp/.cpp`
- `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp/.cpp`
- one or more new Vita runtime model/submesh helper files if needed
- builder source-audit tests for the new 3D path contract

## Validation Plan

Validation should proceed in this order:

1. source audits proving the Vita 3D path no longer returns `nullptr` and explicitly submits white mesh geometry
2. targeted Vita builder/source tests
3. normal city PS Vita build
4. Vita3K runtime launch of `cube_test`

Success for this design is:

- `cube_test` scene loads instead of dying on `MeshComponent`
- the cube appears on-screen as GPU-rendered white geometry

## Risks

- The generated runtime model contract may be larger than the minimum static-mesh subset needed for `cube_test`
- Camera/world transform handling may reveal a second bug after model loading is fixed
- The existing 2D-focused GPU wrapper may need a modest shape change to support 3D triangle submission cleanly

## Mitigations

- keep the first implementation static-mesh only
- keep the material policy explicitly white
- add audits that lock the narrow contract
- use the real normal-build `cube_test` launch as the final validation step

## Decision

Implement the narrow reusable static-mesh Vita 3D path, with forced white GPU-rendered triangles, until `cube_test` appears successfully on screen.
