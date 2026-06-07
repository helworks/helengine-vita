# PS Vita White Cube Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `cube_test` render on PS Vita as GPU-drawn solid white mesh geometry.

**Architecture:** Build the smallest reusable static-mesh Vita 3D path: a Vita-owned runtime model bridge plus a white-triangle GPU submission path in the existing GXM renderer. Keep authored materials ignored for now, and validate with source audits first, then a normal city build and Vita3K launch.

**Tech Stack:** C++, VitaSDK, vita2d/GXM, xUnit source audits, CMake, normal editor-driven PS Vita build

---

## File Structure

- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.hpp`
  - Own the Vita-side runtime model object returned to generated-core mesh components.
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.cpp`
  - Build and dispose the Vita runtime model implementation.
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeSubmesh.hpp`
  - Own one Vita-side static submesh payload with positions and indices needed for white-triangle submission.
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeSubmesh.cpp`
  - Implement the Vita runtime submesh helpers.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.hpp`
  - Expose the new model-building and 3D draw responsibilities.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp`
  - Stop returning `nullptr`, build Vita runtime models, gather mesh drawables, and submit white mesh geometry.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.hpp`
  - Expose a minimal 3D solid-triangle submission path.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.cpp`
  - Implement the actual GPU-backed white-triangle submission path.
- Modify: `C:\dev\helworks\helengine-psvita\CMakeLists.txt`
  - Compile any new Vita runtime model files.
- Modify: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaRenderManager3DSourceAuditTests.cs`
  - Add source audits for the 3D runtime model and white-triangle path.
- Modify: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs`
  - Add source audits for the GPU submission path if that is where the white-triangle logic lives most clearly.

## Task 1: Lock the Missing 3D Runtime Path With Source Audits

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaRenderManager3DSourceAuditTests.cs`
- Modify: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Add a failing source audit that proves Vita 3D model loading is still stubbed**

Add assertions that currently fail because `BuildModelFromRaw(...)` still returns `nullptr`, and require the source to:

- reference a Vita runtime model type
- validate model input
- return a concrete runtime model object

- [ ] **Step 2: Add a failing source audit that proves the Vita GPU path can submit white 3D triangles**

Require the source to show:

- a dedicated 3D submission function on `PsVitaGxmRenderer`
- explicit white fallback color usage
- triangle submission through the GPU path, not through a CPU-side placeholder

- [ ] **Step 3: Run the targeted audits and verify they fail for the expected reasons**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests|FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests"
```

Expected:

```text
FAIL because the Vita 3D path still returns nullptr and has no white-triangle submission path
```

## Task 2: Add the Vita Runtime Model Bridge

**Files:**
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.hpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.cpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeSubmesh.hpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeSubmesh.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\CMakeLists.txt`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Read the generated-core model surfaces used by `MeshComponent`**

Confirm the minimal static-mesh subset needed from:

- `RuntimeModel`
- `RuntimeSubmesh`
- `ModelAsset`
- any vertex/index arrays required by `cube_test`

- [ ] **Step 2: Create the Vita runtime submesh type**

Implement one focused type that stores:

- position data
- triangle indices
- counts needed for drawing

- [ ] **Step 3: Create the Vita runtime model type**

Implement one focused runtime model class that owns:

- one or more Vita runtime submeshes
- any bounds metadata the runtime expects

- [ ] **Step 4: Add the new files to the Vita CMake build**

Update `CMakeLists.txt` so the new runtime model sources compile in normal builds.

- [ ] **Step 5: Re-run the targeted audits if they cover concrete runtime-model type usage**

Run the same targeted audit command and confirm the model-loading assertions move toward green.

## Task 3: Implement the White-Triangle GPU Submission Path

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.hpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.cpp`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Add a minimal 3D submission surface to `PsVitaGxmRenderer`**

Define one function that accepts already-triangulated transformed mesh data and draws it as solid white geometry.

- [ ] **Step 2: Implement the GPU path with the smallest viable draw contract**

Requirements:

- reuse the existing frame lifecycle
- submit triangles on the GPU
- force white color explicitly
- avoid introducing a real material system

- [ ] **Step 3: Re-run the targeted GPU-path audit**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests"
```

Expected:

```text
PASS
```

## Task 4: Wire `PsVitaRenderManager3D` to Load and Draw White Meshes

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.hpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Replace the `BuildModelFromRaw(...)` stub with real Vita runtime model construction**

The method must stop returning `nullptr` and instead build one concrete Vita runtime model from the generated `ModelAsset`.

- [ ] **Step 2: Gather mesh drawables during the Vita 3D draw pass**

Extend the draw path so it finds runtime mesh content from active cameras and submits it through the new white-triangle GPU path.

- [ ] **Step 3: Ignore authored materials explicitly for this pass**

Keep the behavior deliberate and obvious in source: all 3D mesh triangles render white until a later material pass exists.

- [ ] **Step 4: Re-run the targeted 3D renderer audit**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests"
```

Expected:

```text
PASS
```

## Task 5: Rebuild and Validate `cube_test`

**Files:**
- No new source files expected

- [ ] **Step 1: Run the focused Vita source-audit set**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests|FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests"
```

Expected:

```text
PASS
```

- [ ] **Step 2: Run the normal editor-driven city PS Vita build**

Use the existing normal-build path to produce the VPK into:

```text
C:\tmp\city-psvita-cube-test-normal
```

Expected:

```text
Build completed for platform 'psvita'
```

- [ ] **Step 3: Launch the built VPK in Vita3K**

Use:

```powershell
& 'C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1' -VpkPath 'C:\tmp\city-psvita-cube-test-normal\helengine_psvita.vpk'
```

Expected:

```text
cube_test appears on screen as white GPU-rendered geometry
```

- [ ] **Step 4: Commit the white-cube implementation once build and launch validation succeed**

```bash
git add CMakeLists.txt builder.tests/PsVitaRenderManager3DSourceAuditTests.cs builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs src/platform/psvita/rendering/PsVitaGxmRenderer.hpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp src/platform/psvita/rendering/PsVitaRenderManager3D.hpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp src/platform/psvita/rendering/PsVitaRuntimeModel.hpp src/platform/psvita/rendering/PsVitaRuntimeModel.cpp src/platform/psvita/rendering/PsVitaRuntimeSubmesh.hpp src/platform/psvita/rendering/PsVitaRuntimeSubmesh.cpp
git commit -m "feat: render white cube meshes on PS Vita"
```
