# PS Vita Strict Forward Lambert Cube Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make PVM1 models carry normals, require the artifact-backed Forward Lambert path for shader materials, and produce a Cube Test startup VPK.

**Architecture:** The host cooker writes PVM1 v3 with position-aligned normals. The Vita reader restores them into the runtime model. The 3D renderer draws only through Forward Lambert and throws instead of entering any CPU projected triangle fallback.

**Tech Stack:** C# builder serializer and xUnit tests; C++20 Vita runtime; VitaSDK GXM; editor CLI and Vita3K.

---

### Task 1: Preserve normals in PVM1 v3

**Files:**
- Modify: `builder/PsVitaPackedModelAsset.cs`
- Modify: `builder/PsVitaPackedModelAssetBinarySerializer.cs`
- Modify: `builder.tests/PsVitaPlatformAssetBuilderTests.cs`

- [ ] Add a failing serializer round-trip test with three distinct normals and assert they survive cooking.
- [ ] Run `dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter FullyQualifiedName~PsVitaPlatformAssetBuilderTests --no-restore`; expect the new normal assertion to fail.
- [ ] Add `Normals` to `PsVitaPackedModelAsset`, set serializer version to `3`, serialize and deserialize the aligned normal array, and reject missing or misaligned normals in `SerializeModelAssetToBytes`.
- [ ] Re-run the targeted builder tests; expect all selected tests to pass.

### Task 2: Restore normals and forbid GPU fallback for shader materials

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaPackedModelReader.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] Add failing source audits requiring PVM1 v3 normal reads, a strict Forward Lambert failure path, and no queued CPU mesh-triangle submission from `DrawRuntimeModel`.
- [ ] Run `dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests --no-restore`; expect the new audits to fail.
- [ ] Read PVM1 v3 normals into `PsVitaRuntimeModel`; remove the 3D CPU projected-triangle submission from `DrawRuntimeModel` and throw if Forward Lambert cannot draw.
- [ ] Re-run the focused source-audit suite; expect all tests to pass.

### Task 3: Build and verify Cube Test

**Files:**
- Modify: `docs/superpowers/plans/2026-07-21-psvita-shader-artifact-pipeline.md`

- [ ] Run the focused builder and source-audit tests.
- [ ] Build through the editor CLI with `HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID=cube_test` into `C:\dev\helprojs\demodisc\vita-build-strict-gpu-lambert-cube-test`.
- [ ] Verify the VPK contains PVM1 v3 cube data and both shader artifacts, install it in Vita3K, and launch `HLEN00001` without console mode.
- [ ] Record whether the runtime trace enters `cube_test` and whether Vita3K logs GXM draw calls without CPU projected fallback diagnostics.
