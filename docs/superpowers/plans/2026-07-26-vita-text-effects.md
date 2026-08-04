# PS Vita Text Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render each PS Vita text glyph with its authored shadow and outline effects in the same sequence as Windows.

**Architecture:** `PsVitaRenderManager2D::DrawText` already constructs glyph quads. It will construct a local ordered set of effect passes from the generated `ITextDrawable2D` properties and enqueue a copy of each glyph for every pass. Each copy retains the same composed render order, so the existing stable queue preserves shadow, outline, then foreground layering.

**Tech Stack:** C++17, PS Vita GXM queued quads, xUnit source-audit tests.

---

### Task 1: Lock the text-effect contract with an audit

**Files:**
- Modify: `builder.tests/PsVitaRenderManager2DSourceAuditTests.cs`
- Test: `builder.tests/PsVitaRenderManager2DSourceAuditTests.cs`

- [ ] **Step 1: Write the failing test**

Add an xUnit fact that reads `src/platform/psvita/rendering/PsVitaRenderManager2D.cpp` and requires the five generated text properties plus all six effect offsets:

```csharp
Assert.Contains("get_ShadowOffset", sourceCode, StringComparison.Ordinal);
Assert.Contains("get_ShadowColor", sourceCode, StringComparison.Ordinal);
Assert.Contains("get_OutlineScale", sourceCode, StringComparison.Ordinal);
Assert.Contains("get_OutlineColor", sourceCode, StringComparison.Ordinal);
Assert.Contains("float2(-outlineScale, 0.0f)", sourceCode, StringComparison.Ordinal);
Assert.Contains("float2(outlineScale, 0.0f)", sourceCode, StringComparison.Ordinal);
Assert.Contains("float2(0.0f, -outlineScale)", sourceCode, StringComparison.Ordinal);
Assert.Contains("float2(0.0f, outlineScale)", sourceCode, StringComparison.Ordinal);
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter FullyQualifiedName~PsVitaRenderManager2DSourceAuditTests --no-restore
```

Expected: failure because `DrawText` currently emits only the foreground glyph.

- [ ] **Step 3: Implement the minimal pass sequence**

In `PsVitaRenderManager2D::DrawText`, build pass values before the glyph loops in this exact order:

```cpp
shadow offset/color when either shadow coordinate is non-zero;
left, right, top, bottom outline offset/color when outlineScale is positive;
zero-offset foreground pass using text color.
```

For every glyph, queue a quad per pass using `glyphX + passOffset.X`, `glyphY + passOffset.Y`, and the pass color. Preserve `ComposeRenderOrder(ActiveCameraRenderOrder, text->get_RenderOrder2D())` for each copy.

- [ ] **Step 4: Run test to verify it passes**

Run the Task 1 command. Expected: all focused render-manager audits pass.

- [ ] **Step 5: Commit**

```powershell
git add builder.tests/PsVitaRenderManager2DSourceAuditTests.cs src/platform/psvita/rendering/PsVitaRenderManager2D.cpp
git commit -m "feat: render Vita text effects"
```

### Task 2: Verify the cooked native result

**Files:**
- Verify: `C:\dev\helprojs\demodisc\vita-build-standard-shadows\helengine_psvita.vpk`

- [ ] **Step 1: Build the PS Vita VPK**

Run the repository’s platform build script with the DemoDisc project and wait for completion.

- [ ] **Step 2: Launch the produced VPK in Vita3K**

Run `tools\launch-vita3k.ps1` with the produced VPK path.

- [ ] **Step 3: Verify visually**

Open the main menu and confirm left-button text has its shadow and cardinal outline, with the foreground glyph on top. Confirm no text appears above a later UI layer.
