# DS/3DS Bottom Controls Platform Cooking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Exclude DS/3DS-only bottom-screen controls from all non-DS/3DS cooked scene payloads.

**Architecture:** `EditorWindowsBuildScenePackager` correctly prunes authored scene entities before expansion. However, `BlueprintPackagedSceneExpansionService` can introduce new entities after that pass, including DS/3DS-only bottom-screen controls. Run the same platform-existence pruning pass immediately after expansion, before rewrite and dependency collection. The packaged payload becomes the authoritative filtered form, so excluded expanded entities and their descendants cannot reach any target runtime.

**Tech Stack:** C#/.NET 9, xUnit, Helengine editor build packaging.

---

### Task 1: Capture the platform-existence regression

**Files:**
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor.tests/managers/project/EditorWindowsBuildScenePackagerTests.cs`
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor/managers/project/EditorWindowsBuildScenePackager.cs:760-815`

- [x] **Step 1: Write the failing test**

Add a blueprint fixture whose child named `DemoDiscBottomScreenLightButton` carries a false `psvita` existence override. Package a scene containing that blueprint instance for `psvita` and assert that the expanded child is absent. Package the same fixture for `ds` and assert the child remains.

- [x] **Step 2: Run the targeted test to verify it fails**

Run: `dotnet test C:/dev/helworks/helengine/engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~EditorWindowsBuildScenePackagerTests" --no-restore --verbosity minimal`

Expected: the PSVita assertion fails because the initial pruning pass occurs before the blueprint creates the child carrying the existence override.

- [x] **Step 3: Apply the minimal packager correction**

Keep the existing pre-expansion pruning pass, then invoke `PruneEntitySubtreesForTargetPlatform` again immediately after `BlueprintExpansionService.Expand`. This preserves early pruning of excluded blueprint instances while correctly filtering descendants materialized by expansion. Continue clearing platform override metadata only after an entity has been retained for the target payload.

- [x] **Step 4: Run the targeted test to verify it passes**

Run: `dotnet test C:/dev/helworks/helengine/engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~EditorWindowsBuildScenePackagerTests" --no-restore --verbosity minimal`

Expected: the PSVita payload omits the scaffold subtree and DS retains it.

### Task 2: Verify the real Vita output

**Files:**
- Verify: `C:/dev/helprojs/demodisc/vita-build-shader-pipeline-2/cooked/scenes/physics/*.hasset`

- [x] **Step 1: Refresh the generated DS companion scene data**

Run the editor command `menu.generate-physics-nintendo-ds-scenes` after the project platform catalog contains `psvita`.

Expected: the generated bottom-screen subtree receives an explicit false existence override for `psvita`, alongside every other non-DS/3DS platform.

- [x] **Step 2: Verify the authored PSVita exclusion**

Inspect the regenerated `assets/scenes/physics/*.helen` payloads and confirm `DemoDiscBottomScreenLightButton` and `DemoDiscBottomScreenBackButton` are followed by a `psvita` existence override.

- [ ] **Step 3: Build PSVita through the editor CLI**

Run the existing `psvita` debug build with `HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT=C:/dev/helprojs/demodisc/vita-shader-compiler-exchange`.

- [ ] **Step 4: Verify the cooked payloads**

Run: `rg -a -l "DemoDiscBottomScreen(Light|Back)Button" C:/dev/helprojs/demodisc/vita-build-shader-pipeline-2/cooked/scenes/physics`

Expected: no matches.

- [ ] **Step 5: Install and launch the VPK**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/launch-vita3k.ps1 -VpkPath C:/dev/helprojs/demodisc/vita-build-shader-pipeline-2/helengine_psvita.vpk`

Expected: physics scenes show no DS/3DS bottom-screen Light or Back controls.
