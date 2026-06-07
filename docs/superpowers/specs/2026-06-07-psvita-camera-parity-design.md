# PS Vita Camera Parity Design

## Goal

Restore PS Vita 3D camera behavior so it matches the engine's normal cross-platform camera contract instead of using Vita-specific camera basis or projection behavior.

## Problem

`cube_test` now renders on Vita, but the cube is framed incorrectly relative to working engine renderers. The visible symptom is that the cube appears too low and too small instead of landing near the authored screen center.

Current evidence points to camera-contract drift inside the Vita 3D renderer:

- desktop renderers use the engine default camera forward axis `(0,0,-1)`
- Vita currently uses `(0,0,1)` in `PsVitaRenderManager3D.cpp`
- `cube_test` authoring in `city` assumes the normal engine camera convention

## Approved Direction

Fix Vita for strict parity with the engine camera contract. Do not retune `cube_test` authoring and do not add Vita-only framing compensations.

## Scope

- change `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- extend Vita 3D source audits in `builder.tests`
- rebuild and relaunch the normal city PS Vita build for verification

## Design

The Vita renderer must match the existing engine 3D camera pipeline in these areas:

1. camera forward basis
2. camera up basis
3. view matrix creation
4. projection matrix creation
5. world-view-projection composition order
6. clip-space to screen-space mapping

The implementation should treat desktop engine renderers as the reference contract, especially DirectX11 and Vulkan where camera setup is already working for `cube_test`.

The first corrective step is to restore the Vita camera forward basis to the engine default `-Z`. That earlier Vita-only switch to `+Z` was useful for proving that triangles could reach the GPU, but it should now be treated as evidence of a deeper mismatch rather than the final contract.

After restoring the basis, the Vita path should be checked against the engine matrix contract end to end:

- `viewProjection = view * projection`
- `worldViewProjection = world * viewProjection`
- manual projection math must consume that matrix in a way consistent with the engine's row-major transform layout

If Vita still diverges after restoring `-Z`, the remaining fix belongs in the manual projection path, not in authoring or camera conventions.

## Error Handling

- if a camera has no parent, keep failing as it does now
- if projection produces invalid clip-space values, continue rejecting those vertices
- do not add fallback camera rewrites, auto-centering, or Vita-only scene heuristics

## Test Coverage

Source audits should lock:

- Vita camera forward basis back to engine parity
- Vita still uses engine camera projection utilities
- Vita still composes `view * projection` and `world * viewProjection`
- Vita no longer relies on a Vita-only positive-Z camera convention

Runtime verification should use the normal city PS Vita build with `cube_test` as startup and confirm the visual framing matches the authored scene more closely than the current bottom-skewed result.

## Non-Goals

- changing city scene authoring
- introducing a full depth-tested 3D renderer rewrite
- material-system expansion
- Vita-only camera offsets or viewport hacks
