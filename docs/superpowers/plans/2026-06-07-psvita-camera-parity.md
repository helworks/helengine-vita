# PS Vita Camera Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore PS Vita 3D camera math so `cube_test` and other authored scenes use the same camera contract as the engine's working renderers.

**Architecture:** Treat the engine's existing DirectX11 and Vulkan renderers as the camera-contract reference, then bring `PsVitaRenderManager3D` back into parity by fixing basis selection and only then adjusting the manual projection path if needed. Lock the contract with Vita source audits before rebuilding and relaunching the normal city PS Vita package.

**Tech Stack:** C++ Vita renderer code, PowerShell build/launch scripts, xUnit source-audit tests, Vita3K

---

### Task 1: Lock the Engine Camera Contract in Vita Source Audits

**Files:**
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Add the failing forward-basis parity audit**

Add this test to `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`:

```csharp
    /// <summary>
    /// Verifies the Vita camera path uses the same default forward basis as the engine's normal 3D renderers.
    /// </summary>
    [Fact]
    public void Source_whenBuildingCameraViewProjection_usesEngineNegativeZForwardBasis() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, 1.0f), cameraOrientation);", sourceCode, StringComparison.Ordinal);
    }
```

- [ ] **Step 2: Add the matrix-composition parity audit**

Add this test to the same file:

```csharp
    /// <summary>
    /// Verifies the Vita renderer keeps the same view-projection and world-view-projection composition order used by working engine renderers.
    /// </summary>
    [Fact]
    public void Source_whenProjectingVitaMeshes_matchesEngineMatrixCompositionOrder() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("float4x4::Multiply__ref0_ref1_out2(view, projection, viewProjection);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float4x4::Multiply__ref0_ref1_out2(world, ActiveViewProjection, worldViewProjection);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("CameraProjectionUtils::CreatePerspectiveProjection(", sourceCode, StringComparison.Ordinal);
    }
```

- [ ] **Step 3: Run the focused audit to verify failure**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" -clp:ErrorsOnly
```

Expected: FAIL because the current Vita camera forward basis still uses `+Z`.

### Task 2: Restore the Vita Camera Basis to Engine Parity

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Restore the engine forward basis**

Change this line in `PsVitaRenderManager3D.cpp`:

```cpp
        ::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, 1.0f), cameraOrientation);
```

to:

```cpp
        ::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);
```

Leave the up vector and the existing `view * projection` composition unchanged:

```cpp
        ::float3 cameraUp = float4::RotateVector(::float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        ::float3 cameraTarget = cameraPosition + cameraForward;
```

- [ ] **Step 2: Re-run the focused source audit**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" -clp:ErrorsOnly
```

Expected: PASS if the remaining parity contract is already correct.

- [ ] **Step 3: If the audit passes, stop changing camera convention code**

Do not add Vita-only offsets, viewport tweaks, authored-scene changes, or basis flips elsewhere. Move straight to runtime verification.

### Task 3: Verify Runtime Framing Against the Authored Scene

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp` only if runtime evidence proves the manual projection path is still wrong after Task 2
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Test: runtime build output at `C:\tmp\city-psvita-camera-parity`

- [ ] **Step 1: Build the normal city PS Vita package**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 'psvita' -Output 'C:\tmp\city-psvita-camera-parity'
```

Expected: build completes successfully and writes `C:\tmp\city-psvita-camera-parity\helengine_psvita.vpk`.

- [ ] **Step 2: Launch the fresh Vita package**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1' -VpkPath 'C:\tmp\city-psvita-camera-parity\helengine_psvita.vpk'
```

Expected: Vita3K force-stops prior instances, deletes `HLEN00001`, and launches the new VPK.

- [ ] **Step 3: Compare the Vita framing against authored expectations**

Use the authored `cube_test` camera from `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs` as the truth:

- camera position `new float3(7f, 4.5f, 7f)`
- camera orientation from `CreateFromYawPitchRoll(-2.35619449f, -0.27925268f, 0f, out orientation)`
- fullscreen viewport `new float4(0f, 0f, 1f, 1f)`

Expected visual result: the elevated cube should appear near the authored screen center rather than low on the screen and undersized.

- [ ] **Step 4: Only if framing is still wrong, instrument the manual projection path minimally**

If runtime framing remains wrong after restoring `-Z`, add one temporary diagnostic around `TryProjectToScreen(...)` and inspect:

```cpp
        const float normalizedX = clipX * inverseW;
        const float normalizedY = clipY * inverseW;
        const float normalizedZ = clipZ * inverseW;
```

Then verify whether the issue is:

- incorrect manual clip-space interpretation
- incorrect Y inversion
- incorrect world-to-clip multiplication usage

Do not change authored scene data.

### Task 4: Apply the Minimal Projection Fix If Runtime Evidence Requires It

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs` only if a new stable source contract must be locked
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Test: runtime build output at `C:\tmp\city-psvita-camera-parity`

- [ ] **Step 1: Implement one projection-path correction only if Task 3 proves it is necessary**

Possible fix area:

```cpp
    bool PsVitaRenderManager3D::TryProjectToScreen(
        const ::float3& point,
        const ::float4x4& worldViewProjection,
        const ::float4& viewport,
        ::float3& projectedPoint) {
```

Only change the part that the Task 3 evidence proves is wrong. Keep:

- the restored `-Z` camera basis
- `view * projection`
- `world * ActiveViewProjection`

- [ ] **Step 2: Re-run the focused source audit**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" -clp:ErrorsOnly
```

Expected: PASS.

- [ ] **Step 3: Rebuild and relaunch the city Vita package**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 'psvita' -Output 'C:\tmp\city-psvita-camera-parity'
```

Then:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1' -VpkPath 'C:\tmp\city-psvita-camera-parity\helengine_psvita.vpk'
```

Expected: `cube_test` framing is visibly closer to the authored centered composition.

- [ ] **Step 4: Remove any temporary diagnostics introduced in Task 3**

If you added temporary tracing, delete it before finishing so the committed renderer keeps only stable parity code.

### Task 5: Commit and Report Worktree State

**Files:**
- Modify: whichever files changed from Tasks 1-4
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Run the final focused audit set**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --no-build --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" -clp:ErrorsOnly
```

Expected: PASS.

- [ ] **Step 2: Commit the camera-parity fix**

```bash
git add builder.tests/PsVitaRenderManager3DSourceAuditTests.cs src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
git commit -m "Restore PS Vita camera parity"
```

If Task 4 required a stable projection-path contract, include that updated test coverage in the same commit.

- [ ] **Step 3: Report remaining worktree state**

Run:

```bash
git status --short --branch
```

Expected: the camera-parity fix commit exists and any unrelated pre-existing Vita worktree changes remain untouched.
