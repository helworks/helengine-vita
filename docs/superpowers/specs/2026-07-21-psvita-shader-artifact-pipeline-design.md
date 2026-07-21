# PS Vita Shader Artifact Pipeline Design

## Goal

Compile Vita shader source on a real PS Vita, export the complete `SceShaccCgCompileOutput::programData` payload, import it into the host asset pipeline, and load it at runtime without loading `libshacccg.suprx` or invoking `sceShaccCgCompileProgram`.

## Context

The current native path in `src/platform/psvita/rendering/PsVitaGxmSolidColorProgram.cpp` compiles an embedded shader at runtime, copies the compiler output into aligned host memory, registers the resulting `SceGxmProgram` headers with a GXM shader patcher, and creates patched vertex and fragment programs. The builder already has `PsVitaCompiledShader`, but the current `PVMT` material payload stores shader names and material metadata rather than compiled stage bytes.

The new pipeline must preserve the complete compiler-produced program blob. It must not extract or rewrite microcode, because GXM program headers contain the metadata needed for parameter reflection and patcher registration.

## Decisions

### Vita is the compiler authority

The compiler helper runs on a real Vita using the same `libshacccg.suprx` and runtime environment used by the engine. It accepts a source request containing shader source, source name, target profile, entry point, optimization level, warning level, and the compiler identity. It writes a self-describing artifact containing the complete program bytes and diagnostic/result metadata.

The helper is a development/build tool, not a general runtime engine subsystem. It may use an existing Vita file-transfer/debug channel or a deliberately scoped filesystem export path, but the transfer mechanism must not become part of the renderer ABI.

### Artifact format

Each stage artifact contains:

- Four-byte magic and format version.
- Stage/profile identifier (`VP` or `FP`).
- Source hash and compiler/toolchain identity.
- Entry point and compiler options.
- Program byte count.
- Complete `programData` bytes.
- A deterministic artifact hash.

The material asset references the stage artifacts and records the shader variant, parameter contract, and material defaults. Stage blobs are deduplicated by artifact hash.

The format is little-endian and uses explicit bounded lengths. It rejects truncated data, unsupported versions, mismatched stage/profile metadata, and trailing bytes when strict validation is enabled.

### Runtime loading and patching

The Vita runtime loads stage blobs into persistent, suitably aligned memory, validates them with `sceGxmProgramCheck`, registers them with `sceGxmShaderPatcherRegisterProgram`, resolves parameters by name, and creates patched vertex/fragment programs using the current render-state requirements.

The runtime shader patcher remains on Vita. A pre-patched program is not the primary artifact because vertex attributes, output format, multisampling, blending, and related state are runtime choices. The compiler module is no longer required for shipped artifacts.

### Compatibility and invalidation

The artifact cache key includes source hash, target profile, entry point, optimization and precision flags, `libshacccg` version string, Vita SDK identity, and artifact schema version. A cache hit is valid only when every key field matches. The runtime rejects artifacts with an incompatible schema or unsupported program type instead of falling back silently.

## Components

### Vita compiler/export helper

Owns source resolution, `sceShaccCgCompileProgram`, diagnostic serialization, artifact writing, and a small export command protocol. It must support compiling one stage at a time and return nonzero failure status for compiler errors or artifact-write failures.

### Host artifact importer

Reads exported stage artifacts, validates their headers and hashes, records compiler provenance, and adds the bytes to the PS Vita cooked asset set. It does not reinterpret the program blob.

### Builder asset model

Extends the existing `PsVitaCompiledShader` and material cooking path so cooked shader/material assets contain stage artifact references and immutable program payloads. The existing material field names remain stable where possible; new fields are versioned explicitly.

### Vita runtime loader

Replaces the runtime compilation portion of `PsVitaGxmSolidColorProgram` with a loader that obtains vertex and fragment blobs from staged content, validates them, allocates persistent memory, and passes their `SceGxmProgram` headers to the existing patcher path.

## Data flow

```text
Shader source
    -> Vita compiler helper
    -> VP/FP artifact files
    -> host importer and hash validation
    -> cooked Vita asset package
    -> runtime blob loader
    -> sceGxmProgramCheck
    -> shader patcher registration
    -> runtime vertex/fragment program creation
```

## Failure behavior

- Compiler diagnostics are preserved and surfaced to the build caller.
- Missing artifacts, malformed headers, hash mismatches, profile mismatches, and GXM validation failures are hard failures for the programmable material.
- The renderer may retain an explicitly selected emulator/development fallback while bring-up is underway, but it must not silently convert a requested production shader into CPU Lambert rendering.
- Every allocation and GXM registration has a matching cleanup path.

## Validation

The pipeline is complete when:

1. A real Vita compiles the solid-color test shader and exports both stage artifacts.
2. The host importer validates and cooks those exact bytes.
3. A runtime build loads the artifacts without loading `libshacccg.suprx`.
4. GXM accepts the blobs, resolves the expected parameters, creates patched programs, and renders the test mesh.
5. Corrupt, stale, wrong-profile, and wrong-stage artifacts fail deterministically.
6. The artifact format and source-audit tests cover the builder/runtime contract.

## Out of scope

- Reverse engineering or rewriting Vita microcode.
- Pre-baking every possible GXM patched-program state.
- Replacing the general asset transport layer.
- Implementing the full Lambert material in this pipeline; that is covered by the GPU forward-Lambert design.

