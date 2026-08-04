# DS/3DS bottom controls platform cooking

## Goal

Ensure the Nintendo DS/3DS bottom-screen Light and Back controls are serialized only for the `ds` and `3ds` targets. They must be absent from all other cooked scene payloads, including PSVita physics scenes.

## Existing contract

The demo-disc scene generator creates the handheld scaffold separately and assigns entity-existence overrides that restrict its roots to `ds` and `3ds`. The common scene roots receive complementary handheld exclusions. The scene cooker must resolve these overrides for its target platform before it serializes a cooked scene.

## Change

Correct the shared scene cooking path to prune an entity subtree when its resolved existence override is false for the active platform. Pruning applies before asset-reference collection and serialization, so excluded UI entities and their child components do not enter the platform payload.

## Validation

Add a focused regression test that cooks one scene with a DS/3DS-only subtree and verifies that the PSVita cooked payload omits it while DS and 3DS payloads retain it. Rebuild the PSVita package and confirm the physics-scene payloads no longer contain `DemoDiscBottomScreenLightButton` or `DemoDiscBottomScreenBackButton`.
