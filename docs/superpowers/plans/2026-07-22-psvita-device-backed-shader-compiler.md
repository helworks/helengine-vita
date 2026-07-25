# PS Vita Device-Backed Shader Compiler Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real-Vita, launch-and-exit shader compiler VPK and connect it to Helengine's shared shader compile service as the `PsVita` target.

**Architecture:** The shared compiler produces deterministic device jobs. The standalone VPK compiles the job at fixed `ux0:data/helengine_shader_compiler` locations with `libshacccg`, writes PVSA artifacts and a structured result, then exits. The editor-side backend accepts only a matching, validated result and returns it through the existing compiler cache.

**Tech Stack:** C#/.NET 9, `ShaderCompileService`/`IShaderBackend`, System.Text.Json, C++20, VitaSDK, `libshacccg`, PVSA artifact format, CMake/Docker Vita build.

---

## File structure

- External engine: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/compilation/ShaderCompileTarget.cs` — add the shared PS Vita compilation target.
- External engine: `C:/dev/helworks/helengine/engine/helengine.editor/EditorCliBuildRunner.cs` — select the target graphics compile profile for a PS Vita build instead of hard-coding DirectX11.
- External engine tests: `C:/dev/helworks/helengine/engine/helengine.editor.tests/EditorCliBuildRunnerTests.cs` — audit the target selection.
- Builder: `builder/PsVitaPlatformAssetBuilder.cs` — contribute the Vita shader backend and consume returned artifacts when cooking materials.
- Builder: `builder/shaders/PsVitaShaderCompilerJob.cs` — immutable host-side job/stage request model.
- Builder: `builder/shaders/PsVitaShaderCompilerJobSerializer.cs` — deterministic JSON inbox manifest serializer.
- Builder: `builder/shaders/PsVitaShaderCompilerResult.cs` — host-side result/stage model.
- Builder: `builder/shaders/PsVitaShaderCompilerResultSerializer.cs` — strict outbox result parser.
- Builder: `builder/shaders/IPsVitaShaderCompilerExchange.cs` — file-exchange seam for local/FTP/VitaShell transfer workflows.
- Builder: `builder/shaders/PsVitaShaderCompilerExchange.cs` — host-folder inbox/outbox implementation.
- Builder: `builder/shaders/PsVitaShaderBackend.cs` — `IShaderBackend` implementation for `ShaderCompileTarget.PsVita`.
- Builder tests: `builder.tests/PsVitaShaderCompilerJobSerializerTests.cs`, `builder.tests/PsVitaShaderCompilerResultSerializerTests.cs`, `builder.tests/PsVitaShaderBackendTests.cs`.
- Compiler VPK: `tools/shader-compiler/CMakeLists.txt`, `tools/shader-compiler/src/main.cpp`, and focused `tools/shader-compiler/src/*.cpp/.hpp` modules for job reading, compile execution, result writing, and fixed storage paths.
- Native tests: `builder.tests/PsVitaShaderCompilerVpkSourceAuditTests.cs` — audit fixed paths, no command-line parsing, launch-and-exit behavior, and `libshacccg` integration.

### Task 1: Add the shared PS Vita compiler target

**Files:**

- Modify: `C:/dev/helworks/helengine/engine/helengine.shader/shaders/compilation/ShaderCompileTarget.cs`
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor/EditorCliBuildRunner.cs`
- Test: `C:/dev/helworks/helengine/engine/helengine.editor.tests/EditorCliBuildRunnerTests.cs`

- [ ] **Step 1: Write the failing target-selection test**

```csharp
[Fact]
public void Build_whenTargetPlatformIsPsVita_selectsThePsVitaShaderCompileTarget() {
    ShaderCompileTarget target = EditorCliBuildRunner.ResolveShaderCompileTarget("psvita");

    Assert.Equal(ShaderCompileTarget.PsVita, target);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter "FullyQualifiedName~EditorCliBuildRunnerTests.Build_whenTargetPlatformIsPsVita" --no-restore
```

Expected: failure because `PsVita` and `ResolveShaderCompileTarget` do not exist.

- [ ] **Step 3: Add the target and route the CLI build to it**

```csharp
public enum ShaderCompileTarget {
    DirectX9,
    DirectX11,
    DirectX12,
    Vulkan,
    Metal,
    PsVita
}

internal static ShaderCompileTarget ResolveShaderCompileTarget(string platformId) {
    return string.Equals(platformId, "psvita", StringComparison.OrdinalIgnoreCase)
        ? ShaderCompileTarget.PsVita
        : ShaderCompileTarget.DirectX11;
}
```

Use `ResolveShaderCompileTarget(options.PlatformId)` when constructing `ShaderTargetBuildOptions`.

- [ ] **Step 4: Run the focused test to verify it passes**

Run the command from Step 2.

Expected: PASS.

### Task 2: Define deterministic host/Vita job and result contracts

**Files:**

- Create: `builder/shaders/PsVitaShaderCompilerJob.cs`
- Create: `builder/shaders/PsVitaShaderCompilerJobSerializer.cs`
- Create: `builder/shaders/PsVitaShaderCompilerResult.cs`
- Create: `builder/shaders/PsVitaShaderCompilerResultSerializer.cs`
- Test: `builder.tests/PsVitaShaderCompilerJobSerializerTests.cs`
- Test: `builder.tests/PsVitaShaderCompilerResultSerializerTests.cs`

- [ ] **Step 1: Write failing serialization tests**

```csharp
[Fact]
public void Serialize_whenStagesAreSupplied_writesStableJobHashAndOrderedStageRequests() {
    PsVitaShaderCompilerJob job = PsVitaShaderCompilerJob.Create(
        "AABB",
        [new PsVitaShaderCompilerStageRequest("vertex", "source/standard.cg", "VS", "VP", "O3-W4")]);

    string json = new PsVitaShaderCompilerJobSerializer().Serialize(job);

    Assert.Contains("\"jobHash\":\"AABB\"", json, StringComparison.Ordinal);
    Assert.Contains("\"profile\":\"VP\"", json, StringComparison.Ordinal);
}

[Fact]
public void Deserialize_whenArtifactHashDoesNotMatchReportedPayload_rejectsTheResult() {
    string json = "{\"jobHash\":\"AABB\",\"stages\":[{\"stageId\":\"vertex\",\"success\":true,\"artifactHash\":\"BAD\"}]}";

    Assert.Throws<InvalidOperationException>(() => new PsVitaShaderCompilerResultSerializer().Deserialize(json));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderCompilerJobSerializerTests|FullyQualifiedName~PsVitaShaderCompilerResultSerializerTests" --no-restore
```

Expected: failure because the job/result contracts do not exist.

- [ ] **Step 3: Implement the contracts**

Use JSON with these required fields:

```json
{
  "formatVersion": 1,
  "jobHash": "<shared compile cache key>",
  "stages": [
    {
      "stageId": "vertex",
      "sourcePath": "source/ForwardStandardShader.cg",
      "entryPoint": "VS",
      "profile": "VP",
      "optionsSignature": "O3-W4"
    }
  ]
}
```

The result must contain `formatVersion`, `jobHash`, one result for every requested stage, `success`, `diagnostic`, `artifactPath`, `artifactHash`, and `programByteCount`. Reject empty paths, duplicate stage IDs, unknown profiles, missing stages, and a success record without a canonical artifact hash.

- [ ] **Step 4: Run the focused serializer tests**

Run the command from Step 2.

Expected: PASS.

### Task 3: Add the editor-side exchange and PS Vita shader backend

**Files:**

- Create: `builder/shaders/IPsVitaShaderCompilerExchange.cs`
- Create: `builder/shaders/PsVitaShaderCompilerExchange.cs`
- Create: `builder/shaders/PsVitaShaderBackend.cs`
- Modify: `builder/PsVitaPlatformAssetBuilder.cs`
- Test: `builder.tests/PsVitaShaderBackendTests.cs`

- [ ] **Step 1: Write failing backend tests**

```csharp
[Fact]
public void Compile_whenMatchingSuccessfulOutboxResultExists_returnsValidatedStageBinary() {
    RecordingPsVitaShaderCompilerExchange exchange = new();
    exchange.SetResult(CreateSuccessfulResult("AABB", "vertex", ValidVertexArtifactBytes));
    PsVitaShaderBackend backend = new(exchange);

    ShaderCompileResult result = backend.Compile(CreateVertexRequest("AABB"), CreateIncludeResolver());

    Assert.Equal(ShaderCompileTarget.PsVita, backend.Target);
    Assert.Equal(ValidVertexArtifactBytes, result.Binary.Data);
}

[Fact]
public void Compile_whenOutboxJobHashIsStale_throwsInsteadOfUsingTheArtifact() {
    RecordingPsVitaShaderCompilerExchange exchange = new();
    exchange.SetResult(CreateSuccessfulResult("STALE", "vertex", ValidVertexArtifactBytes));

    Assert.Throws<InvalidOperationException>(() => new PsVitaShaderBackend(exchange).Compile(CreateVertexRequest("AABB"), CreateIncludeResolver()));
}
```

- [ ] **Step 2: Run the backend tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBackendTests" --no-restore
```

Expected: failure because the exchange and backend do not exist.

- [ ] **Step 3: Implement exchange behavior and backend registration**

`PsVitaShaderCompilerExchange` writes source files and `manifest.json` beneath its configured host inbox root and reads only `outbox/<jobHash>/results.json`. It must not delete an inbox/outbox directory it did not create.

`PsVitaShaderBackend` implements `IShaderBackend`, declares `ShaderCompileTarget.PsVita`, creates the deterministic job using the shared request identity, writes it through the exchange, validates the matching returned PVSA artifact with `PsVitaShaderArtifactBinarySerializer`, and returns the exact compiled binary. When the outbox result is not available, throw an actionable exception that names the job hash and fixed Vita paths.

Make `PsVitaPlatformAssetBuilder` implement `IShaderBackendRegistryContributor` and register exactly one `PsVitaShaderBackend` built from its configured exchange.

- [ ] **Step 4: Run the backend tests and existing builder tests**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBackendTests|FullyQualifiedName~PsVitaPlatformAssetBuilderTests" --no-restore
```

Expected: PASS.

### Task 4: Build the standalone launch-and-exit compiler VPK

**Files:**

- Create: `tools/shader-compiler/CMakeLists.txt`
- Create: `tools/shader-compiler/src/main.cpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerJobReader.hpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerJobReader.cpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerResultWriter.hpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerResultWriter.cpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerQueueProcessor.hpp`
- Create: `tools/shader-compiler/src/PsVitaShaderCompilerQueueProcessor.cpp`
- Reuse: `src/platform/psvita/shaders/PsVitaShaderArtifactWriter.cpp`
- Test: `builder.tests/PsVitaShaderCompilerVpkSourceAuditTests.cs`

- [ ] **Step 1: Write failing VPK source audits**

```csharp
[Fact]
public void CompilerVpk_whenLaunched_readsTheFixedInboxWritesTheMatchingOutboxAndExits() {
    string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("tools", "shader-compiler", "src", "main.cpp"));

    Assert.Contains("ux0:data/helengine_shader_compiler/inbox/manifest.json", source, StringComparison.Ordinal);
    Assert.Contains("return queueProcessor.ProcessSingleJob();", source, StringComparison.Ordinal);
    Assert.DoesNotContain("argc", source, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the VPK audit to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderCompilerVpkSourceAuditTests" --no-restore
```

Expected: failure because the standalone VPK source does not exist.

- [ ] **Step 3: Implement the compiler VPK**

The tool must:

1. Read only `ux0:data/helengine_shader_compiler/inbox/manifest.json` and source files beneath its fixed inbox directory.
2. Reject `..`, absolute paths, duplicate stage IDs, empty sources, unsupported profiles, and jobs above the documented byte limit.
3. Call `PsVitaShaderArtifactWriter::Write` once per stage with a fixed output path inside `outbox/<jobHash>.tmp/`.
4. Write `results.json` to the same temporary directory, then rename it to `outbox/<jobHash>/` only after every successful artifact/result write is durable.
5. Write a failed `results.json` with the compiler diagnostic for a stage failure.
6. Return after one job; do not inspect command-line arguments or enter a render/input loop.

`tools/shader-compiler/CMakeLists.txt` creates title ID `HLEN00002`, title `Helengine Vita Shader Compiler`, includes `sce_sys/icon0.png`, and links exactly `SceIofilemgr_stub`, `SceKernelModulemgr_stub`, `SceLibKernel_stub`, `SceShaccCg_stub`, `SceShaccCgExt`, and the shared artifact writer dependencies.

- [ ] **Step 4: Run source audits**

Run the command from Step 2.

Expected: PASS.

### Task 5: Compile Standard Shader requests and cook artifact-backed textured materials

**Files:**

- Modify: `builder/PsVitaPlatformAssetBuilder.cs`
- Modify: `builder/PsVitaPlatformDefinitionFactory.cs`
- Modify: `src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaCompiledShaderRuntimeMaterial.hpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmForwardStandardProgram.cpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmForwardStandardProgram.hpp`
- Test: `builder.tests/PsVitaPlatformAssetBuilderTests.cs`
- Test: `builder.tests/PsVitaGxmForwardStandardSourceAuditTests.cs`

- [ ] **Step 1: Write failing material contract and runtime source tests**

```csharp
[Fact]
public void CookMaterial_whenDiffuseTextureIsPresent_requestsThePsVitaForwardStandardVariant() {
    PlatformMaterialCookResult result = CreateBuilder().CookMaterial(CreateTexturedStandardMaterialRequest());

    PsVitaCompiledShaderMaterialAsset material = new PsVitaCompiledShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);

    Assert.Equal("ForwardStandardTextured", material.VariantName);
    Assert.True(material.RequiresDiffuseTexture);
}

[Fact]
public void Source_whenDrawingTexturedStandardMaterial_bindsTheDiffuseTextureBeforeTheIndexedDraw() {
    string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardStandardProgram.cpp"));

    Assert.Contains("sceGxmSetFragmentTexture", source, StringComparison.Ordinal);
    Assert.Contains("SCE_GXM_TEXTURE_FILTER_LINEAR", source, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaPlatformAssetBuilderTests.CookMaterial_whenDiffuseTextureIsPresent|FullyQualifiedName~PsVitaGxmForwardStandardSourceAuditTests" --no-restore
```

Expected: failure because no texture-aware Standard Shader contract or renderer program exists.

- [ ] **Step 3: Implement the first Standard Shader variant**

Use the `PsVita` compiler backend for the material's vertex/fragment requests. Persist both returned artifact hashes, `ForwardStandardTextured`, and `RequiresDiffuseTexture=true` in the cooked material payload.

In the runtime, attach the cooked diffuse texture before returning the compiled runtime material, require UV data and a Vita runtime texture for this variant, bind it through `sceGxmSetFragmentTexture`, and draw with the artifact-backed program. Keep untextured `ForwardLambertOpaque` behavior intact. Throw a clear material contract error when the variant requires data that is absent.

- [ ] **Step 4: Run focused tests**

Run the command from Step 2 plus:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests.Source_whenForwardLambertCannotDraw" --no-restore
```

Expected: PASS; no test may reintroduce a CPU 3D fallback.

### Task 6: Produce and validate the compiler VPK and textured grid

**Files:**

- Modify: `builder/PsVitaNativeBuildExecutor.cs` or create `builder/PsVitaShaderCompilerVpkBuildExecutor.cs` for the tool-specific CMake build.
- Test: `builder.tests/PsVitaShaderCompilerVpkBuildExecutorTests.cs`

- [ ] **Step 1: Write the failing compiler-VPK executor test**

```csharp
[Fact]
public void Build_compilerTool_returnsTheSeparateShaderCompilerVpk() {
    RecordingProcessExecutor processExecutor = new();
    PsVitaShaderCompilerVpkBuildExecutor executor = new(processExecutor);

    string vpkPath = executor.Build(RepositoryRootPath, OutputRootPath, CancellationToken.None);

    Assert.EndsWith("helengine_psvita_shader_compiler.vpk", vpkPath, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderCompilerVpkBuildExecutorTests" --no-restore
```

Expected: failure because the separate tool executor does not exist.

- [ ] **Step 3: Implement the tool build executor**

Build `tools/shader-compiler` through the existing VitaSDK Docker image and return `helengine_psvita_shader_compiler.vpk`. Do not alter the game VPK's title ID, output location, or build executor.

- [ ] **Step 4: Run all new host tests**

Run:

```powershell
rtk dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderCompiler|FullyQualifiedName~PsVitaPlatformAssetBuilderTests|FullyQualifiedName~PsVitaGxmForwardStandardSourceAuditTests" --no-restore
```

Expected: PASS.

- [ ] **Step 5: Hardware checkpoint**

1. Build the standalone compiler VPK through the new executor.
2. Install it on a real Vita with `libshacccg.suprx` already installed at `ur0:data/libshacccg.suprx`.
3. Transfer one generated inbox bundle to `ux0:data/helengine_shader_compiler/inbox/`.
4. Launch the compiler VPK and confirm it exits.
5. Transfer `outbox/<jobHash>/` back to the host exchange root.
6. Run the editor CLI PS Vita build again.
7. Install the produced game VPK in Vita3K and real Vita; verify Textured Cube Grid displays its real textures rather than white and that no CPU 3D fallback runs.

## Plan self-review

- Every design requirement maps to a task: shared target (Task 1), deterministic exchange (Tasks 2–3), launch-and-exit compiler VPK (Task 4), Standard Shader textured vertical slice (Task 5), and hardware tool/game validation (Task 6).
- The plan uses test-first steps before every production change.
- The plan contains no implementation placeholders and deliberately leaves later Standard Shader variants outside the first vertical slice.
