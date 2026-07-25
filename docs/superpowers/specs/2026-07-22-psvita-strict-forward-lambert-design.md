# PS Vita Strict Forward Lambert Design

## Goal

Render shader-backed PS Vita meshes only through the real-Vita Forward Lambert PVSA programs. The cube-test validation build must start directly in `cube_test` and must not fall back to CPU-projected triangles.

## Design

`PVM1` advances from version 2 to version 3. Version 3 stores one normal for every packed position between the position and texture-coordinate arrays. Cooking rejects source models whose normals are absent or not position-aligned. The Vita reader accepts only version 3 and constructs `PsVitaRuntimeModel` with those normals.

`PsVitaRenderManager3D` treats every 3D mesh draw as a strict GPU contract. If its runtime model lacks aligned normals, its material is not compiled for Forward Lambert, its program fails to initialize, or its GXM submission fails, the renderer throws a diagnostic error. It never queues CPU-projected or CPU-Lambert triangles.

The validation VPK uses `HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID=cube_test` during the editor CLI build. This directs the generated boot scene to Cube Test without altering the project's saved scene-selection configuration.

## Verification

Builder tests prove normal round-tripping and reject missing normals. Source audits prove shader-backed meshes cannot enter the CPU projected fallback. The final VPK must contain PVM1 v3 cube data, both PVSA artifacts, and run Cube Test directly in Vita3K with GXM shader logging enabled.
