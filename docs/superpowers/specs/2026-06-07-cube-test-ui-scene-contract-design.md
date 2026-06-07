# Cube Test UI Scene Contract Design

## Goal

Restore the shared instruction UI for `cube_test` without reintroducing the earlier scene drift, while keeping the scene usable for strict Vita renderer parity debugging.

## Problem

`cube_test` had drifted away from its intended purpose. It was temporarily corrected to a single rotating cube at the origin, but that correction also removed the regular instruction/UI layer that other platforms normally show.

The required contract is now:

- one cube only
- cube at `(0,0,0)`
- cube scale `(1,1,1)`
- cube rotates
- no ground
- no DS-specific bottom overlay
- regular shared instruction/UI layer restored

This must be treated as a scene-contract change, not a Vita-only workaround.

## Approved Direction

Restore only the normal shared instruction/UI layer while preserving the minimal rotating-cube scene. Do not add DS overlay content, do not reintroduce extra geometry, and do not compensate for Vita camera issues through authoring changes.

## Scope

- update the authored `city` `cube_test` scene source
- update the source-level scene contract test
- regenerate the committed `cube_test` asset through the normal editor command
- rebuild and relaunch the Vita package to validate the updated scene against the current renderer

## Design

The scene should remain a focused rendering test. The authored content must be:

1. one camera
2. one directional light
3. one cube mesh entity
4. shared instruction/UI layer

The cube entity must:

- stay at the origin
- use scale `(1,1,1)`
- keep the existing spin component behavior

The scene must not add:

- ground
- secondary meshes
- DS bottom-screen overlay content
- Vita-specific framing helpers

The instruction layer should be restored through the same shared UI/instruction path used elsewhere rather than a new scene-local UI implementation. The scene remains a renderer validation scene, but it should match the normal user-facing instruction behavior expected across platforms.

## Error Handling

- if the shared instruction/UI dependencies are missing, scene generation should continue failing normally rather than silently omitting required content
- do not add fallback scene content to mask missing assets or missing UI dependencies

## Test Coverage

Source coverage should lock:

- single cube entity present
- cube local position is `(0,0,0)`
- cube local scale is `(1,1,1)`
- spin component remains attached
- shared instruction/UI layer is present
- ground and other extra scene geometry are absent
- DS bottom overlay remains disabled

Runtime verification should use the normal city PS Vita build and relaunch flow so the updated scene can be checked against the current Vita rendering path.

## Non-Goals

- fixing Vita camera parity in this spec
- adding platform-specific scene variants
- changing the cube into a multi-object demo scene
- adding new instruction systems
