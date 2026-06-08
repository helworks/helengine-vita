# Cube Test Camera Origin-Five Design

## Goal

Make `cube_test` start from the intended authored camera pose at `(0,0,5)` while preserving manual orbit controls without automatic camera drift.

## Problem

The current `cube_test` scene uses a higher diagonal camera pose and auto-orbit behavior. That does not match the intended baseline framing for the test scene.

The required camera contract is:

- camera local position `(0,0,5)`
- camera looks at the origin
- orbit center remains `(0,0,0)`
- manual orbit remains available
- automatic yaw rotation must be disabled

## Approved Direction

Treat this as an authored scene-contract correction. Do not add Vita-only camera compensations and do not remove the shared orbit camera component entirely.

## Scope

- update the authored `city` `cube_test` scene source
- update the source-level scene contract test
- regenerate the committed `cube_test` asset through the normal editor command
- rebuild and relaunch the Vita package for verification

## Design

`CubeTestSceneFactory` should:

1. place the camera at `new float3(0f, 0f, 5f)`
2. author the initial camera orientation so it faces the origin from that pose
3. keep `DemoDiscOrbitCameraComponent`
4. keep `OrbitCenter = float3.Zero`
5. set `AutoYawSpeedRadians = 0f`

The rest of the minimal scene contract stays unchanged:

- one cube at the origin
- scale `(1,1,1)`
- shared instruction/UI layer present
- no ground
- no DS bottom overlay

## Error Handling

- if the authored camera pose cannot derive a valid orbit radius, scene generation should keep failing normally
- do not add fallback camera initialization or runtime auto-repair logic

## Test Coverage

Source coverage should lock:

- `entity.LocalPosition = new float3(0f, 0f, 5f);`
- the authored orientation creation used for the forward-looking camera
- `OrbitCenter = float3.Zero`
- `AutoYawSpeedRadians = 0f`
- the existing one-cube and shared-UI assertions remain intact

## Non-Goals

- changing Vita renderer math in this spec
- removing manual orbit controls
- changing cube content or scene UI behavior
