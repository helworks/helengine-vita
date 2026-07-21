# PS Vita GPU Forward-Lambert Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace CPU-side Lambert evaluation with an artifact-backed native Vita GXM forward-Lambert material for opaque meshes.

**Architecture:** Build on the completed shader-artifact loader. Add explicit forward-Lambert shader variants and a focused GXM program wrapper, then bind transforms/light/material uniforms and normals through the existing 3D draw path. Keep CPU work to scene preparation and submission.

**Tech Stack:** C++20, VitaSDK GXM, Vita shader artifact format, generated core material/runtime types, C# source-audit tests, CMake, Vita3K plus real Vita validation.

---

## File map

- Create `src/platform/psvita/shaders/ForwardLambertShaderSource.hpp/.cpp`: shared vertex/fragment source and stable parameter names, following the existing embedded solid-color shader pattern.
- Create `src/platform/psvita/rendering/PsVitaGxmForwardLambertProgram.hpp/.cpp`: artifact-backed GXM program ownership and patching.
- Create `src/platform/psvita/rendering/PsVitaForwardLambertUniformBinder.hpp/.cpp`: uniform writes and parameter-contract validation.
- Modify `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp/.cpp`: material selection, uniform preparation, and draw binding.
- Create `src/platform/psvita/rendering/PsVitaForwardLambertVertex.hpp`: interleaved position/normal vertex layout for GPU upload.
- Modify `src/platform/psvita/rendering/PsVitaRuntimeModel.hpp/.cpp`: continue exposing the existing `Positions` and `Normals` vectors as the source for interleaved GPU vertices.
- Modify `src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp/.cpp` and `PsVitaCompiledShaderRuntimeMaterial.hpp/.cpp` for the non-generated variant/artifact fields; do not edit generated core output.
- Create `builder.tests/PsVitaForwardLambertSourceAuditTests.cs`: shader contract, CPU fallback, CMake, and runtime binding audits.
- Modify relevant `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs` and `CMakeLists.txt`.

### Task 1: Establish the shader and material contract

**Files:**
- Create: `src/platform/psvita/shaders/ForwardLambertShaderSource.hpp/.cpp`.
- Modify: `builder/PsVitaPlatformAssetBuilder.cs` and `builder/PsVitaCompiledShaderMaterialAsset.cs`.
- Create: `builder.tests/PsVitaForwardLambertSourceAuditTests.cs`.

- [ ] **Step 1: Write failing source-audit tests.**

Assert that the shader source contains `VS` and `PS` entry points, position and normal inputs, world-view-projection, normal transform, light direction/color, ambient, base color, and the Lambert clamp. Assert that the builder exposes the explicit `ForwardLambertOpaque` variant and requires vertex/fragment artifact identities.

- [ ] **Step 2: Run the focused audit and confirm failure.**

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter FullyQualifiedName~PsVitaForwardLambertSourceAuditTests 2>&1 | Select-Object -First 160
```

Expected: failure because the shader and variant contract do not exist.

- [ ] **Step 3: Add the minimal opaque shader source.**

Use explicit structures equivalent to:

```hlsl
cbuffer HelenginePerDraw : register(b0)
{
    float4x4 HelengineWorldViewProjection;
    float4x4 HelengineNormalTransform;
    float4 HelengineBaseColor;
    float4 HelengineLightDirection;
    float4 HelengineLightColor;
    float4 HelengineAmbient;
};

struct VS_IN { float3 pos : POSITION; float3 normal : NORMAL; };
struct PS_IN { float4 pos : SV_POSITION; float3 normal : TEXCOORD0; };

PS_IN VS(VS_IN input) {
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), HelengineWorldViewProjection);
    output.normal = normalize(mul(float4(input.normal, 0.0f), HelengineNormalTransform).xyz);
    return output;
}

float4 PS(PS_IN input) : SV_TARGET {
    float diffuse = max(dot(normalize(input.normal), -normalize(HelengineLightDirection.xyz)), 0.0f);
    return HelengineBaseColor * (HelengineAmbient + HelengineLightColor * diffuse);
}
```

Use the project’s actual shader-language conventions if they differ, while preserving the parameter names and semantic contract.

- [ ] **Step 4: Add the explicit material variant and artifact references.**

Make `ForwardLambertOpaque` a deliberate cooked variant. Require both artifacts and preserve the parameter-contract version. Do not make missing lighting fields silently mean CPU Lambert.

- [ ] **Step 5: Run the audit and commit the contract.**

Expected: shader/variant audit passes.

```powershell
git add src\platform\psvita\shaders builder builder.tests\PsVitaForwardLambertSourceAuditTests.cs
git commit -m "Define Vita forward Lambert shader contract"
```

### Task 2: Add the artifact-backed GXM Lambert program

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaGxmForwardLambertProgram.hpp`
- Create: `src/platform/psvita/rendering/PsVitaGxmForwardLambertProgram.cpp`
- Modify: `CMakeLists.txt`
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Add failing wrapper audits.**

Assert that the wrapper loads vertex/fragment artifacts, validates program headers, registers both with the shader patcher, creates vertex and fragment programs, finds all six required parameters, and exposes context/program/parameter accessors. Assert that it does not call `sceShaccCgCompileProgram`.

- [ ] **Step 2: Implement ownership and initialization.**

Follow the solid-color wrapper’s lifecycle, but keep the class focused on Lambert state. Load complete artifact blobs, allocate aligned persistent memory, create patcher backing and USSE buffers, register programs, configure a position+normal vertex stream, create the opaque fragment program, resolve parameters, and release all resources in reverse ownership order.

- [ ] **Step 3: Add exact vertex attribute configuration.**

Build `PsVitaForwardLambertVertex` values from `PsVitaRuntimeModel::GetPositions()` and `GetNormals()`. Define position offset `0`, normal offset `sizeof(float) * 3`, F32 component count `3` for both attributes, stream stride `sizeof(PsVitaForwardLambertVertex)`, and the existing 32-bit index source in `PsVitaForwardLambertVertex.hpp`. The GXM wrapper reads those constants; the draw manager does not duplicate them. Fail initialization if shader reflection does not contain the expected attributes.

- [ ] **Step 4: Build and run wrapper audits.**

Expected: native compilation/source audits pass; no runtime shader compiler dependency is introduced.

- [ ] **Step 5: Commit the wrapper.**

```powershell
git add src\platform\psvita\rendering CMakeLists.txt builder.tests
git commit -m "Add artifact-backed Vita Lambert GXM program"
```

### Task 3: Bind transforms, normals, lights, and material constants

**Files:**
- Create: `src/platform/psvita/rendering/PsVitaForwardLambertUniformBinder.hpp`
- Create: `src/platform/psvita/rendering/PsVitaForwardLambertUniformBinder.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaRenderManager3D.hpp/.cpp`
- Create: `builder.tests/PsVitaForwardLambertUniformSourceAuditTests.cs`

- [ ] **Step 1: Write failing binder tests/audits.**

Assert that the binder writes world-view-projection, normal transform, base color, light direction, light color, and ambient through GXM uniform APIs, validates all handles, and contains no vertex-loop lighting calculation.

- [ ] **Step 2: Implement the binder interface.**

Define one method that receives the active GXM context, program parameter handles, transform/light/material values, and the current uniform buffers. Validate required pointers/handles before writing. Keep matrix convention conversion in the binder so the draw manager remains presentation/orchestration code.

- [ ] **Step 3: Integrate the binder into opaque mesh submission.**

Select `ForwardLambertOpaque` from the cooked material, initialize the program once per artifact/material identity, bind vertex and fragment programs, set the vertex stream, write uniforms, and issue the existing GXM draw call. Preserve batching/state sort keys and avoid per-draw shader compilation or patcher creation.

- [ ] **Step 4: Remove CPU Lambert work from the production path.**

Delete or bypass only the per-vertex/per-fragment CPU intensity calculation for materials selecting the GPU variant. Keep any explicitly selected compatibility fallback isolated and observable. Do not rewrite generated code; modify the source generator/input if generated runtime fields need to change.

- [ ] **Step 5: Run focused tests.**

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaForwardLambert|FullyQualifiedName~PsVitaGxmRenderPipeline" 2>&1 | Select-Object -First 220
```

Expected: binder, renderer, and source-audit tests pass.

- [ ] **Step 6: Commit integration.**

```powershell
git add src\platform\psvita\rendering builder.tests
git commit -m "Render Vita Lambert lighting on the GPU"
```

### Task 4: Validate geometry, visual correctness, and fallback boundaries

**Files:**
- Modify: `builder.tests/PsVitaForwardLambertSourceAuditTests.cs`.
- Create: `docs/psvita-forward-lambert-validation.md`: the deterministic mesh, CPU-counter, Vita3K, real-Vita, and fallback checks.

- [ ] **Step 1: Add a deterministic normal-direction fixture.**

Use two opaque triangles or a small test mesh with opposing normals and one fixed directional light. Assert through the rendered test scene that the lit and unlit faces differ predictably and that reversing the light direction reverses the result.

- [ ] **Step 2: Add CPU-path instrumentation.**

Instrument the selected GPU material path with a counter for CPU Lambert evaluations and assert it remains zero while draw submission and uniform writes occur. Keep the instrumentation development-only and remove no useful renderer diagnostics.

- [ ] **Step 3: Validate artifact-backed startup.**

Boot with precompiled artifacts and confirm no `libshacccg.suprx` load occurs, shader initialization happens once, repeated draws reuse the same patched programs, and reset/reinitialize does not leak host or GXM allocations.

- [ ] **Step 4: Validate on Vita3K and real Vita separately.**

Use Vita3K for fast shader/material wiring checks. On real Vita, measure CPU submission time, draw count, scene breaks, GPU frame time, and parameter-buffer usage. Do not treat emulator timing as hardware performance evidence.

- [ ] **Step 5: Validate fallback policy.**

Remove one artifact and select the GPU material. Expected: explicit material/program failure. Select the documented compatibility fallback. Expected: fallback rendering with a diagnostic indicating that it was deliberately selected.

- [ ] **Step 6: Commit validation documentation.**

```powershell
git add docs\psvita-forward-lambert-validation.md builder.tests
git commit -m "Document Vita GPU Lambert validation"
```
