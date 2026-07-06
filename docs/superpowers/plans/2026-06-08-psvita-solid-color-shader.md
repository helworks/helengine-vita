# PS Vita Solid-Color Shader Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render `cube_test` through a real PS Vita `GXM` vertex/fragment shader pair using a solid color and GPU transform path.

**Architecture:** Keep the current Vita runtime model loading and scene traversal intact, but add a parallel direct `GXM` draw path for solid-color static meshes. `PsVitaRenderManager3D` remains responsible for camera/model traversal, while `PsVitaGxmRenderer` gains a dedicated solid-color draw entry point backed by one minimal shader program wrapper and one fixed vertex format.

**Tech Stack:** C++, VitaSDK GXM, CMake, existing Vita builder source-audit tests, Vita3K for runtime verification

---

### Task 1: Lock the shader milestone with source audits

**Files:**
- Create: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Test: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Write the failing GXM program audit**

```csharp
namespace builder.tests;

/// <summary>
/// Verifies the Vita renderer contains a dedicated solid-color shader program wrapper.
/// </summary>
public sealed class PsVitaGxmSolidColorProgramSourceAuditTests {
    /// <summary>
    /// Ensures the shader wrapper owns both vertex and fragment program handles.
    /// </summary>
    [Fact]
    public void Source_ContainsDedicatedSolidColorProgramHandles() {
        var source = File.ReadAllText(SourcePath("src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"));
        source.ShouldContain("class PsVitaGxmSolidColorProgram");
        source.ShouldContain("SceGxmVertexProgram*");
        source.ShouldContain("SceGxmFragmentProgram*");
    }

    /// <summary>
    /// Ensures the implementation binds compiled shader assets instead of routing through vita2d.
    /// </summary>
    [Fact]
    public void Source_CreatesVertexAndFragmentProgramsFromGxmShaders() {
        var source = File.ReadAllText(SourcePath("src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp"));
        source.ShouldContain("sceGxmShaderPatcherCreateVertexProgram");
        source.ShouldContain("sceGxmShaderPatcherCreateFragmentProgram");
        source.ShouldNotContain("vita2d_draw_array");
    }
}
```

- [ ] **Step 2: Write the failing render-manager audit**

```csharp
[Fact]
public void Source_RoutesMeshSubmissionThroughSolidColorGxmPath() {
    var source = File.ReadAllText(SourcePath("src/platform/psvita/rendering/PsVitaRenderManager3D.cpp"));
    source.ShouldContain("DrawSolidColorMesh");
    source.ShouldNotContain("SubmitTriangleStrip");
}
```

- [ ] **Step 3: Run the focused audit tests to verify failure**

Run:

```powershell
dotnet test builder.tests/builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" 2>&1 | Select-Object -First 120
```

Expected:

```text
FAIL ... Could not find file '...PsVitaGxmSolidColorProgram.hpp'
FAIL ... Expected source to contain "DrawSolidColorMesh"
```

- [ ] **Step 4: Commit the failing-test baseline**

```bash
git add builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs builder.tests/PsVitaRenderManager3DSourceAuditTests.cs
git commit -m "test: lock PS Vita solid color shader milestone"
```

### Task 2: Add the dedicated solid-color GXM program wrapper

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp`
- Create: `src/platform/psvita/rendering/shaders/PsVitaSolidColorVertex.cg`
- Create: `src/platform/psvita/rendering/shaders/PsVitaSolidColorFragment.cg`
- Modify: `CMakeLists.txt`
- Test: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`

- [ ] **Step 1: Add the minimal shader wrapper header**

```cpp
#pragma once

#include <psp2/gxm.h>

/// <summary>
/// Owns the fixed solid-color shader pair used by the first Vita programmable mesh path.
/// </summary>
class PsVitaGxmSolidColorProgram {
public:
    /// <summary>
    /// Initializes the compiled shader programs and caches the uniform parameter handles.
    /// </summary>
    void Initialize(SceGxmShaderPatcher* shaderPatcher);

    /// <summary>
    /// Releases all shader program objects created during initialization.
    /// </summary>
    void Shutdown(SceGxmShaderPatcher* shaderPatcher);

    /// <summary>
    /// Gets the created vertex program.
    /// </summary>
    SceGxmVertexProgram* GetVertexProgram() const;

    /// <summary>
    /// Gets the created fragment program.
    /// </summary>
    SceGxmFragmentProgram* GetFragmentProgram() const;

private:
    SceGxmVertexProgram* VertexProgram;
    SceGxmFragmentProgram* FragmentProgram;
    const SceGxmProgramParameter* WorldViewProjectionParameter;
    const SceGxmProgramParameter* SolidColorParameter;
};
```

- [ ] **Step 2: Add the minimal shader wrapper implementation**

```cpp
void PsVitaGxmSolidColorProgram::Initialize(SceGxmShaderPatcher* shaderPatcher) {
    auto vertexProgramId = sceGxmShaderPatcherRegisterProgram(shaderPatcher, g_PsVitaSolidColorVertexProgram);
    auto fragmentProgramId = sceGxmShaderPatcherRegisterProgram(shaderPatcher, g_PsVitaSolidColorFragmentProgram);

    sceGxmShaderPatcherCreateVertexProgram(
        shaderPatcher,
        vertexProgramId,
        g_PsVitaSolidColorVertexAttributes,
        g_PsVitaSolidColorVertexStreams,
        &VertexProgram);

    sceGxmShaderPatcherCreateFragmentProgram(
        shaderPatcher,
        fragmentProgramId,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE,
        nullptr,
        g_PsVitaSolidColorVertexProgram,
        &FragmentProgram);
}
```

- [ ] **Step 3: Add the first fixed shader sources**

```c
float4 main(float3 position : POSITION, uniform float4x4 WorldViewProjection) : POSITION {
    return mul(WorldViewProjection, float4(position, 1.0f));
}
```

```c
float4 main(uniform float4 SolidColor) : COLOR {
    return SolidColor;
}
```

- [ ] **Step 4: Wire shader compilation into CMake**

```cmake
set(PSVITA_SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-shaders)

add_custom_command(
    OUTPUT ${PSVITA_SHADER_OUTPUT_DIR}/PsVitaSolidColorVertex.gxp
    COMMAND psp2cgc -profile sce_vp_psp2 ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/psvita/rendering/shaders/PsVitaSolidColorVertex.cg ${PSVITA_SHADER_OUTPUT_DIR}/PsVitaSolidColorVertex.gxp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/psvita/rendering/shaders/PsVitaSolidColorVertex.cg)
```

- [ ] **Step 5: Run the new source-audit tests to verify they pass**

Run:

```powershell
dotnet test builder.tests/builder.tests.csproj --filter FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests 2>&1 | Select-Object -First 120
```

Expected:

```text
Passed! 2 test(s) passed.
```

- [ ] **Step 6: Commit the shader wrapper**

```bash
git add src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp src/platform/psvita/rendering/shaders/PsVitaSolidColorVertex.cg src/platform/psvita/rendering/shaders/PsVitaSolidColorFragment.cg CMakeLists.txt builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs
git commit -m "feat: add PS Vita solid color shader program"
```

### Task 3: Add a direct GXM solid-color mesh draw entry point

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`
- Test: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`

- [ ] **Step 1: Extend the renderer interface with a mesh draw API**

```cpp
/// <summary>
/// Draws a static indexed mesh through the minimal solid-color shader path.
/// </summary>
void DrawSolidColorMesh(
    const float4x4& worldViewProjection,
    const float4& solidColor,
    const float3* positions,
    int32_t positionCount,
    const uint16_t* indices,
    int32_t indexCount);
```

- [ ] **Step 2: Add the renderer implementation and program ownership**

```cpp
void PsVitaGxmRenderer::DrawSolidColorMesh(
    const float4x4& worldViewProjection,
    const float4& solidColor,
    const float3* positions,
    int32_t positionCount,
    const uint16_t* indices,
    int32_t indexCount) {
    BindSolidColorProgram();
    UploadSolidColorUniforms(worldViewProjection, solidColor);
    UploadSolidColorVertices(positions, positionCount);
    sceGxmDraw(
        Context,
        SCE_GXM_PRIMITIVE_TRIANGLES,
        SCE_GXM_INDEX_FORMAT_U16,
        indices,
        indexCount);
}
```

- [ ] **Step 3: Keep the old path available during bring-up**

```cpp
if (!SolidColorProgramIsReady()) {
    DrawProjectedWhiteTrianglesFallback(...);
    return;
}
```

- [ ] **Step 4: Run the focused shader source audits**

Run:

```powershell
dotnet test builder.tests/builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests" 2>&1 | Select-Object -First 120
```

Expected:

```text
Passed! 2 test(s) passed.
```

- [ ] **Step 5: Commit the direct mesh draw entry point**

```bash
git add src/platform/psvita/rendering/PsVitaGxmRenderer.hpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp
git commit -m "feat: add PS Vita solid color mesh draw path"
```

### Task 4: Route `cube_test` mesh submission through the new shader path

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Replace CPU-projected mesh submission with solid-color mesh submission**

```cpp
Renderer->DrawSolidColorMesh(
    worldViewProjection,
    float4(1.0f, 1.0f, 1.0f, 1.0f),
    runtimeSubmesh->Positions.data(),
    static_cast<int32_t>(runtimeSubmesh->Positions.size()),
    runtimeSubmesh->TriangleIndices.data(),
    static_cast<int32_t>(runtimeSubmesh->TriangleIndices.size()));
```

- [ ] **Step 2: Remove the old direct dependency on per-triangle CPU projection for the cube path**

```cpp
source.ShouldNotContain("TryProjectTriangle");
source.ShouldContain("DrawSolidColorMesh");
```

- [ ] **Step 3: Run the render-manager source audits**

Run:

```powershell
dotnet test builder.tests/builder.tests.csproj --filter FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests 2>&1 | Select-Object -First 120
```

Expected:

```text
Passed! 8 test(s) passed.
```

- [ ] **Step 4: Commit the render-manager routing change**

```bash
git add src/platform/psvita/rendering/PsVitaRenderManager3D.hpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp builder.tests/PsVitaRenderManager3DSourceAuditTests.cs
git commit -m "feat: route cube_test through PS Vita solid color shader"
```

### Task 5: Verify the end-to-end Vita build and runtime result

**Files:**
- Modify: `docs/superpowers/specs/2026-06-08-psvita-solid-color-shader-design.md`
- Test: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`
- Test: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Run the focused managed test suite**

Run:

```powershell
dotnet test builder.tests/builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" 2>&1 | Select-Object -First 120
```

Expected:

```text
Passed! All requested tests passed.
```

- [ ] **Step 2: Run a normal city PS Vita build**

Run:

```powershell
dotnet run --project C:\dev\helworks\helengine\helengine.editor\helengine.editor.app\helengine.editor.app.csproj -- build-psvita-city-normal C:\tmp\city-psvita-solid-color-shader 2>&1 | Select-Object -First 160
```

Expected:

```text
Build succeeded
... helengine_psvita.vpk
```

- [ ] **Step 3: Launch the fresh VPK through the updated launcher**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/launch-vita3k.ps1 C:\tmp\city-psvita-solid-color-shader\helengine_psvita.vpk 2>&1 | Select-Object -First 120
```

Expected:

```text
Stopped running Vita3K process
Deleted title HLEN00001
Started Vita3K with helengine_psvita.vpk
```

- [ ] **Step 4: Update the spec status note once verification is complete**

```markdown
## Status

- direct solid-color GXM path implemented
- `cube_test` routed through the programmable path
- verified through normal build and Vita3K launch
```

- [ ] **Step 5: Commit the verification note**

```bash
git add docs/superpowers/specs/2026-06-08-psvita-solid-color-shader-design.md
git commit -m "docs: record PS Vita solid color shader verification"
```
