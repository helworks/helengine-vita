# Shared Standard Shader Shadows Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Standard Shader declare forward, shadowed-forward, and depth-only variants once, compile those variants for every shader-capable target in scope (Windows, PS Vita, Wii U), then add the first real PS Vita shadow render pass.

**Architecture:** The shared shader layer defines immutable names, stage entry points, and source defines for all Standard Shader variants. Editor/CLI shader compilation enumerates those definitions for each shader-capable target. Each target backend lowers or compiles the same variant contract into native artifacts; materials remain parameter collections that select Standard Shader, never platform-specific shadow shader files. PS Vita keeps Vita2D as the sole owner of the active GXM scene and uses Vita2D's supported offscreen render-target API for the shadow pass before the artifact-backed main pass.

**Tech Stack:** C#/.NET 9 shared shader/editor tests, HLSL, DirectX11 and Vulkan shader backends, Wii U GX2 shader tools, PS Vita C++20/VitaSDK/GXM, real Vita device compiler, Vita3K.

---

## File structure

- External engine: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariant.cs` — new immutable description of a Standard Shader variant and its two stage entry points.
- External engine: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariants.cs` — one shared ordered catalog: `ForwardStandard`, `ForwardStandardShadowed`, `ShadowDepth`.
- External engine: `C:/dev/helworks/helengine/engine/helengine.shader/assets/material/BuiltInMaterialIds.cs` — stable shader/program/variant names used by materials and renderers.
- External engine: `C:/dev/helworks/helengine/engine/helengine.editor/shaders/builtin/ForwardStandardShader.hlsl` — shared HLSL entry points for the three variants; no target-specific source copy.
- External engine: `C:/dev/helworks/helengine/engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs` — compile each catalog entry using its own variant name and entry points, rather than repeating the default request.
- External engine test: `C:/dev/helworks/helengine/engine/helengine.editor.tests/shaders/ForwardStandardShaderTests.cs` — verifies the full shared contract for DirectX11 and Vulkan.
- PS Vita builder: `builder/shaders/PsVitaForwardStandardSourceLowerer.cs` — converts a requested shared Standard Shader variant into Cg source; it no longer invents variants.
- PS Vita builder: `builder/PsVitaShaderBundleCompiler.cs`, `builder/PsVitaShaderBundleSource.cs` — maps shared definitions to device stage jobs and bundle entries.
- PS Vita tests: `builder.tests/PsVitaForwardStandardSourceLowererTests.cs`, `builder.tests/PsVitaShaderBundleCompilerTests.cs`, `builder.tests/PsVitaShaderBundleBinarySerializerTests.cs`.
- Shared manifest: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariantMakefileManifestWriter.cs` — emits the canonical Makefile include from `StandardShaderVariants`.
- Wii U: `C:/dev/helworks/helengine-wiiu/tools/wiiu-shaders/standard_shader_variants.mk`, `ForwardStandard.vs/.ps`, `ForwardStandardShadowed.vs/.ps`, `ShadowDepth.vs/.ps`, and `Makefile` — compile the generated manifest identities to GX2 binaries.
- PS Vita renderer: `src/platform/psvita/rendering/PsVitaGxmShadowMap.hpp/.cpp`, `PsVitaGxmShadowDepthProgram.hpp/.cpp`, `PsVitaGxmRenderer.hpp/.cpp`, `PsVitaRenderManager3D.hpp/.cpp`, and `CMakeLists.txt`.

### Task 1: Define the shared Standard Shader variant catalog

**Files:**

- Create: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariant.cs`
- Create: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariants.cs`
- Modify: `C:/dev/helworks/helengine/engine/helengine.shader/assets/material/BuiltInMaterialIds.cs`
- Test: `C:/dev/helworks/helengine/engine/helengine.editor.tests/shaders/StandardShaderVariantsTests.cs`

- [ ] **Step 1: Write the failing shared-catalog test**

```csharp
[Fact]
public void All_returnsTheThreeStandardShaderVariantsInStableOrder() {
    IReadOnlyList<StandardShaderVariant> variants = StandardShaderVariants.All;

    Assert.Collection(variants,
        variant => Assert.Equal("ForwardStandard", variant.Name),
        variant => Assert.Equal("ForwardStandardShadowed", variant.Name),
        variant => Assert.Equal("ShadowDepth", variant.Name));
    Assert.Equal("VS", variants[2].VertexEntryPoint);
    Assert.Equal("ShadowDepthPS", variants[2].PixelEntryPoint);
}
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~StandardShaderVariantsTests" --no-restore
```

Expected: compilation failure because the catalog does not exist.

- [ ] **Step 3: Implement the immutable catalog**

Create `StandardShaderVariant` with required `Name`, `VertexEntryPoint`, `PixelEntryPoint`, and `Defines`. Create `StandardShaderVariants.All` as exactly:

```csharp
new StandardShaderVariant("ForwardStandard", "VS", "PS", Array.Empty<string>()),
new StandardShaderVariant("ForwardStandardShadowed", "VS", "PS", new[] { "HELENGINE_STANDARD_SHADOWED=1" }),
new StandardShaderVariant("ShadowDepth", "VS", "ShadowDepthPS", new[] { "HELENGINE_STANDARD_SHADOW_DEPTH=1" })
```

Add corresponding `BuiltInMaterialIds` constants. Existing material-facing forward selection continues to use `ForwardStandard`; the two additional constants are renderer-selected program variants only.

- [ ] **Step 4: Run the test and verify GREEN**

Run the command from Step 2.

Expected: PASS.

- [ ] **Step 5: Commit the shared catalog**

```powershell
rtk git -C C:\dev\helworks\helengine add engine/helengine.shader/shaders/StandardShaderVariant.cs engine/helengine.shader/shaders/StandardShaderVariants.cs engine/helengine.shader/assets/material/BuiltInMaterialIds.cs engine/helengine.editor.tests/shaders/StandardShaderVariantsTests.cs
rtk git -C C:\dev\helworks\helengine commit -m "feat: declare shared Standard Shader variants"
```

### Task 2: Compile the shared variants for Windows shader backends

**Files:**

- Modify: `C:/dev/helworks/helengine/engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs`
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor/shaders/builtin/ForwardStandardShader.hlsl`
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor.tests/shaders/ForwardStandardShaderTests.cs`

- [ ] **Step 1: Write the failing DirectX11/Vulkan asset test**

```csharp
[Theory]
[InlineData(ShaderCompileTarget.DirectX11)]
[InlineData(ShaderCompileTarget.Vulkan)]
public void LoadShaderAsset_whenStandardShaderIsLoaded_compilesEverySharedVariant(ShaderCompileTarget target) {
    ShaderAsset asset = EditorBuiltInShaderAssetLibrary.LoadShaderAsset(target, "ForwardStandardShader.hlsl");

    Assert.Equal(6, asset.Binaries.Length);
    Assert.Contains(asset.Binaries, binary => binary.Stage == ShaderStage.Pixel && binary.Variant == "ForwardStandardShadowed");
    Assert.Contains(asset.Binaries, binary => binary.Stage == ShaderStage.Pixel && binary.Variant == "ShadowDepth" && binary.ProgramName == "ForwardStandardShader.ps");
}
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~ForwardStandardShaderTests.LoadShaderAsset_whenStandardShaderIsLoaded_compilesEverySharedVariant" --no-restore
```

Expected: failure because the library publishes only `default` and `Mesh`, and always sends `default` to `ShaderCompileRequest`.

- [ ] **Step 3: Compile each catalog definition correctly**

Replace the string-only variant loop with `StandardShaderVariants.All`. Pass `variant.Name` into `ShaderCompileRequest`; translate `variant.Defines` into `ShaderDefine` values added to `ShaderPlatformDefines.BuildDefines`; use each variant’s declared vertex/pixel entry point. Keep material layout reflection based on `ForwardStandard` only.

Add `ShadowDepthPS` to `ForwardStandardShader.hlsl` behind `HELENGINE_STANDARD_SHADOW_DEPTH`; it must write no material texture/light result. The regular `PS` path keeps the existing Windows lighting implementation and observes `HELENGINE_STANDARD_SHADOWED` only for the explicit shadowed artifact identity.

- [ ] **Step 4: Run focused Windows tests and verify GREEN**

Run the command from Step 2.

Expected: PASS for DirectX11 and Vulkan, with six binaries and no duplicate `(program, stage, variant)` keys.

- [ ] **Step 5: Commit the shared Windows compilation slice**

```powershell
rtk git -C C:\dev\helworks\helengine add engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs engine/helengine.editor/shaders/builtin/ForwardStandardShader.hlsl engine/helengine.editor.tests/shaders/ForwardStandardShaderTests.cs
rtk git -C C:\dev\helworks\helengine commit -m "feat: compile all Standard Shader variants"
```

### Task 3: Make the Vita compiler consume the shared contract

**Files:**

- Modify: `builder/shaders/PsVitaForwardStandardSourceLowerer.cs`
- Modify: `builder/PsVitaShaderBundleCompiler.cs`
- Modify: `builder/PsVitaShaderBundleSource.cs`
- Test: `builder.tests/PsVitaForwardStandardSourceLowererTests.cs`
- Test: `builder.tests/PsVitaShaderBundleCompilerTests.cs`

- [ ] **Step 1: Write the failing shared-contract test**

```csharp
[Fact]
public void Cook_forwardStandardShader_submitsOneStagePairForEachSharedVariant() {
    PsVitaShaderBundleCompilerTestExchange exchange = new();
    new PsVitaShaderBundleCompiler(exchange).Cook(CreateForwardStandardRequest());

    Assert.Equal(StandardShaderVariants.All.Count * 2, exchange.SubmittedJob.Stages.Count);
    Assert.Contains(exchange.SubmittedSourceFiles, file => file.SourceText.Contains("HELENGINE_STANDARD_SHADOW_DEPTH", StringComparison.Ordinal));
}
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBundleCompilerTests.Cook_forwardStandardShader_submitsOneStagePairForEachSharedVariant" --no-restore
```

Expected: failure because Vita owns a separate hand-maintained variant list.

- [ ] **Step 3: Remove Vita-owned variant declaration**

Make `PsVitaForwardStandardSourceLowerer.Lower(StandardShaderVariant variant)` switch solely on `StandardShaderVariants` identities and generate the Cg form for that requested shared variant. `PsVitaShaderBundleCompiler` must enumerate `StandardShaderVariants.All`, preserve each `variant.Name` in `PsVitaShaderBundleSource`, and add bundle entries under that same name. Do not derive names such as `ForwardStandardTextured` in the Vita compiler; retain the material’s original unshadowed variant as an alias to `ForwardStandard` only at bundle lookup.

- [ ] **Step 4: Run all Vita compiler/bundle tests and verify GREEN**

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaForwardStandardSourceLowererTests|FullyQualifiedName~PsVitaShaderBundleCompilerTests|FullyQualifiedName~PsVitaShaderBundleBinarySerializerTests" --no-restore
```

Expected: PASS; every Vita PVSA pair comes from a shared variant identity.

- [ ] **Step 5: Commit only safely tracked Vita files**

Do not stage pre-existing untracked shader-pipeline files in this shared workspace. First move the slice into a clean checkout or have the owning agent stage its files; then commit exactly the files listed above with:

```powershell
rtk git commit -m "feat: consume shared Standard Shader variants on Vita"
```

### Task 4: Compile the same variant identities for Wii U

**Files:**

- Create: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/StandardShaderVariantMakefileManifestWriter.cs`
- Test: `C:/dev/helworks/helengine/engine/helengine.editor.tests/shaders/StandardShaderVariantMakefileManifestWriterTests.cs`
- Create: `C:/dev/helworks/helengine-wiiu/tools/wiiu-shaders/standard_shader_variants.mk`
- Create: `C:/dev/helworks/helengine-wiiu/tools/wiiu-shaders/ForwardStandard.vs/.ps`, `ForwardStandardShadowed.vs/.ps`, and `ShadowDepth.vs/.ps`
- Modify: `C:/dev/helworks/helengine-wiiu/Makefile`

- [ ] **Step 1: Write the failing Wii U artifact-manifest test**

The shared test must assert that the Makefile include is emitted directly from the catalog:

```powershell
string manifest = new StandardShaderVariantMakefileManifestWriter().Write();

Assert.Contains("STANDARD_SHADER_VARIANTS := ForwardStandard ForwardStandardShadowed ShadowDepth", manifest, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the test and verify RED**

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~StandardShaderVariantMakefileManifestWriterTests" --no-restore
```

Expected: failure because the shared manifest writer does not exist.

- [ ] **Step 3: Map catalog identities to GX2 sources**

Implement `StandardShaderVariantMakefileManifestWriter.Write()` to produce exactly `STANDARD_SHADER_VARIANTS := ForwardStandard ForwardStandardShadowed ShadowDepth` plus a trailing newline. Commit the generated `standard_shader_variants.mk` only when its bytes equal that writer output. Make the Wii U `Makefile` include it and derive `REQUIRED_SHADER_BINFILES` from `STANDARD_SHADER_VARIANTS`. Compile regular and shadowed forward variants from matching source pairs and a depth-only pair named `ShadowDepth`.

- [ ] **Step 4: Run the test and build once**

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~StandardShaderVariantMakefileManifestWriterTests" --no-restore
rtk make -C C:\dev\helworks\helengine-wiiu -n
```

Expected: contract PASS and Makefile output names `ForwardStandard_shader.bin`, `ForwardStandardShadowed_shader.bin`, and `ShadowDepth_shader.bin`.

- [ ] **Step 5: Commit in the Wii U repository**

```powershell
rtk git -C C:\dev\helworks\helengine-wiiu add Makefile tools/wiiu-shaders
rtk git -C C:\dev\helworks\helengine-wiiu commit -m "feat: compile shared Standard Shader variants"
```

### Task 5: Add a Vita2D-owned offscreen shadow target

**Files:**

- Create: `src/platform/psvita/rendering/PsVitaGxmRenderPassController.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp/.cpp`
- Modify: `CMakeLists.txt`
- Test: `builder.tests/PsVitaGxmShadowMapSourceAuditTests.cs`

- [ ] **Step 1: Write the failing ownership audit**

```csharp
[Fact]
public void Source_whenShadowPassesAreEnabled_usesVita2dOffscreenTargetWithoutRawSceneOwnership() {
    string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmShadowMap.cpp"));

    Assert.Contains("vita2d_create_empty_texture_rendertarget", source, StringComparison.Ordinal);
    Assert.Contains("vita2d_start_drawing_advanced", source, StringComparison.Ordinal);
    Assert.Contains("vita2d_end_drawing", source, StringComparison.Ordinal);
    Assert.DoesNotContain("sceGxmBeginScene", source, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the audit and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmShadowMapSourceAuditTests" --no-restore
```

Expected: failure because no offscreen shadow target exists.

- [ ] **Step 3: Implement the supported offscreen target**

Create `PsVitaGxmShadowMap` around `vita2d_create_empty_texture_rendertarget`. Begin the depth pass with `vita2d_start_drawing_advanced`, finish it with `vita2d_end_drawing`, and obtain the public active context through `vita2d_get_context`. Do not use the private `_vita2d_context` symbol and do not call raw `sceGxmBeginScene` or `sceGxmEndScene`.

- [ ] **Step 4: Verify the boundary**

Run the command from Step 2, build the Vita executable, then launch the existing unshadowed `cube_test` on Vita3K and real Vita.

Expected: audit PASS and existing unshadowed Standard rendering remains stable.

### Task 6: Add the first Vita directional shadow tier

**Files:**

- Create: `src/platform/psvita/rendering/PsVitaGxmShadowMap.hpp/.cpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmShadowDepthProgram.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmForwardLambertProgram.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp/.cpp`
- Test: `builder.tests/PsVitaGxmShadowMapSourceAuditTests.cs`, `builder.tests/PsVitaGxmRendererShadowSourceAuditTests.cs`, `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Write failing behavior audits**

Require a 256 x 256 target, strict validation of `HelengineLightViewProjection`, `HelengineShadowTexture`, and `HelengineShadowBias`, caster filtering from `CastsShadows`, receiver selection from `ReceivesShadows`, and an unshadowed Standard path only when no directional shadow light is enabled.

- [ ] **Step 2: Run the audits and verify RED**

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmShadowMapSourceAuditTests|FullyQualifiedName~PsVitaGxmRendererShadowSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" --no-restore
```

Expected: failure because no offscreen target or shadow draw operations exist.

- [ ] **Step 3: Implement one directional hard shadow map**

Allocate one 256 x 256 Vita2D render-target texture. Render opaque Standard casters with the `ShadowDepth` artifact, resume the main Vita2D target, bind that texture into `ForwardStandardShadowed`, and attenuate direct light with one hard comparison and explicit bias. Select at most one enabled directional shadow light. Missing artifacts, parameters, textures, or GXM calls throw diagnostics; no Lambert/CPU fallback is allowed.

- [ ] **Step 4: Re-run source audits and build artifacts on real Vita**

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmShadowMapSourceAuditTests|FullyQualifiedName~PsVitaGxmRendererShadowSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests" --no-restore
```

Then use the established real-Vita compiler exchange to produce the matching PVSA files, rerun the editor CLI cook, and inspect `cooked/shaders/psvita/shaders.psvb` for `ForwardStandard`, `ForwardStandardShadowed`, and `ShadowDepth`.

- [ ] **Step 5: Validate on both targets**

Launch the focused caster/receiver scene in Vita3K and real Vita. Confirm visible shadowing, `CastsShadows` and `ReceivesShadows` independently change only their intended behavior, disabled shadow light returns to unshadowed Standard, no CPU/Lambert fallback occurs, and record FPS.

## Final verification checklist

- [ ] DirectX11 and Vulkan compile all three shared Standard Shader variants.
- [ ] Wii U produces all three matching GX2 variant identities.
- [ ] Vita device compiler produces all three matching PVSA pairs and the bundle preserves them.
- [ ] Existing unshadowed Vita Standard scenes still run in Vita3K and on real hardware.
- [ ] Vita directional shadow scene passes caster/receiver/light-state checks on both targets.
- [ ] No material serialization changes or generated C++ edits were made.
- [ ] No CPU or Lambert fallback is reachable from any shadow path.
