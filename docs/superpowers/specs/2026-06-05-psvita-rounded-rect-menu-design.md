# PS Vita Rounded-Rect Menu Rendering Design

Date: 2026-06-05
Repository: `C:\dev\helworks\helengine-psvita`
Scope: Finish the current menu by adding filled rounded-rectangle rendering to the PS Vita 2D renderer

## Goal

Extend the working PS Vita 2D renderer so the current menu scene renders its rounded UI panels in addition to the already-working sprites and text.

This milestone is intentionally narrow:

- render the rounded rectangles used by the current menu
- keep sprites and text working as-is
- keep the real editor CLI to VPK to Vita3K flow intact

This milestone explicitly does not need to add:

- border or stroke rendering
- generic vector-shape rendering
- clipping or masking
- builder-side asset changes

## Constraints

- All Vita-specific implementation stays in `helengine-psvita`.
- The build path must continue through the external PS Vita builder and the editor CLI.
- The current GPU-backed Vita2D path remains the rendering backend.
- The design should reuse the existing per-frame 2D queue and flush model instead of adding a separate rendering subsystem.
- The implementation should only cover the rounded-rect behavior the current menu actually uses.

## Current State

The PS Vita runtime now successfully:

- boots generated core
- loads the startup scene through the editor-built manifest path
- uploads runtime textures into native Vita textures
- renders menu sprites and text through the Vita2D-backed GPU path
- stays alive in Vita3K and visibly shows the menu

The remaining gap inside `PsVitaRenderManager2D` is `DrawRoundedRect(...)`, which is still an empty body. The current visible menu therefore shows text and images over the clear-color background, without the intended rounded UI panels.

## Chosen Approach

The chosen approach is CPU-side tessellation of filled rounded rectangles into solid-color 2D triangles, flushed through the existing PS Vita GPU renderer.

Each rounded rectangle will be converted into:

- one center rectangle
- edge rectangles
- four corner triangle fans

The resulting solid-color triangles will be queued during `DrawRoundedRect(...)` and flushed in the same frame lifecycle as the current textured sprite and text geometry.

This approach is preferred because it:

- fits the current menu need exactly
- stays inside the runtime renderer instead of pushing shape generation into assets
- reuses the already-working Vita2D-backed draw path
- gives a clean base for later border or clipping work without replacing the architecture

## Architecture

### 1. Solid-Color Geometry Queue

`PsVitaRenderManager2D` should gain a second per-frame queue for solid-color triangles alongside the existing textured quad queue.

Responsibilities:

- store rounded-rect triangle vertices generated during the current frame
- preserve render order so panels stay behind text and sprites
- flush solid-color geometry before textured geometry during `Draw()`

This should remain a renderer-internal detail. No builder or generated-core changes are needed for the first rounded-rect milestone.

### 2. GPU Renderer Extension

`PsVitaGxmRenderer` should gain one additional submission surface for solid-color triangles.

Responsibilities:

- accept a list of color-only 2D triangles
- allocate transient GPU-visible vertex memory through Vita2D pool allocation
- submit those triangles through the existing GPU-backed frame lifecycle
- preserve the current textured-quad submission path unchanged

The renderer should stay minimal. This is not a general-purpose primitive API. It is only the color-triangle path needed to render the current menu’s rounded rectangles.

### 3. Rounded-Rect Tessellation

`DrawRoundedRect(...)` should:

- validate the drawable and its parent state
- resolve screen position, size, radius, and fill color
- clamp radius to `min(width, height) / 2`
- fall back to a plain filled rectangle when the radius is zero or effectively zero
- tessellate the shape into center, edge, and corner geometry
- append triangles to the solid-color queue

The first milestone only needs filled geometry. Border rendering is intentionally excluded.

## Data Flow

For one rounded rectangle:

1. generated core calls `DrawRoundedRect(...)`
2. Vita renderer resolves drawable data into screen-space geometry
3. the renderer appends solid-color triangle vertices into the current frame queue
4. `PsVitaRenderManager2D::Draw()` flushes:
   - rounded-rect triangles first
   - textured sprites and text second
5. the Vita GPU renderer presents the frame

This preserves the current menu layering model: background and panel shapes render before logo and text.

## Tessellation Strategy

The first implementation should use fixed small fan tessellation for each corner instead of adaptive subdivision.

Recommended structure:

- center rectangle
- top edge
- bottom edge
- left edge
- right edge
- four corner fans with a small fixed segment count

The exact segment count can be modest because the current menu is simple and the PS Vita screen resolution is limited. The goal is stable visible UI, not mathematically perfect vector curves.

## First Milestone Boundaries

Included:

- filled rounded rectangles for current menu usage
- solid-color triangle queue
- solid-color GPU submission path
- real editor CLI verification
- Vita3K visual verification

Excluded:

- rounded-rect borders
- clipping and scissor integration
- generic polygon rendering
- runtime shape caching
- material batching beyond one solid-color path and one textured path

## Test-First Implementation

Implementation should start with one failing source-audit test that proves:

- `DrawRoundedRect(...)` is no longer an empty method
- `PsVitaRenderManager2D` contains solid-color queue state
- `PsVitaGxmRenderer` contains a solid-color triangle submission entrypoint

Then implement in this order:

1. add the failing audit
2. extend the Vita GPU renderer with a solid-color draw path
3. extend `PsVitaRenderManager2D` with a rounded-rect triangle queue
4. implement filled rounded-rect tessellation
5. rebuild through the real editor CLI path
6. verify the visible menu in Vita3K

## Success Criteria

This milestone is complete when all of the following are true:

- `dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal` passes
- the editor CLI `psvita` build succeeds
- the VPK still boots and stays alive in Vita3K
- the visible menu includes its rounded UI panels
- Vita-specific work remains isolated to `helengine-psvita`

## Risks

### Overbuilding The Shape Path

The main risk is turning a narrow menu fix into a generalized shape system too early. This design avoids that by limiting the feature to filled rounded rectangles only.

### Draw Ordering Regressions

Rounded rectangles must render before text and sprites in the current menu. The flush order needs to stay explicit so the menu does not regress visually.

### Excessive Geometry Complexity

Very high corner segment counts would waste CPU and submission work for little visual gain. The first implementation should favor a small fixed tessellation level that is visibly smooth enough on the Vita screen.
