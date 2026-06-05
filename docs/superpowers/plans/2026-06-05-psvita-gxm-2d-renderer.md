# PS Vita GXM 2D Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first native PS Vita GXM 2D renderer that shows main-menu sprites and text through the real editor CLI build path and boots in Vita3K.

**Architecture:** Keep all Vita-specific code inside `helengine-psvita`, preserve the existing external builder plus editor CLI flow, and replace the current empty `DrawSprite` / `DrawText` bodies with a batched textured-quad renderer backed by native GXM. Runtime textures continue to be created from the existing cooked asset pipeline, then lazily uploaded to Vita GPU memory and reused across sprite and text draws.

**Tech Stack:** C++20, VitaSDK GXM/display APIs, CMake, Docker, .NET 9 test runner, external PS Vita builder DLL, Helengine editor CLI, Vita3K

---

## File Structure

- Create: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`
  Purpose: lock in the new GXM file list, renderer references, and non-empty sprite/text draw paths before implementation grows.
- Create: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
  Purpose: declare the renderer-owned Vita GXM lifecycle, frame submission, texture upload, and present interface.
- Create: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`
  Purpose: implement GXM initialization, render-target setup, textured-quad submission, and display presentation.
- Create: `src/platform/psvita/rendering/PsVitaGpuTexture.hpp`
  Purpose: declare the native Vita GPU texture owner used by runtime textures.
- Create: `src/platform/psvita/rendering/PsVitaGpuTexture.cpp`
  Purpose: implement Vita texture allocation, upload, and release behavior.
- Create: `src/platform/psvita/rendering/PsVitaQueuedQuad.hpp`
  Purpose: define the queued 2D quad command shape shared by sprite and text submission.
- Create: `src/platform/psvita/rendering/PsVitaTexturedQuadVertex.hpp`
  Purpose: define the packed vertex layout used by the textured 2D pipeline.
- Modify: `src/platform/psvita/PsVitaBootHost.hpp`
  Purpose: replace direct framebuffer ownership with a renderer-owned GXM frame lifecycle.
- Modify: `src/platform/psvita/PsVitaBootHost.cpp`
  Purpose: initialize the Vita GXM renderer, pass it into the 2D runtime, and present through the renderer path.
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
  Purpose: add queued-quad storage, a renderer reference, and a `Draw()` flush override.
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`
  Purpose: convert sprite and text draw requests into queued quads and flush them through the GXM renderer.
- Modify: `src/platform/psvita/rendering/PsVitaRuntimeTexture.hpp`
  Purpose: extend the runtime texture to track dimensions and optional uploaded Vita GPU texture residency.
- Modify: `src/platform/psvita/rendering/PsVitaRuntimeTexture.cpp`
  Purpose: implement the new runtime texture ownership and GPU-texture accessor behavior.
- Modify: `src/platform/psvita/rendering/PsVitaTextureCache.hpp`
  Purpose: expose runtime textures with enough metadata for Vita GPU upload and release.
- Modify: `src/platform/psvita/rendering/PsVitaTextureCache.cpp`
  Purpose: preserve texture conversion while populating runtime dimensions and runtime asset metadata needed by the GXM upload path.
- Modify: `CMakeLists.txt`
  Purpose: compile the new GXM renderer sources and link the required Vita graphics libraries.
- Verify only: `builder/helengine.psvita.builder.csproj`
  Purpose: rebuild the external PS Vita builder DLL used by the editor CLI.

## Verification Prerequisites

These plan steps assume the following local tools and paths already exist:

- editor CLI: `C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll`
- test project: `builder.tests\helengine.psvita.builder.tests.csproj`
- Vita3K: `C:\dev\helworks\emus\vita-3k\Vita3K.exe`
- local builder output path: `C:\tmp\psvita-builder-cli`
- local editor build output path: `C:\tmp\city-psvita-main-menu-cli`

To avoid the known local `bin` locking issue, every `dotnet test` command in this plan writes intermediates into `C:\tmp`.

### Task 1: Lock The GXM Renderer Shape With Source Audits

**Files:**
- Create: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`
- Modify: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Write the failing source-audit test for the new renderer shape**

Create `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs` with assertions that check:

- `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp` exists
- `src/platform/psvita/rendering/PsVitaGpuTexture.hpp` exists
- `src/platform/psvita/rendering/PsVitaQueuedQuad.hpp` exists
- `src/platform/psvita/rendering/PsVitaTexturedQuadVertex.hpp` exists
- `PsVitaRenderManager2D.hpp` declares `void Draw() override;`
- `PsVitaRenderManager2D.cpp` does not contain empty `DrawSprite` and `DrawText` bodies
- `PsVitaBootHost.cpp` references `PsVitaGxmRenderer`
- `CMakeLists.txt` includes the new source files

- [ ] **Step 2: Run the new audit to confirm it fails first**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
```

Expected: FAIL because the GXM renderer files and `Draw()` override do not exist yet.

- [ ] **Step 3: Add the test file to the test project if the project does not auto-include it**

Inspect `builder.tests\helengine.psvita.builder.tests.csproj`. If it does not use SDK default item inclusion, add the new test file explicitly so the runner discovers it.

- [ ] **Step 4: Re-run the failing audit and capture the exact first missing symbol**

Run the same command from Step 2 again.

Expected: FAIL with a concrete missing-file or missing-symbol assertion that becomes the next implementation target.

- [ ] **Step 5: Commit the failing-test lock-in**

Run:

```powershell
git add -- builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs builder.tests\helengine.psvita.builder.tests.csproj
git commit -m "Add PS Vita GXM renderer source audits"
```

Expected: commit succeeds with only the new failing audit coverage staged.

### Task 2: Introduce The Vita GXM Foundation And Build Wiring

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`
- Create: `src/platform/psvita/rendering/PsVitaQueuedQuad.hpp`
- Create: `src/platform/psvita/rendering/PsVitaTexturedQuadVertex.hpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the failing declarations the audit expects**

Create the new header files with the minimal concrete surface:

- `PsVitaGxmRenderer` with `Initialize()`, `Shutdown()`, `BeginFrame()`, `SubmitQuads(...)`, and `PresentFrame()`
- `PsVitaQueuedQuad` with fields for texture pointer, render order, four positions, four UVs, and packed tint
- `PsVitaTexturedQuadVertex` with packed position, UV, and color

- [ ] **Step 2: Run the audit to verify the declarations alone are not enough**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
```

Expected: FAIL because `PsVitaBootHost.cpp`, `PsVitaRenderManager2D.cpp`, and `CMakeLists.txt` still do not reference the new renderer.

- [ ] **Step 3: Implement the minimal GXM renderer skeleton and wire it into CMake**

Add `PsVitaGxmRenderer.cpp` with the first concrete lifecycle implementation, even if quad submission is temporarily a stub:

- initialize the Vita GXM context and display-facing resources
- expose a stable frame lifecycle surface
- keep the implementation narrow to textured 2D submission

Update `CMakeLists.txt` to compile the new renderer sources and link the Vita graphics libraries required by GXM in addition to the existing display and kernel stubs.

- [ ] **Step 4: Re-run the audit to verify the file list and references now exist**

Run the same command from Step 2.

Expected: PASS for file-existence and CMake-reference assertions, while later behavior assertions continue to fail until the render manager is implemented.

- [ ] **Step 5: Commit the GXM foundation layer**

Run:

```powershell
git add -- CMakeLists.txt src/platform/psvita/rendering/PsVitaGxmRenderer.hpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp src/platform/psvita/rendering/PsVitaQueuedQuad.hpp src/platform/psvita/rendering/PsVitaTexturedQuadVertex.hpp
git commit -m "Add PS Vita GXM renderer foundation"
```

Expected: commit succeeds with only the new foundation files and CMake wiring staged.

### Task 3: Extend Runtime Textures To Native GPU Residency

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaGpuTexture.hpp`
- Create: `src/platform/psvita/rendering/PsVitaGpuTexture.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRuntimeTexture.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRuntimeTexture.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaTextureCache.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaTextureCache.cpp`
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Extend the audit with runtime-texture residency expectations**

Add assertions that verify:

- `PsVitaGpuTexture.hpp` exists
- `PsVitaRuntimeTexture` exposes width and height accessors
- `PsVitaRuntimeTexture` exposes Vita GPU texture accessors or setters
- `PsVitaGxmRenderer.cpp` references `PsVitaGpuTexture`

- [ ] **Step 2: Run the audit to confirm the new residency assertions fail**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
```

Expected: FAIL because runtime textures only own CPU-side ABGR8888 pixels today.

- [ ] **Step 3: Implement the minimal GPU-texture ownership path**

Make these concrete changes:

- `PsVitaGpuTexture` owns the Vita texture object plus its backing allocation
- `PsVitaRuntimeTexture` stores width, height, runtime asset id, CPU-side texels, and an optional `PsVitaGpuTexture`
- `PsVitaTextureCache` populates width and height when converting authored texture payloads
- `PsVitaGxmRenderer` can lazily upload a `PsVitaRuntimeTexture` the first time a queued quad needs it

- [ ] **Step 4: Re-run the audit and then the full builder test suite**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-alltests-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-alltests-obj\
```

Expected: the audit passes for runtime-texture residency shape, and the existing builder/plugin tests continue to pass.

- [ ] **Step 5: Commit the runtime texture upload path**

Run:

```powershell
git add -- builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs src/platform/psvita/rendering/PsVitaGpuTexture.hpp src/platform/psvita/rendering/PsVitaGpuTexture.cpp src/platform/psvita/rendering/PsVitaRuntimeTexture.hpp src/platform/psvita/rendering/PsVitaRuntimeTexture.cpp src/platform/psvita/rendering/PsVitaTextureCache.hpp src/platform/psvita/rendering/PsVitaTextureCache.cpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp
git commit -m "Add PS Vita runtime texture GPU residency"
```

Expected: commit succeeds with only the runtime-texture and upload-path changes staged.

### Task 4: Queue And Flush Sprite Quads Through The 2D Manager

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Extend the audit with sprite-queue and flush assertions**

Add assertions that verify:

- `PsVitaRenderManager2D.hpp` declares a `Draw() override`
- `PsVitaRenderManager2D` stores queued quad state
- `DrawSprite` references `PsVitaQueuedQuad`
- `Draw()` references `PsVitaGxmRenderer`

- [ ] **Step 2: Run the audit to confirm the sprite path is still missing**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
```

Expected: FAIL because the current 2D manager has no queue and no frame flush.

- [ ] **Step 3: Implement the minimal sprite queue**

Make these concrete changes:

- inject or assign a `PsVitaGxmRenderer` reference into `PsVitaRenderManager2D`
- add queued-quad storage
- implement `DrawSprite` to validate the texture, calculate UVs from `SourceRect`, apply color and rotation, and append one quad
- implement `Draw()` to sort queued quads by `RenderOrder2D` and texture identity, submit them through `PsVitaGxmRenderer`, and clear the queue

- [ ] **Step 4: Re-run the audit and full test suite**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-alltests-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-alltests-obj\
```

Expected: sprite-related audit assertions pass, and existing tests remain green.

- [ ] **Step 5: Commit the sprite queue path**

Run:

```powershell
git add -- builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs src/platform/psvita/rendering/PsVitaRenderManager2D.hpp src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Add PS Vita sprite quad submission"
```

Expected: commit succeeds with only the sprite-queue changes staged.

### Task 5: Render Font-Atlas Text Through The Same Quad Pipeline

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Extend the audit with text-path expectations**

Add assertions that verify:

- `DrawText` references `FontAsset`
- `DrawText` resolves glyphs from the font atlas
- `DrawText` enqueues `PsVitaQueuedQuad` instances
- `DrawText` is no longer an empty body

- [ ] **Step 2: Run the audit to confirm the text path still fails**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
```

Expected: FAIL because text rendering is still unimplemented.

- [ ] **Step 3: Implement the minimal text quad path**

Make these concrete changes:

- read `FontAsset`, `Characters`, `LineHeight`, `AtlasWidth`, and `AtlasHeight`
- respect `FontScale`
- if `WrapText` is true, compute the wrapped line layout before quad emission
- respect left, center, and right alignment within the drawable width
- emit one queued quad per visible glyph using the font atlas runtime texture

- [ ] **Step 4: Re-run the audit and the full test suite**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-audit-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-audit-obj\
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-alltests-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-alltests-obj\
```

Expected: the audit passes for text-path assertions, and all builder tests stay green.

- [ ] **Step 5: Commit the text rendering path**

Run:

```powershell
git add -- builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs src/platform/psvita/rendering/PsVitaRenderManager2D.hpp src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Add PS Vita font atlas text rendering"
```

Expected: commit succeeds with only the text-queue changes staged.

### Task 6: Integrate The GXM Renderer Into The Vita Boot Host

**Files:**
- Modify: `src/platform/psvita/PsVitaBootHost.hpp`
- Modify: `src/platform/psvita/PsVitaBootHost.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp`
- Modify: `builder.tests/PsVitaBootHostSourceAuditTests.cs`

- [ ] **Step 1: Extend the boot-host audit with GXM lifecycle assertions**

Add assertions that verify:

- `PsVitaBootHost` stores or initializes `PsVitaGxmRenderer`
- `InitializeGraphics()` delegates to GXM setup
- the generated-core main loop reaches `RenderManager2D::Draw()`
- manual cornflower-blue fallback remains only for the no-generated-core path

- [ ] **Step 2: Run the boot-host audit to confirm integration still fails**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaBootHostSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-boothost-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-boothost-obj\
```

Expected: FAIL because boot host still owns the direct framebuffer presentation loop.

- [ ] **Step 3: Integrate the renderer into the boot host**

Make these concrete changes:

- replace direct display-buffer ownership with a renderer-owned GXM lifecycle for generated-core builds
- construct `PsVitaRenderManager2D` with access to `PsVitaGxmRenderer`
- ensure the main loop updates core, flushes 2D draw work, and presents through the renderer
- preserve the simple blue-screen fallback when generated core is absent

- [ ] **Step 4: Re-run the boot-host audit and full test suite**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal --filter FullyQualifiedName~PsVitaBootHostSourceAuditTests -p:BaseOutputPath=C:\tmp\psvita-boothost-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-boothost-obj\
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-alltests-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-alltests-obj\
```

Expected: boot-host audit passes and the existing test suite remains green.

- [ ] **Step 5: Commit the boot-host integration**

Run:

```powershell
git add -- builder.tests\PsVitaBootHostSourceAuditTests.cs src/platform/psvita/PsVitaBootHost.hpp src/platform/psvita/PsVitaBootHost.cpp src/platform/psvita/rendering/PsVitaRenderManager2D.hpp src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "Integrate PS Vita GXM renderer into boot host"
```

Expected: commit succeeds with only the boot-host integration staged.

### Task 7: Verify Through The Real Editor CLI And Vita3K

**Files:**
- Verify only: `builder/helengine.psvita.builder.csproj`
- Verify only: `platform-plugin.json`
- Verify only: local editor settings outside this repo

- [ ] **Step 1: Rebuild the external PS Vita builder DLL used by the editor**

Run:

```powershell
dotnet build builder\helengine.psvita.builder.csproj -c Debug -o C:\tmp\psvita-builder-cli
```

Expected: `C:\tmp\psvita-builder-cli\helengine.psvita.builder.dll` is rebuilt with the latest repo changes.

- [ ] **Step 2: Run the full PS Vita editor CLI build**

Run:

```powershell
$env:HELENGINE_PSVITA_REPOSITORY_ROOT='C:\dev\helworks\helengine-psvita'
dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psvita --output C:\tmp\city-psvita-main-menu-cli
```

Expected: the build succeeds and emits `C:\tmp\city-psvita-main-menu-cli\helengine_psvita.vpk`.

- [ ] **Step 3: Boot the generated VPK in Vita3K**

Run:

```powershell
& 'C:\dev\helworks\emus\vita-3k\Vita3K.exe' -z 'C:\tmp\city-psvita-main-menu-cli\helengine_psvita.vpk'
```

Expected: Vita3K installs and runs the new package in console mode.

- [ ] **Step 4: Verify visible menu pixels and capture the first failure if not**

Check for:

- visible menu sprites
- visible menu text
- no immediate crash during startup scene load

If the menu is not visible, capture the first concrete renderer failure from Vita3K logs or console output and fix only that issue before re-running Steps 2 and 3.

- [ ] **Step 5: Commit the final renderer verification pass**

Run:

```powershell
git status --short
git add -- CMakeLists.txt builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs src\platform\psvita\PsVitaBootHost.hpp src\platform\psvita\PsVitaBootHost.cpp src\platform\psvita\rendering\
git commit -m "Render PS Vita menu sprites and text through GXM"
```

Expected: commit succeeds with only the renderer implementation staged, and the repo is ready for another Vita3K validation pass or code review.
