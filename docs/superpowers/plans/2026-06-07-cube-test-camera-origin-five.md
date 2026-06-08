# Cube Test Camera Origin-Five Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set `cube_test` to start from camera position `(0,0,5)` looking at the origin while preserving manual orbit controls and disabling automatic orbit drift.

**Architecture:** Keep the existing `DemoDiscOrbitCameraComponent`, but correct the authored initial pose and set `AutoYawSpeedRadians = 0f` so the runtime orbit system derives the intended static baseline. Lock the camera contract in the city scene source test, regenerate the authored asset, then verify through the normal Vita build and relaunch flow.

**Tech Stack:** C# scene authoring, xUnit source tests, helengine editor command pipeline, PS Vita builder, PowerShell launch script

---

### Task 1: Lock the Authored Camera Contract in Source Tests

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`

- [ ] **Step 1: Update the source test to require the new camera pose**

Assert all of the following in `CityCubeTestSceneSourceTests.cs`:

- `entity.LocalPosition = new float3(0f, 0f, 5f);`
- authored camera orientation creation for a forward-looking origin view
- `OrbitCenter = float3.Zero`
- `AutoYawSpeedRadians = 0f`

Keep the existing one-cube, shared UI, and no-ground assertions.

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj' --filter "FullyQualifiedName~CityCubeTestSceneSourceTests" -clp:ErrorsOnly
```

Expected: FAIL because the current authored camera position and orbit settings do not yet match `(0,0,5)` with zero auto-yaw.

### Task 2: Correct the Authored Cube-Test Camera

**Files:**
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`

- [ ] **Step 1: Change the camera position and orientation**

Update `CreateCameraEntity()` and its orientation helper so the camera starts at:

```csharp
entity.LocalPosition = new float3(0f, 0f, 5f);
```

and looks at the origin from that pose.

- [ ] **Step 2: Keep orbit controls but disable automatic yaw**

Keep `DemoDiscOrbitCameraComponent`, keep `OrbitCenter = float3.Zero`, and set:

```csharp
AutoYawSpeedRadians = 0f
```

- [ ] **Step 3: Run the focused source test to verify it passes**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj' --filter "FullyQualifiedName~CityCubeTestSceneSourceTests" -clp:ErrorsOnly
```

Expected: PASS.

### Task 3: Regenerate and Validate Through the Normal Vita Flow

**Files:**
- Regenerate: `C:\dev\helprojs\city\assets\scenes\rendering\cube_test.helen`
- Build output: `C:\tmp\city-psvita-camera-origin-five`

- [ ] **Step 1: Regenerate the authored cube-test asset**

Run:

```powershell
dotnet run --project 'C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\helengine.editor.app.csproj' -c Debug -- --project 'C:\dev\helprojs\city\project.heproj' --editor-command menu.generate-cube-test-scene
```

- [ ] **Step 2: Run the normal PS Vita build**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform psvita -Output 'C:\tmp\city-psvita-camera-origin-five'
```

- [ ] **Step 3: Relaunch the finished VPK**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1' -VpkPath 'C:\tmp\city-psvita-camera-origin-five\helengine_psvita.vpk'
```

- [ ] **Step 4: Check the minimum runtime evidence**

Verify with one screenshot or filtered boot-log lines that:

- the cube is still rendering
- the first-frame camera starts from the new `(0,0,5)` baseline
- auto-orbit no longer drifts immediately
