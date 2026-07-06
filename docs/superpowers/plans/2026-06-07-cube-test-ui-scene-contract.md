# Cube Test UI Scene Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the regular shared instruction UI in `cube_test` while keeping the scene as a single rotating cube at `(0,0,0)` with scale `(1,1,1)` and no DS bottom overlay.

**Architecture:** Keep `cube_test` as a minimal authored rendering scene and restore only the shared instruction/UI path that other platforms already use. Lock the scene contract with a source-level test in `helengine`, then regenerate the authored asset and validate the result through the normal PS Vita build and relaunch flow.

**Tech Stack:** C# scene authoring, xUnit source tests, helengine editor command pipeline, PS Vita builder, PowerShell launch script, git

---

### Task 1: Lock the Updated Scene Contract in Source Tests

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`

- [ ] **Step 1: Rewrite the source test to express the new contract**

Update `CityCubeTestSceneSourceTests.cs` so it asserts all of the following against `CubeTestSceneFactory.cs`:

- the cube entity remains present
- `entity.LocalPosition = new float3(0f, 0f, 0f);`
- `entity.LocalScale = new float3(1f, 1f, 1f);`
- `CubeTestSpinComponent` is still attached
- the shared instruction/UI path is present again
- `UseDefaultBottomOverlay = false`
- no ground helper is present

Use negative assertions for drift-prone content such as ground creation or DS-specific overlay content.

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj' --filter "FullyQualifiedName~CityCubeTestSceneSourceTests" -clp:ErrorsOnly
```

Expected: FAIL because the current test still locks the temporary no-UI scene contract and scale `2,2,2`.

- [ ] **Step 3: Commit the failing test only if working in an isolated branch**

```bash
git add C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs
git commit -m "test: lock cube_test shared UI contract"
```

If the branch already contains in-progress work and a red test would be disruptive, skip this commit and continue immediately to Task 2.

### Task 2: Restore the Shared Instruction UI in the Authored Scene

**Files:**
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs`
- Reference: `C:\dev\helprojs\city\assets\codebase\rendering\CubeTestSpinComponent.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`

- [ ] **Step 1: Restore the shared instruction/UI path**

Edit `CubeTestSceneFactory.cs` so the scene contains:

- one camera entity
- one directional light entity
- one cube mesh entity
- the normal shared instruction/UI content path used by other authored scenes

Do not add:

- ground
- extra meshes
- DS bottom overlay content
- Vita-specific framing adjustments

- [ ] **Step 2: Normalize the cube transform**

In `CreateCubeEntity(...)`, ensure the cube is authored exactly as:

```csharp
entity.LocalPosition = new float3(0f, 0f, 0f);
entity.LocalScale = new float3(1f, 1f, 1f);
entity.LocalOrientation = float4.Identity;
```

Keep the existing `CubeTestSpinComponent` attached. Do not change the camera or light unless required by the shared instruction/UI restoration path.

- [ ] **Step 3: Update the source test to match the restored contract**

Adjust `CityCubeTestSceneSourceTests.cs` so it asserts the restored UI path and the `1,1,1` cube scale while continuing to reject ground and DS overlay drift.

- [ ] **Step 4: Run the focused scene source test to verify it passes**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj' --filter "FullyQualifiedName~CityCubeTestSceneSourceTests" -clp:ErrorsOnly
```

Expected: PASS.

- [ ] **Step 5: Commit the scene contract restoration**

```bash
git add C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs
git commit -m "feat: restore cube_test shared instruction UI"
```

### Task 3: Regenerate the Authored Cube-Test Asset

**Files:**
- Regenerate: `city` committed `cube_test` scene asset produced by editor command
- Reference: `C:\dev\helprojs\city\project.heproj`

- [ ] **Step 1: Run the normal editor command**

Run:

```powershell
dotnet run --project 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\helengine.editor.app.csproj' -c Debug -- --project 'C:\dev\helprojs\city\project.heproj' --editor-command menu.generate-cube-test-scene
```

Expected: `Editor command 'menu.generate-cube-test-scene' executed successfully.`

- [ ] **Step 2: Inspect the generated asset only if regeneration fails**

If the command fails, inspect the source scene factory and any editor error output before making changes. Do not patch generated assets manually.

- [ ] **Step 3: Commit the regenerated asset if it changed**

```bash
git add C:\dev\helprojs\city
git commit -m "chore: regenerate cube_test scene asset"
```

Limit the commit to the regenerated scene asset and any directly related authored-scene outputs.

### Task 4: Validate Through the Normal PS Vita Build and Relaunch Flow

**Files:**
- Build output: `C:\tmp\city-psvita-cube-test-ui`
- Launch script: `tools/launch-vita3k.ps1`
- Diagnostic reference: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`

- [ ] **Step 1: Run the normal city PS Vita build**

Use the existing normal editor-driven build flow and place the output in:

```text
C:\tmp\city-psvita-cube-test-ui
```

Expected: build succeeds and emits `helengine_psvita.vpk`.

- [ ] **Step 2: Launch the rebuilt VPK**

Run the existing Vita launcher script against the new VPK:

```powershell
powershell -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1' 'C:\tmp\city-psvita-cube-test-ui\helengine_psvita.vpk'
```

Expected: the script force-stops old Vita3K processes, deletes `HLEN00001`, and launches the fresh package.

- [ ] **Step 3: Check only the minimum evidence needed**

Verify:

- the scene still loads as `cube_test`
- the rotating cube is still being submitted
- the shared instruction/UI layer is present again

Use the smallest available evidence source:

- focused log inspection
- a single Vita3K screenshot
- existing temporary renderer diagnostics if already present

Do not broaden this step into a new camera-parity implementation unless the scene contract itself is still wrong.

- [ ] **Step 4: Commit only if this task required code or script changes**

```bash
git status --short
```

If Task 4 only produced build outputs or logs, do not create a commit here.
