# PS Vita Shader Artifact Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export complete Vita `programData` shader blobs from a real Vita, cook them into engine assets, and load them without runtime shader compilation.

**Architecture:** Add a small Vita-side compiler/export command that emits versioned vertex/fragment artifacts. Extend the builder to validate and package those artifacts, then replace the runtime `shacccg` compilation in the GXM program wrapper with artifact loading while retaining runtime GXM shader patching.

**Tech Stack:** C++20, VitaSDK `SceShaccCg`/GXM, C#/.NET 9 builder and tests, CMake, existing staged-content and cooked-asset formats.

---

## File map

- Create `src/platform/psvita/shaders/PsVitaShaderArtifactFormat.hpp`: fixed artifact constants and serialized header definition.
- Create `src/platform/psvita/shaders/PsVitaShaderArtifactWriter.hpp/.cpp`: Vita compiler invocation, diagnostics, hash, and artifact writing.
- Create `src/platform/psvita/shaders/PsVitaShaderArtifactReader.hpp/.cpp`: runtime artifact loading and structural validation.
- Modify `src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp/.cpp`: consume stage artifacts and remove production runtime compilation.
- Create `builder/PsVitaShaderArtifact.cs`: immutable host representation of one validated stage artifact.
- Create `builder/PsVitaShaderArtifactBinarySerializer.cs`: host serializer/deserializer shared by importer and tests.
- Modify `builder/PsVitaCompiledShader.cs` and `builder/PsVitaCompiledShaderMaterialAsset.cs`: carry stage artifact metadata and bytes.
- Modify `builder/PsVitaCompiledShaderMaterialBinarySerializer.cs` and `builder/PsVitaPlatformAssetBuilder.cs`: cook versioned artifact-backed material payloads.
- Create `builder.tests/PsVitaShaderArtifactBinarySerializerTests.cs`: malformed-input and round-trip tests.
- Modify `builder.tests/PsVitaCompiledShaderMaterialSourceAuditTests.cs` and add `builder.tests/PsVitaShaderArtifactSourceAuditTests.cs`: source/build contract tests.
- Modify `CMakeLists.txt`: include the new Vita shader sources and any required link library.
- Create `docs/superpowers/specs/`-adjacent developer documentation only if the existing build/export command needs a user-facing invocation reference.

### Task 1: Lock the artifact contract with host tests

**Files:**
- Create: `builder/PsVitaShaderArtifact.cs`
- Create: `builder/PsVitaShaderArtifactBinarySerializer.cs`
- Create: `builder.tests/PsVitaShaderArtifactBinarySerializerTests.cs`
- Modify: `builder/PsVitaCompiledShader.cs`

- [ ] **Step 1: Write failing tests for the artifact header and round trip.**

Define tests for a vertex artifact and fragment artifact containing the same fields the Vita exporter will write: magic, version, stage/profile, compiler identity, source hash, entry point, option signature, and complete program bytes. Assert that serialization followed by deserialization preserves every field byte-for-byte.

- [ ] **Step 2: Write failing tests for rejection behavior.**

Cover truncated headers, invalid magic, unsupported version, zero program size, stage/profile mismatch, declared size larger than remaining input, invalid string length, hash mismatch, and trailing bytes under strict parsing. Each case must throw `InvalidOperationException` with a message naming the violated field.

- [ ] **Step 3: Run only the new tests and confirm failure.**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter FullyQualifiedName~PsVitaShaderArtifactBinarySerializerTests 2>&1 | Select-Object -First 160
```

Expected: compilation/test failure because the artifact model and serializer are not implemented.

- [ ] **Step 4: Implement the model and serializer.**

Use explicit little-endian `BinaryWriter`/`BinaryReader` operations, bounded UTF-8 string lengths, and a fixed header order. `PsVitaShaderArtifact` must expose immutable stage metadata and a defensive copy of program bytes. Compute the artifact hash over the canonical header fields plus program bytes; verify it during deserialization.

- [ ] **Step 5: Run the focused tests and confirm pass.**

Expected: all artifact serializer tests pass.

- [ ] **Step 6: Commit the contract.**

```powershell
git add builder\PsVitaShaderArtifact.cs builder\PsVitaShaderArtifactBinarySerializer.cs builder\PsVitaCompiledShader.cs builder.tests\PsVitaShaderArtifactBinarySerializerTests.cs
git commit -m "Define PS Vita shader artifact format"
```

### Task 2: Add the Vita compiler/export helper

**Files:**
- Create: `src/platform/psvita/shaders/PsVitaShaderArtifactFormat.hpp`
- Create: `src/platform/psvita/shaders/PsVitaShaderArtifactWriter.hpp`
- Create: `src/platform/psvita/shaders/PsVitaShaderArtifactWriter.cpp`
- Modify: `CMakeLists.txt`
- Create: `builder.tests/PsVitaShaderArtifactSourceAuditTests.cs`

- [ ] **Step 1: Add source-audit tests for the Vita compiler boundary.**

Assert that the writer references `sceShaccCgInitializeCompileOptions`, `sceShaccCgCompileProgram`, `sceShaccCgDestroyCompileOutput`, `sceShaccCgGetVersionString`, both vertex/fragment profiles, explicit program size handling, artifact magic/version, and failure propagation. Assert that the CMake source list contains the writer source and `shacccg` is linked.

- [ ] **Step 2: Run the source-audit test and confirm failure.**

Expected: missing-file/source assertions fail.

- [ ] **Step 3: Define the C++ artifact header matching the host format.**

Use fixed-width fields and explicit byte counts. Do not serialize pointers, `SceGxmProgram` objects, allocator addresses, or GXM patcher handles. Store the complete `programData` payload after the header.

- [ ] **Step 4: Implement source callbacks and compiler invocation.**

The writer must accept source text, source filename, profile, entry point, optimization/warning settings, and output path. Initialize compile options, provide a callback-backed source file, compile, copy `programData` for the exact `programSize`, obtain the compiler version, compute the canonical hash, write the artifact, destroy compiler output, and return a failure code for every failed operation.

- [ ] **Step 5: Add a bounded export command entrypoint.**

Expose one command used by the Vita development harness: input source path, output artifact path, profile, and entry point. Reject missing arguments and unsupported profiles. Keep transport/file-transfer code outside the writer so the artifact format remains reusable.

- [ ] **Step 6: Build the Vita target and rerun source audits.**

Run the repository’s smallest available native build command from the documented Vita container/toolchain. Expected: the new source compiles and source-audit tests pass. If the local environment lacks the Vita SDK, record the exact missing tool in the implementation handoff rather than weakening the code path.

- [ ] **Step 7: Commit the Vita exporter.**

```powershell
git add src\platform\psvita\shaders CMakeLists.txt builder.tests\PsVitaShaderArtifactSourceAuditTests.cs
git commit -m "Add Vita shader artifact exporter"
```

### Task 3: Cook artifact-backed materials on the host

**Files:**
- Modify: `builder/PsVitaCompiledShaderMaterialAsset.cs`
- Modify: `builder/PsVitaCompiledShaderMaterialBinarySerializer.cs`
- Modify: `builder/PsVitaPlatformAssetBuilder.cs`
- Modify: `builder.tests/PsVitaCompiledShaderMaterialSourceAuditTests.cs`
- Modify: `builder.tests/PsVitaPlatformAssetBuilderTests.cs`

- [ ] **Step 1: Add failing tests for stage references and payload versioning.**

Assert that a cooked programmable material contains vertex and fragment artifact identities, variant, source/compiler signature, and required parameter contract. Assert that old payload versions are rejected with a clear version error and that a material with one missing stage fails cooking.

- [ ] **Step 2: Implement version-2 material serialization.**

Keep `PVMT` magic but increment the payload version. Add bounded fields for vertex artifact hash, fragment artifact hash, variant, parameter-contract version, and base color. Do not silently reinterpret version-1 payloads as version 2.

- [ ] **Step 3: Update material cooking to resolve and validate artifacts.**

Resolve both stage artifacts by their content identity, validate stage/profile and source/compiler signatures, deduplicate blobs by hash, and include the referenced artifacts in the returned cooked asset dependencies. Missing or stale artifacts must throw instead of selecting CPU Lambert implicitly.

- [ ] **Step 4: Run focused builder tests.**

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaCompiledShaderMaterial|FullyQualifiedName~PsVitaPlatformAssetBuilder" 2>&1 | Select-Object -First 200
```

Expected: all focused cooking and serializer tests pass.

- [ ] **Step 5: Commit host cooking.**

```powershell
git add builder builder.tests
git commit -m "Cook Vita materials from compiled shader artifacts"
```

### Task 4: Load artifacts and retain runtime GXM patching

**Files:**
- Create: `src/platform/psvita/shaders/PsVitaShaderArtifactReader.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp/.cpp`
- Modify: `src/platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp/.cpp`
- Modify: `CMakeLists.txt`
- Modify: `builder.tests/PsVitaGxmSolidColorProgramSourceAuditTests.cs`
- Modify: `builder.tests/PsVitaGxmRenderPipelineSourceAuditTests.cs`

- [ ] **Step 1: Add failing source audits for the no-runtime-compiler path.**

Assert that the production initialization path references staged artifact loading, `sceGxmProgramCheck`, `sceGxmShaderPatcherRegisterProgram`, parameter lookup, and both patcher creation calls. Assert that it no longer references `sceShaccCgCompileProgram`, `sceShaccCgInitializeCompileOptions`, or `sceKernelLoadStartModule` in the production program wrapper.

- [ ] **Step 2: Implement the runtime artifact reader.**

Read exact bytes from staged content, validate magic/version/length/hash/stage/profile, allocate aligned persistent memory, copy only the complete program payload, and expose `const SceGxmProgram*`. The reader must release its allocation on destruction and must never return a pointer into a temporary stream buffer.

- [ ] **Step 3: Replace compilation in the solid-color wrapper.**

Change initialization to receive the cooked material/artifact identities, load the two stage blobs, call `sceGxmProgramCheck`, construct the existing patcher backing/USSE allocations, register both program headers, resolve parameters, and create vertex/fragment programs. Remove the compiler module state and compile-output cleanup from this wrapper.

- [ ] **Step 4: Preserve explicit development fallback policy.**

Keep any Vita3K/emulator fallback behind an explicit configuration/material choice. A missing production artifact must return a failed program initialization with the artifact identity in diagnostics; it must not silently choose CPU Lambert.

- [ ] **Step 5: Build and run focused tests.**

Run the builder source audits and the smallest native CMake build. Expected: no runtime compiler symbols in the artifact-backed wrapper, successful GXM program registration path compilation, and all source audits pass.

- [ ] **Step 6: Commit runtime loading.**

```powershell
git add src\platform\psvita\shaders src\platform\psvita\rendering CMakeLists.txt builder.tests
git commit -m "Load Vita shader artifacts through GXM"
```

### Task 5: Validate on real hardware and close the pipeline

**Files:**
- Create: `docs/psvita-shader-artifact-workflow.md`: the exact real-Vita export, host-import, cook, and runtime verification sequence.

- [ ] **Step 1: Compile the solid-color vertex and fragment sources on a real Vita.**

Record the Vita firmware/runtime, `sceShaccCgGetVersionString`, target profiles, optimization/precision options, artifact hashes, and output sizes. Export both artifacts through the development transfer path.

- [ ] **Step 2: Import and cook the exported artifacts on the host.**

Verify the host importer reports matching hashes and stage/profile identities. Build the cooked package without invoking a host shader compiler.

- [ ] **Step 3: Boot the artifact-backed build on Vita.**

Verify that the application does not load `libshacccg.suprx`, GXM validates both blobs, patcher creation succeeds, uniforms resolve, and the solid-color mesh renders.

- [ ] **Step 4: Exercise rejection cases.**

Test one byte corruption, wrong stage, wrong profile, stale compiler signature, truncated file, and missing dependency. Expected: deterministic build/runtime diagnostics and no silent CPU-material substitution.

- [ ] **Step 5: Commit workflow documentation and final verification.**

```powershell
git add docs\psvita-shader-artifact-workflow.md
git commit -m "Document Vita shader artifact export workflow"
```
