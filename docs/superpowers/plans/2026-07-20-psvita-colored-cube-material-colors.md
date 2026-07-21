# PS Vita Colored Cube Material Colors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Preserve authored standard-material base colors in the PS Vita CPU-projected `vita2d` 3D fallback so colored cubes render with their intended colors.

**Architecture:** Keep the existing CPU transform, cull, Lambert, painter-sort, and `vita2d` triangle submission pipeline. Fix the material-loading boundary: recognize cooked `ShaderMaterialAsset` payloads before they are reduced to `MaterialAsset`, decode its named `BaseColorBuffer` float4 into packed ABGR, and store that value in `PsVitaCompiledShaderRuntimeMaterial`. The runtime-compiled GXM shader path remains disabled.

**Tech Stack:** C++20 PS Vita native runtime, VitaSDK/vita2d, C# .NET 9 builder/source-audit tests, generated Helengine core asset types.

---

### Task 1: Add failing builder and native source-contract tests

**Files:**
- Modify: `builder.tests/PsVitaPlatformAssetBuilderTests.cs`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`
- Modify: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`

- [ ] **Step 1: Add the builder regression assertion.**

Extend the standard-material cook test with a non-white color such as `#3366cc`, then assert that the `BaseColorBuffer` payload is 16 bytes and its four little-endian float components are `0x33/255f`, `0x66/255f`, `0xcc/255f`, and `1f`. This proves the builder already emits the data the runtime must consume.

- [ ] **Step 2: Add the native source assertions.**

Extend `PsVitaRenderManager3DSourceAuditTests` to require a dedicated standard shader-material conversion path containing:

```csharp
Assert.Contains("ShaderMaterialAsset", source, StringComparison.Ordinal);
Assert.Contains("BaseColorBuffer", source, StringComparison.Ordinal);
Assert.Contains("SetBaseColorAbgr", source, StringComparison.Ordinal);
```

Also assert that the generic material fallback is not selected before the shader-material branch. Extend the solid-color audit to retain:

```csharp
Assert.Contains("constexpr bool EnableRuntimeCompiledSolidColorProgram = false;", rendererSource, StringComparison.Ordinal);
```

- [ ] **Step 3: Run the focused tests and verify they fail for the missing native extraction.**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --no-restore --filter "FullyQualifiedName~PsVitaPlatformAssetBuilderTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests|FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests" -m:1 -nr:false -v minimal
```

Expected: the builder assertion passes, while the new native source assertions fail because the current loader discards standard shader-material color data.

- [ ] **Step 4: Commit the red tests.**

```powershell
git add builder.tests\PsVitaPlatformAssetBuilderTests.cs builder.tests\PsVitaRenderManager3DSourceAuditTests.cs builder.tests\PsVitaGxmSolidColorProgramSourceAuditTests.cs
git commit -m "test: specify PS Vita colored material fallback"
```

### Task 2: Add a focused Vita material color decoder

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaMaterialColorDecoder.hpp`
- Create: `src/platform/psvita/rendering/PsVitaMaterialColorDecoder.cpp`
- Modify: `CMakeLists.txt`
- Modify: `builder.tests/PsVitaRenderManager3DSourceAuditTests.cs`

- [ ] **Step 1: Define the decoder contract.**

Add one focused type with a documented API:

```cpp
class PsVitaMaterialColorDecoder final {
public:
    static std::uint32_t DecodeBaseColorAbgr(const std::vector<std::uint8_t>& data);
};
```

The method must require exactly four finite little-endian float channels, clamp each channel to `[0, 1]`, convert to bytes with round-to-nearest, and return ABGR in the existing Vita convention. Throw a descriptive native exception for missing, short, oversized, or non-finite data.

- [ ] **Step 2: Add the source to the native build.**

Add `PsVitaMaterialColorDecoder.cpp` to the Vita native source list in `CMakeLists.txt` beside the other rendering sources.

- [ ] **Step 3: Add source-contract checks for validation and byte order.**

Require the decoder source to contain the exact behaviors used by the runtime contract:

```csharp
Assert.Contains("BaseColorBuffer", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("DecodeBaseColorAbgr", renderManagerSource, StringComparison.Ordinal);
Assert.Contains("std::memcpy", decoderSource, StringComparison.Ordinal);
Assert.Contains("std::isfinite", decoderSource, StringComparison.Ordinal);
Assert.Contains("std::clamp", decoderSource, StringComparison.Ordinal);
```

- [ ] **Step 4: Run the focused tests and verify the decoder contract is still red until wired.**

Run the same focused test command from Task 1. Expected: the new decoder source checks fail until the files and build registration exist.

- [ ] **Step 5: Commit the decoder.**

```powershell
git add src/platform/psvita/rendering/PsVitaMaterialColorDecoder.hpp src/platform/psvita/rendering/PsVitaMaterialColorDecoder.cpp CMakeLists.txt builder.tests/PsVitaRenderManager3DSourceAuditTests.cs
git commit -m "feat: add PS Vita material base color decoder"
```

### Task 3: Preserve standard shader-material colors during runtime loading

**Files:**
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp`

- [ ] **Step 1: Add a standard shader-material conversion helper declaration.**

Declare a private helper that accepts `::ShaderMaterialAsset*`, creates `PsVitaCompiledShaderRuntimeMaterial`, copies the material id/render state and shader metadata, locates the `BaseColorBuffer`, decodes it through `PsVitaMaterialColorDecoder`, and stores the resulting ABGR.

- [ ] **Step 2: Route shader-material payloads before generic material reduction.**

In `BuildMaterialFromCooked(std::string, IContentStreamSource*)`, handle both the explicit shader-material binary-header path and the generic deserialized asset path through the shader-material overload before the `MaterialAsset` cast. Do not call the generic overload for a `ShaderMaterialAsset`.

- [ ] **Step 3: Keep defaults and existing paths explicit.**

For the existing dedicated `PVMT` payload, preserve its serialized `BaseColorAbgr`. For a standard shader material, require the `BaseColorBuffer` binding and decode it; do not silently substitute white for malformed data. Leave ordinary generic `MaterialAsset` payloads on their existing opaque-white fallback because they have no color field.

- [ ] **Step 4: Run the focused tests and verify green.**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --no-restore --filter "FullyQualifiedName~PsVitaPlatformAssetBuilderTests|FullyQualifiedName~PsVitaRenderManager3DSourceAuditTests|FullyQualifiedName~PsVitaGxmSolidColorProgramSourceAuditTests" -m:1 -nr:false -v minimal
```

Expected: PASS, including the assertions proving authored colors reach `SetBaseColorAbgr` and the runtime shader path remains disabled.

- [ ] **Step 5: Commit the runtime integration.**

```powershell
git add src/platform/psvita/rendering/PsVitaRenderManager3D.hpp src/platform/psvita/rendering/PsVitaRenderManager3D.cpp
git commit -m "feat: preserve PS Vita standard material colors"
```

### Task 4: Build and validate the Vita artifact

**Files:**
- No source changes expected.

- [ ] **Step 1: Run the complete Vita builder test project.**

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --no-restore -m:1 -nr:false -v minimal
```

Expected: all Vita builder tests pass; existing unrelated worktree changes remain present and unmodified.

- [ ] **Step 2: Run the smallest native build validation.**

Use the repository’s deterministic Vita Docker build:

```powershell
docker run --rm -v C:\dev\helworks\helengine-psvita:/workspace -w /workspace helengine-psvita make
```

Expected: `build/helengine_psvita.vpk` is produced without compilation errors.

- [ ] **Step 3: Verify repository state and artifact.**

```powershell
Get-Item build\helengine_psvita.vpk | Select-Object FullName,Length,LastWriteTime
git status --short
```

Expected: the VPK exists, and only the pre-existing unrelated modifications plus the implementation commits are shown.
