# PS Vita GXM 2D Renderer Design

Date: 2026-06-05
Repository: `C:\dev\helworks\helengine-psvita`
Scope: Main-menu-ready PS Vita 2D rendering for sprites and text using native GXM

## Goal

Implement a PS Vita 2D renderer that can render the main menu through the real editor CLI build path, without introducing throwaway software rendering or pushing Vita-specific code into the main `helengine` repository.

The first milestone must render:

- `DrawSprite`
- `DrawText`

The first milestone explicitly does not need to render:

- `DrawRoundedRect`
- clipping or masking
- advanced UI effects
- multiple 2D material pipelines

## Constraints

- All Vita-specific implementation stays in `helengine-psvita`.
- The build path must continue to run through the editor CLI and the external PS Vita builder.
- The renderer must be native GXM from the start. A software framebuffer rasterizer is out of scope.
- The runtime must continue to consume the existing generated core and cooked asset outputs produced by the editor.
- The first implementation should be narrow and biased toward getting menu sprites and text visible before expanding feature coverage.

## Reference Inputs

This design is based on:

- `docs/PS Vita Rendering Docs.md`
- the current `PsVitaBootHost`
- the current `PsVitaRenderManager2D`
- sibling-platform runtime texture and 2D renderer structure

The local rendering research points to the same architectural direction:

- PS Vita performance depends on respecting its tile-based deferred renderer and scene-based GXM submission model.
- Scene count and pass count should stay low.
- State changes should be minimized.
- Alpha-heavy UI work is expensive, so batching matters immediately.
- Native GXM ownership is a better long-term fit than a temporary software path or a wrapper that would later be replaced.

## Current State

The current PS Vita runtime can:

- boot a native Vita app
- initialize generated core when present
- load startup scene manifests
- stage cooked assets into the VPK through the editor CLI path
- materialize runtime textures as CPU-owned ABGR8888 pixel buffers

The current PS Vita runtime cannot yet:

- submit textured 2D geometry through GXM
- render sprites
- render font atlas glyphs
- present a real menu through the runtime renderer

`PsVitaRenderManager2D` currently owns texture conversion and release flow, but `DrawSprite` and `DrawText` are still empty.

## Chosen Approach

The selected approach is a batched native GXM textured-quad renderer.

`DrawSprite` and `DrawText` will not submit immediate draw calls directly. They will enqueue quad data into a CPU-side command list owned by the PS Vita 2D renderer. At frame flush, the renderer will upload any pending textures, sort and batch quad submissions, emit one transparent 2D pass, and present through a Vita-owned GXM render path.

This approach is preferred because it:

- aligns with the PS Vita rendering guidance in the local research
- avoids building a disposable software renderer
- avoids introducing a wrapper dependency that would later need replacement
- gives a usable path from first menu pixels to later optimization without replacing the architecture

## Architecture

### 1. Vita GXM Foundation Layer

Add a PS Vita renderer-owned GXM layer responsible for:

- GXM initialization and shutdown
- render target creation
- display buffer allocation and swap management
- shader patcher ownership
- vertex and fragment program ownership
- dynamic vertex and index buffer allocation
- frame begin, frame submit, and present

This layer should be focused on textured 2D quad rendering and presentation. It should not try to become a generalized renderer abstraction in the first milestone.

### 2. Runtime Texture Ownership

Keep `PsVitaRuntimeTexture` as the runtime representation created from authored cooked texture assets.

Extend it so it owns:

- CPU-side ABGR8888 texels
- texture dimensions
- deterministic runtime asset id
- optional PS Vita GPU texture residency state

The initial uploaded texture path should be simple:

- keep authored pixel conversion in the existing runtime texture/cache layer
- lazily upload the texture to Vita GPU memory on first use
- reuse uploaded textures across subsequent sprite and text draws
- defer GPU destruction until the renderer reaches a safe release point

This keeps the editor/builder side generic for now and follows the same general pattern already used by sibling platforms, where the runtime renderer specializes the final texture representation.

### 3. `PsVitaRenderManager2D`

`PsVitaRenderManager2D` should become the CPU-side 2D command collector and flush coordinator.

Responsibilities:

- build or reuse runtime textures
- translate `ISpriteDrawable2D` into quad commands
- translate `ITextDrawable2D` into glyph quad commands
- sort commands by 2D render order and texture
- flush those commands through the Vita GXM layer during `Draw()`
- keep release timing safe for runtime textures and font-owned atlas textures

It should not attempt to own low-level display setup directly.

### 4. Quad Pipeline

The first pass uses a single textured-quad pipeline for both sprites and text.

One shared vertex format is sufficient:

- screen position
- texture coordinates
- packed ABGR color

One shared index pattern is sufficient:

- 4 vertices per quad
- 6 indices per quad

One shared blend mode is sufficient:

- standard alpha blending

This is enough for menu sprites and atlas-backed text.

## Sprite Rendering

`DrawSprite` will:

- validate that the drawable and runtime texture are usable
- resolve sprite world or screen placement through the data exposed by the engine runtime
- derive normalized UVs from `SourceRect` and the runtime texture dimensions
- apply size, tint, and rotation
- enqueue one quad with render-order metadata and texture identity

The first milestone only needs to support the sprite data already exposed by `ISpriteDrawable2D`:

- `Texture`
- `Rotation`
- `Color`
- `SourceRect`
- `Size`

## Text Rendering

`DrawText` will use the existing generated-core text and font data rather than introducing a separate Vita text format.

Responsibilities:

- read `FontAsset`
- use the existing font atlas runtime texture
- respect `FontScale`
- respect wrapping
- respect horizontal alignment
- emit one textured quad per visible glyph

The first milestone will use the current engine-side text layout data and helpers as the authoritative source of glyph metrics and layout behavior. Text is therefore rendered as batched textured quads through the same pipeline as sprites.

## Frame Lifecycle

The current boot host still owns a manual framebuffer presentation loop. That should be replaced for the 2D-rendered path.

The target frame lifecycle is:

1. Boot host initializes the PS Vita GXM layer.
2. Runtime update builds normal render command requests through generated core.
3. `PsVitaRenderManager2D::DrawSprite` and `DrawText` enqueue quad commands.
4. `PsVitaRenderManager2D::Draw()` flushes the current frame:
   - begin frame
   - upload any dirty textures
   - sort and batch quads
   - bind the textured 2D pipeline
   - submit batched quads
   - present

The 2D renderer should remain a single transparent UI pass for the first milestone. Extra passes are intentionally avoided.

## First Milestone Boundaries

Included:

- native GXM setup for 2D
- sprite rendering
- font atlas text rendering
- runtime texture GPU upload
- editor CLI to Vita VPK to Vita3K verification

Excluded:

- rounded rectangles
- clipping
- masking
- signed-distance-field text
- nine-slice UI
- multi-material UI
- builder-side Vita-exclusive cooked texture formats
- 3D renderer integration beyond what is needed to coexist safely

## Test-First Implementation Plan

Implementation should begin with source-audit coverage that proves the architecture changed in the intended way before the renderer is fully operational.

Planned initial tests:

- assert `PsVitaRenderManager2D` no longer contains empty `DrawSprite` and `DrawText` bodies
- assert new PS Vita GXM renderer source files exist and are referenced
- assert boot host references the Vita GXM renderer path
- assert `CMakeLists.txt` includes the new Vita renderer sources and shader assets

After that:

1. add the GXM foundation layer
2. extend runtime textures to GPU residency
3. implement sprite quad submission
4. implement text glyph submission
5. wire frame begin, flush, and present
6. verify through the real editor CLI build and Vita3K

## Success Criteria

This milestone is complete when all of the following are true:

- PS Vita still builds through the editor CLI path
- the generated VPK still installs and boots in Vita3K
- the menu shows visible sprites
- the menu shows visible text
- no Vita-specific code has been pushed into the main `helengine` repository

## Risks

### GXM Setup Complexity

The first working GXM path is more expensive to establish than a software path. That is accepted because the software path would be discarded and would not help long-term.

### Alpha Cost

The local rendering research strongly suggests that blended work can become the dominant cost on PS Vita. The first milestone mitigates this by constraining the pipeline to a single textured pass and batching by texture and order.

### Texture Format Prematurity

A builder-side Vita-exclusive texture format could be introduced too early and lock the project into the wrong asset boundary. This design intentionally keeps texture specialization in the runtime renderer for the first milestone.

### Menu Feature Gaps

If the menu requires clipping or unsupported UI primitives immediately, this milestone may show partial output before full parity. That is acceptable for the current step because the immediate objective is to get sprite and text pixels on screen through the correct native renderer path.

## Decision

Proceed with a batched native GXM 2D renderer that supports only sprites and font-atlas text in the first milestone.
