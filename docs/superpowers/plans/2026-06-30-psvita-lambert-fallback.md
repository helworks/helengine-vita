# PS Vita Lambert Fallback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the emulator-safe PS Vita 3D fallback render Lambert-lit triangles using existing directional and ambient scene lights.

**Architecture:** Keep the current CPU-projected 3D path and upgrade it from white-only triangles to colored solid-color vertices. Extend the Vita runtime model to carry normals, resolve lighting in `PsVitaRenderManager3D`, and flush the lit vertices through the existing `SubmitSolidColorTriangles(...)` path so Vita3K remains stable.

**Tech Stack:** C++, VitaSDK, vita2d, xUnit source audits in `helengine.editor.tests`, normal editor-driven PS Vita build, Vita3K runtime launch

---

## File Structure

- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\PsVitaLambertFallbackSourceTests.cs`
  - Cross-repo source audit for the Lambert fallback slice.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.hpp`
  - Add copied normal storage to the Vita runtime model.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.cpp`
  - Implement the new normal-carrying constructor and accessor.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.hpp`
  - Replace the white-triangle queue with a colored-vertex queue and declare lighting helpers.
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp`
  - Copy normals from `ModelAsset`, resolve directional and ambient lights, compute Lambert colors, and flush through `SubmitSolidColorTriangles(...)`.

## Task 1: Lock the Lambert Fallback Contract With a Failing Source Audit

**Files:**
- Create: `C:\dev\helworks\helengine\engine\helengine.editor.tests\PsVitaLambertFallbackSourceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj`

- [ ] **Step 1: Write the failing source audit**

Create `PsVitaLambertFallbackSourceTests.cs` with this content:

```csharp
namespace helengine.editor.tests;

/// <summary>
/// Verifies the PS Vita emulator-safe 3D fallback path upgrades from white triangles to Lambert-lit colored vertices.
/// </summary>
public sealed class PsVitaLambertFallbackSourceTests {
    /// <summary>
    /// Ensures the Vita runtime model copies authored normals alongside positions.
    /// </summary>
    [Fact]
    public void PsVita_runtime_model_copies_normals_for_lambert_fallback() {
        string headerPath = @"C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.hpp";
        string sourcePath = @"C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.cpp";
        string headerSource = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("explicit PsVitaRuntimeModel(std::vector<::float3> positions, std::vector<::float3> normals);", headerSource, StringComparison.Ordinal);
        Assert.Contains("const std::vector<::float3>& GetNormals() const;", headerSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<::float3> Normals;", headerSource, StringComparison.Ordinal);
        Assert.Contains("GetNormals", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Vita 3D fallback resolves scene lights and submits colored solid-color vertices instead of white-only mesh triangles.
    /// </summary>
    [Fact]
    public void PsVita_render_manager_uses_existing_lights_and_colored_vertices_for_lambert_fallback() {
        string headerPath = @"C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.hpp";
        string sourcePath = @"C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp";
        string headerSource = File.ReadAllText(headerPath);
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("std::vector<rendering::PsVitaSolidColorVertex> QueuedMeshTriangles;", headerSource, StringComparison.Ordinal);
        Assert.Contains("DirectionalLightComponent", source, StringComparison.Ordinal);
        Assert.Contains("AmbientLightComponent", source, StringComparison.Ordinal);
        Assert.Contains("SubmitSolidColorTriangles(QueuedMeshTriangles);", source, StringComparison.Ordinal);
        Assert.Contains("GetNormals()", source, StringComparison.Ordinal);
        Assert.Contains("ResolveActiveDirectionalLight", source, StringComparison.Ordinal);
        Assert.Contains("ResolveAmbientLightColor", source, StringComparison.Ordinal);
        Assert.Contains("BuildLambertVertexColor", source, StringComparison.Ordinal);
        Assert.DoesNotContain("SubmitSolidWhiteMeshTriangles(QueuedMeshTriangles);", source, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 2: Run the source audit to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj -v q --filter "FullyQualifiedName~PsVitaLambertFallbackSourceTests"
```

Expected:

```text
FAIL because PsVitaRuntimeModel does not yet expose normals and PsVitaRenderManager3D still queues white mesh triangles.
```

- [ ] **Step 3: Commit the failing test**

```bash
git -C C:\dev\helworks\helengine add engine/helengine.editor.tests/PsVitaLambertFallbackSourceTests.cs
git -C C:\dev\helworks\helengine commit -m "test: lock PS Vita lambert fallback source contract"
```

## Task 2: Extend the Vita Runtime Model to Carry Authored Normals

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.hpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRuntimeModel.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj`

- [ ] **Step 1: Update `PsVitaRuntimeModel.hpp` to store normals**

Change the class shape to include normals:

```cpp
/// Creates one PS Vita runtime model with copied positions and normals for the Lambert fallback renderer.
explicit PsVitaRuntimeModel(std::vector<::float3> positions, std::vector<::float3> normals);

/// Gets the copied model-space normals owned by this runtime model.
const std::vector<::float3>& GetNormals() const;

/// Stores the copied model-space normals used by the PS Vita Lambert fallback renderer.
std::vector<::float3> Normals;
```

- [ ] **Step 2: Update `PsVitaRuntimeModel.cpp` to implement the new constructor and accessor**

Implement:

```cpp
PsVitaRuntimeModel::PsVitaRuntimeModel(std::vector<::float3> positions, std::vector<::float3> normals)
    : Positions(std::move(positions))
    , Normals(std::move(normals))
    , Submeshes(nullptr) {
}

const std::vector<::float3>& PsVitaRuntimeModel::GetNormals() const {
    return Normals;
}
```

- [ ] **Step 3: Copy normals in `BuildModelFromRaw(...)`**

In `PsVitaRenderManager3D.cpp`, add a copied normal array beside positions:

```cpp
std::vector<::float3> copiedNormals;
if (data->Normals != nullptr && data->Normals->Length == data->Positions->Length) {
    copiedNormals.reserve(static_cast<std::size_t>(data->Normals->Length));
    for (int32_t normalIndex = 0; normalIndex < data->Normals->Length; ++normalIndex) {
        copiedNormals.push_back((*data->Normals)[normalIndex]);
    }
}

auto* runtimeModel = new rendering::PsVitaRuntimeModel(std::move(copiedPositions), std::move(copiedNormals));
```

- [ ] **Step 4: Re-run the source audit**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj -v q --filter "FullyQualifiedName~PsVitaLambertFallbackSourceTests.PsVita_runtime_model_copies_normals_for_lambert_fallback"
```

Expected:

```text
PASS
```

- [ ] **Step 5: Commit the runtime-model change**

```bash
git add src/platform/psvita/rendering/PsVitaRuntimeModel.hpp src/platform/psvita/rendering/PsVitaRuntimeModel.cpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
git commit -m "feat: copy normals into PS Vita runtime models"
```

## Task 3: Replace White Mesh Vertices With Lambert-Lit Colored Vertices

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.hpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaRenderManager3D.cpp`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj`

- [ ] **Step 1: Change the queued 3D fallback vertex storage**

In `PsVitaRenderManager3D.hpp`, replace the white queue and declare helpers:

```cpp
static ::DirectionalLightComponent* ResolveActiveDirectionalLight();
static ::float3 ResolveDirectionalLightDirection(::DirectionalLightComponent* lightComponent);
static ::float3 ResolveAmbientLightColor();
static std::uint32_t ResolveLambertBaseColor(::MeshComponent* meshComponent, int32_t submeshIndex);
static std::uint32_t BuildLambertVertexColor(
    std::uint32_t baseColorAbgr,
    const ::float3& worldNormal,
    const ::float3& lightDirection,
    const ::float3& ambientLightColor,
    bool hasDirectionalLight);

std::vector<rendering::PsVitaSolidColorVertex> QueuedMeshTriangles;
```

- [ ] **Step 2: Flush through the existing solid-color path instead of the white-only mesh path**

In `DrawCamera(...)`, replace:

```cpp
GxmRenderer->SubmitSolidWhiteMeshTriangles(QueuedMeshTriangles);
```

with:

```cpp
GxmRenderer->SubmitSolidColorTriangles(QueuedMeshTriangles);
```

- [ ] **Step 3: Implement the lighting helpers in `PsVitaRenderManager3D.cpp`**

Add the smallest viable helpers:

```cpp
DirectionalLightComponent* PsVitaRenderManager3D::ResolveActiveDirectionalLight() {
    if (Core::Instance == nullptr || Core::Instance->ObjectManager == nullptr || Core::Instance->ObjectManager->DirectionalLights == nullptr) {
        return nullptr;
    }

    List<DirectionalLightComponent*>* directionalLights = Core::Instance->ObjectManager->DirectionalLights;
    for (int32_t lightIndex = 0; lightIndex < directionalLights->get_Count(); ++lightIndex) {
        DirectionalLightComponent* light = (*directionalLights)[lightIndex];
        if (light != nullptr && light->get_Parent() != nullptr && light->get_Intensity() > 0.0f) {
            return light;
        }
    }

    return nullptr;
}

float3 PsVitaRenderManager3D::ResolveDirectionalLightDirection(DirectionalLightComponent* lightComponent) {
    float3 forward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), lightComponent->get_Parent()->get_Orientation());
    return float3::Normalize(forward);
}

float3 PsVitaRenderManager3D::ResolveAmbientLightColor() {
    float3 ambient(0.0f, 0.0f, 0.0f);
    // accumulate AmbientLights here
    return ambient;
}
```

- [ ] **Step 4: Replace white triangle queuing with Lambert-colored vertex queuing**

Inside `DrawRuntimeModel(...)`, compute world-space positions, normals, and colors before projection:

```cpp
const std::vector<::float3>& normals = runtimeModel->GetNormals();
DirectionalLightComponent* directionalLight = ResolveActiveDirectionalLight();
float3 ambientLightColor = ResolveAmbientLightColor();
bool hasDirectionalLight = directionalLight != nullptr;
float3 lightDirection = hasDirectionalLight ? ResolveDirectionalLightDirection(directionalLight) : float3(0.0f, 0.0f, 0.0f);

uint32_t baseColorAbgr = ResolveLambertBaseColor(meshComponent, submeshIndex);

QueuedMeshTriangles.push_back(rendering::PsVitaSolidColorVertex {
    projectedVertex0.X,
    projectedVertex0.Y,
    projectedVertex0.Z,
    BuildLambertVertexColor(baseColorAbgr, worldNormal0, lightDirection, ambientLightColor, hasDirectionalLight)
});
```

If `normals.size() != positions.size()`, use a normalized face normal for all three vertices of the triangle.

- [ ] **Step 5: Make missing-light scenes stay visible**

In `BuildLambertVertexColor(...)`, clamp lighting so “no lights” returns the base color rather than black:

```cpp
float lambert = hasDirectionalLight ? std::max(0.0f, float3::Dot(float3::Normalize(worldNormal), -lightDirection)) : 1.0f;
float3 lighting = ambientLightColor + float3(lambert, lambert, lambert);
lighting = float3(
    std::min(1.0f, lighting.X),
    std::min(1.0f, lighting.Y),
    std::min(1.0f, lighting.Z));
```

- [ ] **Step 6: Re-run the full source audit**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj -v q --filter "FullyQualifiedName~PsVitaLambertFallbackSourceTests"
```

Expected:

```text
PASS
```

- [ ] **Step 7: Commit the Lambert fallback implementation**

```bash
git add src/platform/psvita/rendering/PsVitaRenderManager3D.hpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
git commit -m "feat: add lambert lighting to PS Vita fallback renderer"
```

## Task 4: Build and Verify the Stopgap in Vita3K

**Files:**
- No additional source files expected

- [ ] **Step 1: Run the source audit again as a clean verification gate**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj -v q --filter "FullyQualifiedName~PsVitaLambertFallbackSourceTests"
```

Expected:

```text
PASS
```

- [ ] **Step 2: Build the city PS Vita output**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform psvita -Output C:\dev\helprojs\output\psvita-2026-06-30-lambert
```

Expected:

```text
Build completed for platform 'psvita'
```

- [ ] **Step 3: Launch in Vita3K without re-enabling the runtime shader compiler path**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-psvita\tools\launch-vita3k.ps1 -VpkPath C:\dev\helprojs\output\psvita-2026-06-30-lambert\helengine_psvita.vpk
```

Expected:

```text
The app reaches MainMenu/cube_test without the libshacccg crash path, and the visible cube is directionally shaded instead of flat white.
```

- [ ] **Step 4: Capture the boot-log evidence**

Run:

```powershell
rtk powershell -NoProfile -Command "Get-Content 'C:\Users\Helena\AppData\Roaming\Vita3K\Vita3K\ux0\data\helengine_psvita_boot.log' -Tail 80"
```

Expected:

```text
RunMainLoop continues past first draw3d, with no libshacccg-triggered crash and visible frame progression.
```

- [ ] **Step 5: Commit the verification-complete slice**

```bash
git add src/platform/psvita/rendering/PsVitaRuntimeModel.hpp src/platform/psvita/rendering/PsVitaRuntimeModel.cpp src/platform/psvita/rendering/PsVitaRenderManager3D.hpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
git commit -m "feat: light PS Vita fallback meshes with lambert shading"
```
