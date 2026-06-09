# PS Vita Shared Solid-Color Shader Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile one shared minimal `ForwardSolidColorShader` through a renderer-agnostic Helengine shader pipeline for DirectX 11, Vulkan, and PS Vita, then use the cooked Vita result to render `cube_test` with a real GPU shader instead of the current `vita2d` fallback/experiment path.

**Architecture:** Keep shader authoring and package flow shared in `helengine`, but keep every backend implementation in its own backend-owned package or external extension. Shared packages must not directly reference DirectX 11, Vulkan, PS Vita, or any other renderer/backend assembly; bootstrap code must explicitly register available shader backends into shared compile orchestration. All PS Vita-specific code lives in `helengine-psvita`, and if editor code needs PS Vita shader support it must load the PS Vita implementation dynamically rather than adding a compile-time dependency from shared/editor assemblies.

**Tech Stack:** `helengine.shader`, `helengine.editor`, `helengine.directx11`, `helengine.vulkan`, `helengine-psvita` builder/runtime, dynamic platform-extension loading, City authored scene generation, VitaSDK Docker image, GXM.

---

## Workspace Layout

**PS Vita repo stays on `main`.**

**Create worktrees before touching cross-repo code:**
- `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline` for `C:\dev\helworks\helengine`
- `C:\tmp\city-worktrees\psvita-shared-shader-pipeline` for `C:\dev\helprojs\city`

**Current status:** Tasks 1-4 were already completed in earlier execution and committed separately:
- `helengine-psvita`: `ffc579f` `revert: remove borrowed vita2d shader experiment`
- `helengine` worktree: `db9be7e` `feat: add PS Vita shader compile target`
- `helengine` worktree: `72862d0` `feat: add shared forward solid color shader`
- `helengine` worktree: `019421b` `feat: add shader backend registry seam`

**Primary files this plan touches**

- **helengine worktree**
  - Modify: `engine/helengine.shader/shaders/compilation/ShaderCompileTarget.cs`
  - Modify: `engine/helengine.shader/shaders/compilation/ShaderTargetNames.cs`
  - Modify: `engine/helengine.shader/shaders/compilation/ShaderPlatformDefines.cs`
  - Create: `engine/helengine.shader/shaders/compilation/ShaderBackendRegistry.cs`
  - Modify: `engine/helengine.editor/shaders/ShaderModuleManagerOptions.cs`
  - Modify: `engine/helengine.editor/shaders/ShaderModuleManager.cs`
  - Modify: `engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs`
  - Modify: `engine/helengine.editor/EditorSession.cs`
  - Modify: `engine/helengine.editor/EditorCliCommandRunner.cs`
  - Modify: `engine/helengine.render.validation/RenderShaderFactory.cs`
  - Create: `engine/helengine.editor/shaders/builtin/ForwardSolidColorShader.hlsl`
  - Create: `engine/helengine.editor.tests/shaders/ForwardSolidColorShaderTests.cs`
  - Create: `engine/helengine.editor.tests/shaders/PsVitaShaderPackagePipelineTests.cs`

- **helengine-psvita repo**
  - Modify: `builder/PsVitaPlatformAssetBuilder.cs`
  - Modify: `builder/PsVitaPlatformDefinitionFactory.cs`
  - Modify: `builder/helengine.psvita.builder.csproj`
  - Create: `builder/PsVitaCompiledShader.cs`
  - Create: `builder/PsVitaShaderCompiler.cs`
  - Create: `builder/PsVitaShaderBackend.cs`
  - Create: `builder/PsVitaShaderBackendRegistration.cs`
  - Create: `builder/PsVitaEditorShaderBackendExtension.cs`
  - Create: `builder.tests/PsVitaShaderBackendTests.cs`
  - Modify: `CMakeLists.txt`
  - Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.cpp`
  - Modify: `src/platform/psvita/rendering/PsVitaGxmRenderer.hpp`
  - Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
  - Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp`
  - Create: `builder/PsVitaCompiledShaderMaterialAsset.cs`
  - Create: `builder/PsVitaCompiledShaderMaterialBinarySerializer.cs`
  - Create: `builder.tests/PsVitaCompiledShaderMaterialSourceAuditTests.cs`
  - Create: `src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp`
  - Create: `src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.cpp`
  - Create: `src/platform/psvita/rendering/PsVitaGxmShaderProgram.hpp`
  - Create: `src/platform/psvita/rendering/PsVitaGxmShaderProgram.cpp`

- **city worktree**
  - Modify: `assets/codebase/rendering.tools/RenderingSceneGenerationAssets.cs`
  - Modify: `assets/codebase/rendering.tools/RenderingSceneAssetPreparationService.cs`
  - Modify: `assets/codebase/rendering.tools/CubeTestSceneFactory.cs`
  - Modify: `assets/codebase/rendering.tools/RenderingSceneGenerator.cs`
  - Create: `assets/codebase/rendering.tools/ForwardSolidColorMaterialFactory.cs`
  - Create: `assets/codebase/rendering.tools.tests/CubeTestSolidColorShaderSourceTests.cs`

---

### Task 1: Reset the PS Vita runtime path to a clean baseline

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmSolidColorProgram.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaGxmSolidColorProgramSourceAuditTests.cs`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Write the failing architectural audit**

```csharp
[Fact]
public void Source_whenUsingProgrammableVitaMeshPath_doesNotBorrowVita2dShaderGlobals() {
    string sourceCode = File.ReadAllText(GetRendererPath());
    Assert.DoesNotContain("_vita2d_colorVertexProgram", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("_vita2d_colorFragmentProgram", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("_vita2d_colorWvpParam", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("_vita2d_ortho_matrix", sourceCode, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the audit to verify it fails**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests"
```

Expected: FAIL because the current experimental code still references borrowed `vita2d` shader internals.

- [ ] **Step 3: Revert the off-track renderer experiment**

```cpp
bool PsVitaGxmRenderer::DrawSolidColorMesh(
    const ::float4x4& worldViewProjection,
    const ::float3* positions,
    int32_t positionCount,
    const std::uint32_t* indices,
    int32_t indexCount,
    std::uint32_t colorAbgr) {
    (void)worldViewProjection;
    (void)positions;
    (void)positionCount;
    (void)indices;
    (void)indexCount;
    (void)colorAbgr;
    return false;
}
```

- [ ] **Step 4: Run the audit to verify it passes**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests"
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/platform/psvita/rendering/PsVitaGxmRenderer.cpp src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs builder.tests/PsVitaRenderManager3DSourceAuditTests.cs
git commit -m "revert: remove borrowed vita2d shader experiment"
```

---

### Task 2: Add `PsVita` as a first-class shared shader target

**Files:**
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.shader\shaders\compilation\ShaderCompileTarget.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.shader\shaders\compilation\ShaderTargetNames.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.shader\shaders\compilation\ShaderPlatformDefines.cs`
- Test: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor.tests\shaders\PsVitaShaderPackagePipelineTests.cs`

- [ ] **Step 1: Write the failing target-name test**

```csharp
[Fact]
public void GetTargetName_whenTargetIsPsVita_returnsPsvita() {
    Assert.Equal("psvita", ShaderTargetNames.GetTargetName(ShaderCompileTarget.PsVita));
}

[Fact]
public void TryParseTarget_whenNameIsPsvita_returnsPsVita() {
    Assert.True(ShaderTargetNames.TryParseTarget("psvita", out ShaderCompileTarget target));
    Assert.Equal(ShaderCompileTarget.PsVita, target);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
```

Expected: FAIL because `PsVita` does not exist yet.

- [ ] **Step 3: Add the shared target plumbing**

```csharp
public enum ShaderCompileTarget {
    DirectX9,
    DirectX11,
    DirectX12,
    Vulkan,
    Metal,
    PsVita
}
```

```csharp
case ShaderCompileTarget.PsVita:
    return "psvita";
```

```csharp
case "psvita":
    target = ShaderCompileTarget.PsVita;
    return true;
```

```csharp
case ShaderCompileTarget.PsVita:
    return "HELENGINE_PLATFORM_PSVITA";
```

- [ ] **Step 4: Run the test to verify it passes**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add engine/helengine.shader/shaders/compilation/ShaderCompileTarget.cs engine/helengine.shader/shaders/compilation/ShaderTargetNames.cs engine/helengine.shader/shaders/compilation/ShaderPlatformDefines.cs engine/helengine.editor.tests/shaders/PsVitaShaderPackagePipelineTests.cs
git commit -m "feat: add PS Vita shader compile target"
```

---

### Task 3: Add one shared minimal `ForwardSolidColorShader`

**Files:**
- Create: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\shaders\builtin\ForwardSolidColorShader.hlsl`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\shaders\EditorBuiltInShaderAssetLibrary.cs`
- Test: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor.tests\shaders\ForwardSolidColorShaderTests.cs`

- [ ] **Step 1: Write the failing shared-shader compilation tests**

```csharp
[Theory]
[InlineData(ShaderCompileTarget.DirectX11)]
[InlineData(ShaderCompileTarget.Vulkan)]
public void LoadShaderAsset_whenForwardSolidColorShaderIsRequested_compilesForDesktopTargets(ShaderCompileTarget target) {
    ShaderAsset shaderAsset = EditorBuiltInShaderAssetLibrary.LoadShaderAsset(target, "ForwardSolidColorShader.hlsl");
    Assert.Equal("ForwardSolidColorShader", shaderAsset.Id);
    Assert.Equal(ShaderTargetNames.GetTargetName(target), shaderAsset.TargetName);
    Assert.Equal(4, shaderAsset.Binaries.Length);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~ForwardSolidColorShaderTests"
```

Expected: FAIL because the shader file does not exist yet.

- [ ] **Step 3: Add the shared shader source**

```hlsl
cbuffer HelenginePerDraw : register(b0)
{
    float4x4 HelengineWorldViewProjection;
    float4 HelengineBaseColor;
};

struct VS_IN
{
    float3 pos : POSITION;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
};

PS_IN VS(VS_IN input)
{
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), HelengineWorldViewProjection);
    return output;
}

float4 PS(PS_IN input) : SV_TARGET
{
    return HelengineBaseColor;
}
```

- [ ] **Step 4: Add built-in variant support**

```csharp
if (string.Equals(shaderName, "ForwardStandardShader", StringComparison.Ordinal)
    || string.Equals(shaderName, "ForwardSolidColorShader", StringComparison.Ordinal)) {
    return new[] {
        DefaultVariantName,
        MeshVariantName
    };
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~ForwardSolidColorShaderTests"
```

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add engine/helengine.editor/shaders/builtin/ForwardSolidColorShader.hlsl engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs engine/helengine.editor.tests/shaders/ForwardSolidColorShaderTests.cs
git commit -m "feat: add shared forward solid color shader"
```

---

### Task 4: Introduce one renderer-agnostic shader backend registration seam

**Files:**
- Create: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.shader\shaders\compilation\ShaderBackendRegistry.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\shaders\ShaderModuleManagerOptions.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\shaders\ShaderModuleManager.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\shaders\EditorBuiltInShaderAssetLibrary.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\EditorSession.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor\EditorCliCommandRunner.cs`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.render.validation\RenderShaderFactory.cs`
- Test: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor.tests\shaders\PsVitaShaderPackagePipelineTests.cs`

- [ ] **Step 1: Write the failing architecture audit**

```csharp
[Fact]
public void SharedShaderCompilationSource_whenInspected_doesNotDirectlyInstantiateRendererSpecificBackends() {
    string moduleManagerSource = File.ReadAllText(Path.Combine(GetHelengineRoot(), "engine", "helengine.editor", "shaders", "ShaderModuleManager.cs"));
    string builtInLibrarySource = File.ReadAllText(Path.Combine(GetHelengineRoot(), "engine", "helengine.editor", "shaders", "EditorBuiltInShaderAssetLibrary.cs"));
    string renderValidationSource = File.ReadAllText(Path.Combine(GetHelengineRoot(), "engine", "helengine.render.validation", "RenderShaderFactory.cs"));

    Assert.DoesNotContain("new helengine.directx11.DirectX11ShaderBackend()", moduleManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("new helengine.vulkan.VulkanShaderBackend()", moduleManagerSource, StringComparison.Ordinal);
    Assert.DoesNotContain("new helengine.directx11.DirectX11ShaderBackend()", builtInLibrarySource, StringComparison.Ordinal);
    Assert.DoesNotContain("new helengine.vulkan.VulkanShaderBackend()", builtInLibrarySource, StringComparison.Ordinal);
    Assert.DoesNotContain("new DirectX11ShaderBackend()", renderValidationSource, StringComparison.Ordinal);
    Assert.DoesNotContain("new VulkanShaderBackend()", renderValidationSource, StringComparison.Ordinal);
    Assert.Contains("ShaderBackendRegistry", moduleManagerSource, StringComparison.Ordinal);
    Assert.Contains("ShaderBackendRegistry", builtInLibrarySource, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
```

Expected: FAIL because shared/editor code still constructs DirectX 11 and Vulkan backends directly.

- [ ] **Step 3: Add the shared registry**

```csharp
public sealed class ShaderBackendRegistry {
    readonly Dictionary<ShaderCompileTarget, IShaderBackend> backends = new();

    public void Register(IShaderBackend backend) {
        if (backend == null) {
            throw new ArgumentNullException(nameof(backend));
        }

        backends[backend.Target] = backend;
    }

    public void RegisterInto(ShaderCompileService service) {
        if (service == null) {
            throw new ArgumentNullException(nameof(service));
        }

        foreach (IShaderBackend backend in backends.Values) {
            service.RegisterBackend(backend);
        }
    }
}
```

- [ ] **Step 4: Thread the registry through shared/editor shader orchestration**

```csharp
public ShaderModuleManagerOptions(
    string shaderRootPath,
    string packageOutputPath,
    ShaderPackageBuildOptions buildOptions,
    ShaderCompileTarget runtimeTarget,
    int buildDelayMilliseconds,
    ShaderBackendRegistry backendRegistry) {
    ...
    BackendRegistry = backendRegistry ?? throw new ArgumentNullException(nameof(backendRegistry));
}
```

```csharp
ShaderCompileService BuildCompileService() {
    ShaderFilesystemIncludeResolver includeResolver = new ShaderFilesystemIncludeResolver(options.ShaderRootPath);
    ShaderMemoryCompileCache cache = new ShaderMemoryCompileCache();
    ShaderSourceHasher hasher = new ShaderSourceHasher();
    ShaderCompileService service = new ShaderCompileService(includeResolver, cache, hasher);
    options.BackendRegistry.RegisterInto(service);
    return service;
}
```

```csharp
static ShaderBackendRegistry BackendRegistry = new ShaderBackendRegistry();

public static void ConfigureShaderBackends(ShaderBackendRegistry backendRegistry) {
    BackendRegistry = backendRegistry ?? throw new ArgumentNullException(nameof(backendRegistry));
}
```

- [ ] **Step 5: Make bootstrap callers pass explicit registries**

```csharp
ShaderBackendRegistry backendRegistry = CreateShaderBackendRegistry(runtimeTarget);
EditorBuiltInShaderAssetLibrary.ConfigureShaderBackends(backendRegistry);
return new ShaderModuleManager(new ShaderModuleManagerOptions(
    shaderRootPath,
    packageOutputPath,
    buildOptions,
    runtimeTarget,
    ShaderBuildDelayMilliseconds,
    backendRegistry));
```

```csharp
static ShaderCompileService CreateCompileService(ShaderCompileTarget target, ShaderBackendRegistry backendRegistry) {
    ...
    backendRegistry.RegisterInto(service);
    return service;
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
```

Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add engine/helengine.shader/shaders/compilation/ShaderBackendRegistry.cs engine/helengine.editor/shaders/ShaderModuleManagerOptions.cs engine/helengine.editor/shaders/ShaderModuleManager.cs engine/helengine.editor/shaders/EditorBuiltInShaderAssetLibrary.cs engine/helengine.editor/EditorSession.cs engine/helengine.editor/EditorCliCommandRunner.cs engine/helengine.render.validation/RenderShaderFactory.cs engine/helengine.editor.tests/shaders/PsVitaShaderPackagePipelineTests.cs
git commit -m "refactor: inject shader backends through shared registry"
```

---

### Task 5: Add one PS Vita shader backend implementation in `helengine-psvita` and load it dynamically

**Files:**
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaCompiledShader.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaShaderCompiler.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaShaderBackend.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaShaderBackendRegistration.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaEditorShaderBackendExtension.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaShaderBackendTests.cs`
- Modify: `C:\dev\helworks\helengine-psvita\builder\helengine.psvita.builder.csproj`
- Modify: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline\engine\helengine.editor.tests\shaders\PsVitaShaderPackagePipelineTests.cs`

- [ ] **Step 1: Write the failing PS Vita backend tests**

```csharp
[Fact]
public void Compile_whenForwardSolidColorShaderIsRequestedForPsVita_returnsEmbeddedBytecode() {
    ShaderBackendRegistry backendRegistry = new ShaderBackendRegistry();
    new PsVitaEditorShaderBackendExtension().Register(backendRegistry);
    EditorBuiltInShaderAssetLibrary.ConfigureShaderBackends(backendRegistry);

    ShaderAsset shaderAsset = EditorBuiltInShaderAssetLibrary.LoadShaderAsset(ShaderCompileTarget.PsVita, "ForwardSolidColorShader.hlsl");

    Assert.Contains(shaderAsset.Binaries, binary =>
        binary.ProgramName == "ForwardSolidColorShader.vs" &&
        binary.TargetName == "psvita" &&
        binary.Variant == "Mesh" &&
        binary.Bytes.Length > 0);
    Assert.Contains(shaderAsset.Binaries, binary =>
        binary.ProgramName == "ForwardSolidColorShader.ps" &&
        binary.TargetName == "psvita" &&
        binary.Variant == "Mesh" &&
        binary.Bytes.Length > 0);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBackendTests"
```

Expected: FAIL because no PS Vita shader backend implementation or dynamic editor extension exists in `helengine-psvita` yet.

- [ ] **Step 3: Discover and lock the PS Vita shader compiler command**

Run:

```powershell
rtk proxy docker run --rm vitasdk/vitasdk:latest sh -lc "ls /usr/local/vitasdk/bin | grep -E 'cgc|shader|gxp|gxm|psp2' | sort"
```

Expected: identify the actual Vita shader-stage compiler and any packer/linker required for vertex and fragment programs.

- [ ] **Step 4: Add the PS Vita backend implementation and dynamic editor extension**

```csharp
public sealed class PsVitaCompiledShader {
    public PsVitaCompiledShader(byte[] bytecode) {
        Bytecode = bytecode ?? throw new ArgumentNullException(nameof(bytecode));
    }

    public byte[] Bytecode { get; }
}
```

```csharp
public sealed class PsVitaShaderBackend : IShaderBackend {
    readonly PsVitaShaderCompiler Compiler = new PsVitaShaderCompiler();

    public ShaderCompileTarget Target => ShaderCompileTarget.PsVita;

    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        PsVitaCompiledShader compiledShader = request.Stage == ShaderStage.Vertex
            ? Compiler.CompileVertexStage(request.Source.Path, request.EntryPoint, Path.GetTempPath())
            : Compiler.CompilePixelStage(request.Source.Path, request.EntryPoint, Path.GetTempPath());

        return new ShaderCompileResult(
            request.ProgramName,
            request.Stage,
            new ShaderCompiledBinary(compiledShader.Bytecode),
            Array.Empty<ShaderCompileDiagnostic>(),
            new ShaderProgramDefinition(request.ProgramName, request.Stage, request.EntryPoint, Array.Empty<ShaderParameterDefinition>(), Array.Empty<ShaderResourceBindingDefinition>(), Array.Empty<ShaderConstantBufferDefinition>()));
    }
}
```

```csharp
public sealed class PsVitaEditorShaderBackendExtension {
    public void Register(ShaderBackendRegistry backendRegistry) {
        if (backendRegistry == null) {
            throw new ArgumentNullException(nameof(backendRegistry));
        }

        backendRegistry.Register(new PsVitaShaderBackend());
    }
}
```

- [ ] **Step 5: Run the tests to verify PS Vita shader packages compile**

Run:

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~PsVitaShaderPackagePipelineTests"
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBackendTests"
```

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add builder/PsVitaCompiledShader.cs builder/PsVitaShaderCompiler.cs builder/PsVitaShaderBackend.cs builder/PsVitaShaderBackendRegistration.cs builder/PsVitaEditorShaderBackendExtension.cs builder/helengine.psvita.builder.csproj builder.tests/PsVitaShaderBackendTests.cs
git commit -m "feat: add PS Vita shader backend extension"
```

---

### Task 6: Translate shader-backed materials into one Vita-native cooked material payload

**Files:**
- Modify: `C:\dev\helworks\helengine-psvita\builder\PsVitaPlatformAssetBuilder.cs`
- Modify: `C:\dev\helworks\helengine-psvita\builder\PsVitaPlatformDefinitionFactory.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaCompiledShaderMaterialAsset.cs`
- Create: `C:\dev\helworks\helengine-psvita\builder\PsVitaCompiledShaderMaterialBinarySerializer.cs`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaCompiledShaderMaterialSourceAuditTests.cs`

- [ ] **Step 1: Write the failing builder test**

```csharp
[Fact]
public void CookMaterial_whenShaderFieldsArePresent_returnsReferencedShaderAssetId() {
    PsVitaPlatformAssetBuilder builder = new PsVitaPlatformAssetBuilder();
    PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
        "CubeTestSolid",
        "materials/rendering/cube_test_solid.hasset",
        "psvita",
        "debug",
        "default",
        "standard-shader",
        new Dictionary<string, string> {
            ["shader-asset-id"] = "ForwardSolidColorShader",
            ["vertex-program"] = "ForwardSolidColorShader.vs",
            ["pixel-program"] = "ForwardSolidColorShader.ps",
            ["variant"] = "Mesh",
            ["base-color"] = "#ffffffff"
        }));

    Assert.Contains(result.ReferencedShaderAssetIds, id => id == "ForwardSolidColorShader");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaCompiledShaderMaterialSourceAuditTests|FullyQualifiedName~PsVitaPlatformAssetBuilderTests"
```

Expected: FAIL because the builder still cooks only the old base-color payload.

- [ ] **Step 3: Add the cooked material asset and serializer**

```csharp
public sealed class PsVitaCompiledShaderMaterialAsset {
    public string ShaderAssetId { get; set; }
    public string VertexProgramName { get; set; }
    public string PixelProgramName { get; set; }
    public string VariantName { get; set; }
    public uint BaseColorAbgr { get; set; }
}
```

```csharp
public sealed class PsVitaCompiledShaderMaterialBinarySerializer {
    public byte[] Serialize(PsVitaCompiledShaderMaterialAsset asset) { ... }
    public PsVitaCompiledShaderMaterialAsset Deserialize(byte[] bytes) { ... }
}
```

- [ ] **Step 4: Cook the shared shader-backed material payload**

```csharp
if (request.MaterialFields.TryGetValue("shader-asset-id", out string shaderAssetId)) {
    PsVitaCompiledShaderMaterialAsset cookedAsset = new PsVitaCompiledShaderMaterialAsset {
        ShaderAssetId = shaderAssetId,
        VertexProgramName = request.MaterialFields["vertex-program"],
        PixelProgramName = request.MaterialFields["pixel-program"],
        VariantName = request.MaterialFields["variant"],
        BaseColorAbgr = ParseColor(request.MaterialFields["base-color"])
    };

    return new PlatformMaterialCookResult(
        new PsVitaCompiledShaderMaterialBinarySerializer().Serialize(cookedAsset),
        [shaderAssetId]);
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaCompiledShaderMaterialSourceAuditTests|FullyQualifiedName~PsVitaPlatformAssetBuilderTests"
```

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add builder/PsVitaPlatformAssetBuilder.cs builder/PsVitaPlatformDefinitionFactory.cs builder/PsVitaCompiledShaderMaterialAsset.cs builder/PsVitaCompiledShaderMaterialBinarySerializer.cs builder.tests/PsVitaCompiledShaderMaterialSourceAuditTests.cs
git commit -m "feat: cook PS Vita compiled shader materials"
```

---

### Task 7: Bind one real compiled GXM shader-program pair at runtime

**Files:**
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmShaderProgram.hpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmShaderProgram.cpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaCompiledShaderMaterialReader.hpp`
- Create: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaCompiledShaderMaterialReader.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.cpp`
- Modify: `C:\dev\helworks\helengine-psvita\src\platform\psvita\rendering\PsVitaGxmRenderer.hpp`
- Test: `C:\dev\helworks\helengine-psvita\builder.tests\PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Write the failing source audit**

```csharp
[Fact]
public void Source_whenBindingCompiledShaderPrograms_usesDedicatedProgramWrapperInsteadOfBorrowedVita2dHandles() {
    string sourceCode = File.ReadAllText(GetRendererPath());
    Assert.Contains("PsVitaGxmShaderProgram", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("_vita2d_colorVertexProgram", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("_vita2d_colorFragmentProgram", sourceCode, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the audit to verify it fails**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests"
```

Expected: FAIL because the runtime still has no compiled-program wrapper.

- [ ] **Step 3: Add one compiled-program wrapper and cooked-material reader**

```cpp
class PsVitaGxmShaderProgram final {
public:
    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    SceGxmVertexProgram* GetVertexProgram() const;
    SceGxmFragmentProgram* GetFragmentProgram() const;
};
```

```cpp
class PsVitaCompiledShaderMaterialReader final {
public:
    static bool TryRead(const std::string& path, PsVitaCompiledShaderMaterial& material);
};
```

- [ ] **Step 4: Bind real compiled shader programs in `PsVitaGxmRenderer`**

```cpp
bool PsVitaGxmRenderer::DrawSolidColorMesh(...) {
    if (!SolidColorProgram.IsReady()) {
        return false;
    }

    sceGxmSetVertexProgram(Context, SolidColorProgram.GetVertexProgram());
    sceGxmSetFragmentProgram(Context, SolidColorProgram.GetFragmentProgram());
    ...
}
```

- [ ] **Step 5: Run the audit to verify it passes**

Run:

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests|FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests"
```

Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/platform/psvita/rendering/PsVitaGxmShaderProgram.hpp src/platform/psvita/rendering/PsVitaGxmShaderProgram.cpp src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.cpp src/platform/psvita/rendering/PsVitaGxmRenderer.cpp src/platform/psvita/rendering/PsVitaGxmRenderer.hpp builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs
git commit -m "feat: bind compiled PS Vita shader programs"
```

---

### Task 8: Route `cube_test` through the shared solid-color material and verify end to end

**Files:**
- Modify: `C:\tmp\city-worktrees\psvita-shared-shader-pipeline\assets\codebase\rendering.tools\RenderingSceneGenerationAssets.cs`
- Modify: `C:\tmp\city-worktrees\psvita-shared-shader-pipeline\assets\codebase\rendering.tools\RenderingSceneAssetPreparationService.cs`
- Modify: `C:\tmp\city-worktrees\psvita-shared-shader-pipeline\assets\codebase\rendering.tools\CubeTestSceneFactory.cs`
- Create: `C:\tmp\city-worktrees\psvita-shared-shader-pipeline\assets\codebase\rendering.tools\ForwardSolidColorMaterialFactory.cs`
- Test: `C:\tmp\city-worktrees\psvita-shared-shader-pipeline\assets\codebase\rendering.tools.tests\CubeTestSolidColorShaderSourceTests.cs`
- Verify only: `C:\tmp\helengine-worktrees\psvita-shared-shader-pipeline`
- Verify only: `C:\dev\helworks\helengine-psvita`

- [ ] **Step 1: Write the failing authored-scene test**

```csharp
[Fact]
public void CubeTestScene_whenGenerated_usesForwardSolidColorShaderMaterial() {
    string sourceCode = File.ReadAllText(GetCubeTestFactoryPath());
    Assert.Contains("ForwardSolidColorShader", sourceCode, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
rtk proxy dotnet test assets/codebase/rendering.tools.tests/rendering.tools.tests.csproj --filter "FullyQualifiedName~CubeTestSolidColorShaderSourceTests"
```

Expected: FAIL because `cube_test` still uses the previous material path.

- [ ] **Step 3: Route `cube_test` through one shared shader-backed material**

```csharp
public static class ForwardSolidColorMaterialFactory {
    public static ShaderMaterialAsset Create(string materialId, string colorHex) {
        return new ShaderMaterialAsset {
            Id = materialId,
            ShaderAssetId = "ForwardSolidColorShader",
            VertexProgram = "ForwardSolidColorShader.vs",
            PixelProgram = "ForwardSolidColorShader.ps",
            Variant = "Mesh",
            Fields = new Dictionary<string, string> {
                ["base-color"] = colorHex
            }
        };
    }
}
```

- [ ] **Step 4: Run the authored-scene test to verify it passes**

Run:

```powershell
rtk proxy dotnet test assets/codebase/rendering.tools.tests/rendering.tools.tests.csproj --filter "FullyQualifiedName~CubeTestSolidColorShaderSourceTests"
```

Expected: PASS

- [ ] **Step 5: Run the Helengine shader verification**

```powershell
rtk proxy dotnet test engine/helengine.editor.tests/helengine.editor.tests.csproj --filter "FullyQualifiedName~ForwardSolidColorShaderTests|FullyQualifiedName~PsVitaShaderPackagePipelineTests"
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderBackendTests"
```

Expected: PASS

- [ ] **Step 6: Run the PS Vita builder/runtime verification**

```powershell
rtk proxy dotnet test builder.tests/helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaCompiledShaderMaterialSourceAuditTests|FullyQualifiedName~PsVitaGxmRenderPipelineSourceAuditTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests"
```

Expected: PASS

- [ ] **Step 7: Commit the City changes**

```bash
git add assets/codebase/rendering.tools/RenderingSceneGenerationAssets.cs assets/codebase/rendering.tools/RenderingSceneAssetPreparationService.cs assets/codebase/rendering.tools/CubeTestSceneFactory.cs assets/codebase/rendering.tools/ForwardSolidColorMaterialFactory.cs assets/codebase/rendering.tools.tests/CubeTestSolidColorShaderSourceTests.cs
git commit -m "feat: route cube_test through shared solid color material"
```
