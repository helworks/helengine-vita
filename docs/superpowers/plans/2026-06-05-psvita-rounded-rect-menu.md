# PS Vita Rounded-Rect Menu Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render the current menu's filled rounded UI panels on PS Vita through the real editor CLI build path.

**Architecture:** Keep the existing Vita2D-backed renderer and extend it with one solid-color triangle path. `PsVitaRenderManager2D` will tessellate filled rounded rectangles into queued solid-color triangles, flush those before textured quads, and rely on the existing editor-built startup/menu runtime path for verification.

**Tech Stack:** C++20, Vita2D, VitaSDK, CMake, .NET 9 test runner, Helengine editor CLI, Vita3K

---

## File Structure

- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`
  Purpose: lock in the rounded-rect renderer shape before implementation.
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
  Purpose: declare one solid-color triangle submission entrypoint.
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`
  Purpose: submit solid-color transient triangle geometry through Vita2D.
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
  Purpose: add queued solid-color triangle state and rounded-rect helpers.
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`
  Purpose: tessellate filled rounded rectangles and flush them before textured geometry.

## Verification Prerequisites

- test project: `builder.tests\helengine.psvita.builder.tests.csproj`
- editor CLI: `C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll`
- project for end-to-end verification: `C:\dev\helprojs\city\project.heproj`
- editor output path: `C:\tmp\city-psvita-main-menu-cli`
- Vita3K: `C:\dev\helworks\emus\vita-3k\Vita3K.exe`

Use `C:\tmp` for test intermediates to avoid local bin locking.

### Task 1: Lock The Rounded-Rect Renderer Shape

**Files:**
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Add the failing rounded-rect audit**

Add one new test that checks:

```csharp
[Fact]
public void Source_whenRenderingRoundedRectMenuPanels_containsSolidColorTrianglePath() {
    string renderManagerHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
    string renderManagerSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
    string gxmRendererHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.hpp");
    string gxmRendererSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");

    string renderManagerHeaderSource = File.ReadAllText(renderManagerHeaderPath);
    string renderManagerSource = File.ReadAllText(renderManagerSourcePath);
    string gxmRendererHeaderSource = File.ReadAllText(gxmRendererHeaderPath);
    string gxmRendererSource = File.ReadAllText(gxmRendererSourcePath);

    Assert.Contains("std::vector<rendering::PsVitaSolidColorVertex>", renderManagerHeaderSource, StringComparison.Ordinal);
    Assert.Contains("void SubmitSolidColorTriangles", gxmRendererHeaderSource, StringComparison.Ordinal);
    Assert.Contains("DrawRoundedRect(::IRoundedRectDrawable2D* shape)", renderManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("void PsVitaRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {\r\n    }", renderManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("void PsVitaRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {\n    }", renderManagerSource, StringComparison.Ordinal);
    Assert.Contains("vita2d_draw_array", gxmRendererSource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused audit to verify it fails**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~Source_whenRenderingRoundedRectMenuPanels_containsSolidColorTrianglePath -p:BaseOutputPath=C:\tmp\psvita-roundedrect-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-roundedrect-audit-obj\
```

Expected: FAIL because the solid-color queue and renderer entrypoint do not exist yet.

- [ ] **Step 3: Commit the failing audit**

Run:

```bash
git add -- builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs
git commit -m "Add PS Vita rounded-rect renderer source audits"
```

### Task 2: Add The Solid-Color Renderer Path

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`

- [ ] **Step 1: Declare the solid-color submission surface**

Add the public entrypoint:

```cpp
void SubmitSolidColorTriangles(const std::vector<PsVitaSolidColorVertex>& vertices);
```

Add any minimal private helper only if needed to keep `SubmitSolidColorTriangles(...)` readable.

- [ ] **Step 2: Add the minimal solid-color draw implementation**

Implement the path with Vita2D transient memory and color vertices:

```cpp
void PsVitaGxmRenderer::SubmitSolidColorTriangles(const std::vector<PsVitaSolidColorVertex>& vertices) {
    if (!Initialized || !FrameBegun || vertices.empty()) {
        return;
    }

    vita2d_color_vertex* drawVertices = static_cast<vita2d_color_vertex*>(vita2d_pool_memalign(
        static_cast<unsigned int>(sizeof(vita2d_color_vertex) * vertices.size()),
        8u));
    if (drawVertices == nullptr) {
        throw std::runtime_error("PS Vita rounded-rect submission failed to allocate transient GPU-visible vertex memory.");
    }

    for (std::size_t index = 0; index < vertices.size(); ++index) {
        drawVertices[index].x = vertices[index].PositionX;
        drawVertices[index].y = vertices[index].PositionY;
        drawVertices[index].z = 0.5f;
        drawVertices[index].color = vertices[index].ColorAbgr;
    }

    vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, drawVertices, vertices.size());
}
```

- [ ] **Step 3: Re-run the focused audit**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~Source_whenRenderingRoundedRectMenuPanels_containsSolidColorTrianglePath -p:BaseOutputPath=C:\tmp\psvita-roundedrect-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-roundedrect-audit-obj\
```

Expected: still FAIL because `PsVitaRenderManager2D` does not yet define the solid-color queue or implement `DrawRoundedRect(...)`.

- [ ] **Step 4: Commit the renderer extension**

Run:

```bash
git add -- src/platform/psvita/rendering/PsVitaGxmRenderer.hpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp
git commit -m "Add PS Vita solid-color triangle renderer path"
```

### Task 3: Add Rounded-Rect Queue State To The 2D Manager

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`

- [ ] **Step 1: Add the minimal solid-color vertex type and queue state**

Add one focused vertex type near the renderer declarations:

```cpp
namespace helengine::psvita::rendering {
    struct PsVitaSolidColorVertex final {
        float PositionX;
        float PositionY;
        std::uint32_t ColorAbgr;
        std::uint8_t RenderOrder;
    };
}
```

Store queued rounded-rect geometry:

```cpp
std::vector<rendering::PsVitaSolidColorVertex> QueuedSolidColorTriangles;
```

Add private helpers:

```cpp
void AppendSolidTriangle(
    float x0,
    float y0,
    float x1,
    float y1,
    float x2,
    float y2,
    std::uint32_t colorAbgr,
    std::uint8_t renderOrder);

void AppendSolidQuad(
    float left,
    float top,
    float right,
    float bottom,
    std::uint32_t colorAbgr,
    std::uint8_t renderOrder);
```

- [ ] **Step 2: Flush solid-color triangles before textured quads**

Update `Draw()`:

```cpp
std::sort(QueuedSolidColorTriangles.begin(), QueuedSolidColorTriangles.end(), [](const rendering::PsVitaSolidColorVertex& left, const rendering::PsVitaSolidColorVertex& right) {
    return left.RenderOrder < right.RenderOrder;
});

if (GxmRenderer != nullptr && !QueuedSolidColorTriangles.empty()) {
    GxmRenderer->SubmitSolidColorTriangles(QueuedSolidColorTriangles);
}

QueuedSolidColorTriangles.clear();
```

Keep the existing textured-quad sorting and flush immediately after that block.

- [ ] **Step 3: Run the focused audit**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~Source_whenRenderingRoundedRectMenuPanels_containsSolidColorTrianglePath -p:BaseOutputPath=C:\tmp\psvita-roundedrect-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-roundedrect-audit-obj\
```

Expected: still FAIL because `DrawRoundedRect(...)` remains empty.

- [ ] **Step 4: Commit the queue state**

Run:

```bash
git add -- src/platform/psvita/rendering/PsVitaRenderManager2D.hpp src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Add PS Vita rounded-rect triangle queue"
```

### Task 4: Implement Filled Rounded-Rect Tessellation

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`

- [ ] **Step 1: Implement the filled rounded-rect body**

Replace the empty method with shape validation and clamped geometry:

```cpp
void PsVitaRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
    if (shape == nullptr || shape->get_Parent() == nullptr) {
        return;
    }

    int2 size = shape->get_Size();
    if (size.X <= 0 || size.Y <= 0) {
        return;
    }

    float3 position = shape->get_Parent()->get_Position();
    byte4 fillColor = shape->get_FillColor();
    std::uint32_t packedColor = (static_cast<std::uint32_t>(fillColor.W) << 24)
        | (static_cast<std::uint32_t>(fillColor.Z) << 16)
        | (static_cast<std::uint32_t>(fillColor.Y) << 8)
        | static_cast<std::uint32_t>(fillColor.X);

    float left = position.X;
    float top = position.Y;
    float right = position.X + static_cast<float>(size.X);
    float bottom = position.Y + static_cast<float>(size.Y);
    float radius = std::min(
        static_cast<float>(shape->get_BorderRadius()),
        std::min(static_cast<float>(size.X), static_cast<float>(size.Y)) * 0.5f);

    if (radius <= 0.0f) {
        AppendSolidQuad(left, top, right, bottom, packedColor, shape->get_RenderOrder2D());
        return;
    }

    // then append center, edge quads, and corner fans
}
```

- [ ] **Step 2: Add the fixed-segment corner fan tessellation**

Use a small fixed segment count such as `8`:

```cpp
constexpr int RoundedCornerSegmentCount = 8;
```

For each corner:

```cpp
for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; ++segmentIndex) {
    float angle0 = startAngle + (segmentStep * segmentIndex);
    float angle1 = startAngle + (segmentStep * (segmentIndex + 1));

    AppendSolidTriangle(
        centerX,
        centerY,
        centerX + (std::cos(angle0) * radius),
        centerY + (std::sin(angle0) * radius),
        centerX + (std::cos(angle1) * radius),
        centerY + (std::sin(angle1) * radius),
        packedColor,
        shape->get_RenderOrder2D());
}
```

Corner centers:

- top-left: `left + radius`, `top + radius`
- top-right: `right - radius`, `top + radius`
- bottom-right: `right - radius`, `bottom - radius`
- bottom-left: `left + radius`, `bottom - radius`

- [ ] **Step 3: Re-run the focused audit and the full test suite**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~Source_whenRenderingRoundedRectMenuPanels_containsSolidColorTrianglePath -p:BaseOutputPath=C:\tmp\psvita-roundedrect-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-roundedrect-audit-obj\
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-alltests-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-alltests-obj\
```

Expected: both commands pass.

- [ ] **Step 4: Commit the rounded-rect tessellation**

Run:

```bash
git add -- src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Render PS Vita menu rounded rectangles"
```

### Task 5: Verify Through The Real Editor Path And Vita3K

**Files:**
- Verify only: `C:\tmp\city-psvita-main-menu-cli`

- [ ] **Step 1: Rebuild through the real editor CLI path**

Run:

```powershell
dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psvita --output C:\tmp\city-psvita-main-menu-cli
```

Expected: `Build completed for platform 'psvita': C:\tmp\city-psvita-main-menu-cli`

- [ ] **Step 2: Run the Vita3K smoke verification**

Run:

```powershell
Copy-Item 'C:\tmp\city-psvita-main-menu-cli\helengine_psvita.vpk' '.\build\helengine_psvita.vpk' -Force
.\.codex\run-vita3k-smoke.ps1
```

Expected:

- `AliveAfter20s=True`
- no new renderer crash
- boot trace reaches first present

- [ ] **Step 3: Perform visible confirmation**

Run one visible launch and capture:

```powershell
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$process = Start-Process 'C:\dev\helworks\emus\vita-3k\Vita3K.exe' -ArgumentList '-r', 'HLEN00001' -PassThru
Start-Sleep -Seconds 8
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bitmap.Save('C:\tmp\vita3k-psvita-rounded-rect-menu.png', [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()
if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
```

Expected: the saved image shows the menu with its rounded panels rendered behind the text and logo.

- [ ] **Step 4: Commit the final verified state**

Run:

```bash
git add -- CMakeLists.txt builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs src/platform/psvita/rendering/PsVitaGxmRenderer.hpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp src/platform/psvita/rendering/PsVitaRenderManager2D.hpp src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Finish PS Vita menu rounded-rect rendering"
```
