# PS Vita Device-Backed Shader Compiler Design

## Goal

Add PS Vita as a backend of Helengine's shared shader compilation service. The backend uses a separate, launch-and-exit **Helengine Vita Shader Compiler** VPK to compile shader jobs with the real Vita `libshacccg` compiler and return validated PVSA artifacts to the editor.

## Scope

The first vertical slice compiles the existing `ForwardStandardShader` base-color and diffuse-texture variant. It replaces the hand-exported Forward Lambert artifact pair as the normal compilation path without adding CPU rendering fallbacks.

The compiler VPK is a tool, not the final game executable. It has no project command-line flags, scene browser, or engine runtime dependency beyond the code needed to read a job, invoke `libshacccg`, write artifacts, and exit.

## Architecture

The existing `ShaderCompileService` remains the owner of compile requests, cache keys, source hashing, include resolution, variants, and diagnostics. A new `PsVita` target delegates physical compilation to the Vita compiler VPK.

```text
HLSL shader source and compile request
        |
        v
ShaderCompileService / PsVita backend
        |
        v
inbox manifest and source bundle on Vita storage
        |
        v
Helengine Vita Shader Compiler VPK
        |
        v
libshacccg on real Vita
        |
        v
PVSA artifacts and results manifest on Vita storage
        |
        v
PsVita backend validates, caches, and returns compiled binaries
```

## Compiler VPK Contract

The compiler VPK processes one fixed queue location and exits after the job is complete:

```text
ux0:data/helengine_shader_compiler/inbox/manifest.json
ux0:data/helengine_shader_compiler/inbox/source/
ux0:data/helengine_shader_compiler/outbox/
```

The host writes the inbox completely before the VPK launches. The VPK writes artifacts into a job-specific temporary output directory, validates each output, writes `results.json`, then publishes the completed directory atomically into `outbox`. This prevents the host from accepting incomplete artifacts.

The manifest identifies one job, its stable source/request hash, and one or more stages. Each stage specifies its relative source path, entry point, vertex or fragment profile, preprocessor defines, and compiler options. The result records each stage's success or failure, exact compiler diagnostics, artifact file name, canonical artifact hash, and program byte count.

The VPK has no interactive UI. It launches, compiles the fixed inbox job, writes one result, and exits to LiveArea. A user can transfer jobs and results with VitaShell or FTP; the editor uses the same format programmatically.

## Shared Compiler Integration

Add `PsVita` to `ShaderCompileTarget` and implement an `IShaderBackend` in the PS Vita platform builder. The backend:

1. Lowers the supported HLSL request to the Vita Cg-compatible source accepted by `libshacccg`.
2. Writes one deterministic job manifest and source bundle.
3. Waits for a matching outbox result whose job hash equals the request cache key.
4. Rejects failed, stale, malformed, stage-mismatched, or hash-mismatched results.
5. Returns `ShaderCompileResult` binaries that the existing `ShaderCompileService` caches.

The backend never substitutes old artifacts when a source request fails. A build cannot package a shader stage that was not successfully compiled for the exact request hash.

## Runtime and Material Integration

The platform cooker consumes the returned PVSA artifacts by their canonical hash. Compiled PS Vita materials contain explicit vertex and fragment artifact identities, texture-binding requirements, and the parameter contract version.

The runtime selects the artifact-backed Standard Shader variant based on the cooked material contract. The first textured variant binds position, normal, UV, base color, directional light, ambient light, and the diffuse texture. An unsupported material feature produces an explicit PS Vita material/shader error; it never selects the removed CPU 3D fallback.

## First Variant: Forward Standard Textured

The first implementation supports:

- Position, normal, and UV vertex inputs.
- Diffuse texture sampled in the fragment shader.
- Authored base-color multiplier.
- One directional light plus ambient light.
- Opaque material render state.

Normal mapping, metallic/roughness, emissive, alpha cutout, dynamic shadows, and multiple lights remain outside this vertical slice. Their future variants use the same manifest/backend/artifact contract rather than a separate compilation system.

## Errors and Diagnostics

The compiler VPK always emits a result manifest when it can open the inbox manifest. If the job manifest itself cannot be read, it writes a tool-level failure result where possible and exits.

The editor backend reports these as build diagnostics:

- No completed result for the requested job hash.
- A result whose request hash differs from the requested compile cache key.
- A failed stage with the compiler diagnostic preserved verbatim.
- An invalid PVSA header, stage profile, or artifact hash.
- An incomplete vertex/fragment artifact pair required by a material variant.

## Verification

Automated tests cover target registration, deterministic job serialization, result validation, stale-result rejection, failure propagation, PVSA artifact staging, and material selection for textured versus untextured variants.

Manual validation uses the following sequence:

1. Host creates one Standard Shader job.
2. User transfers or receives the inbox bundle on a real Vita.
3. User launches the compiler VPK; it compiles and exits.
4. Host retrieves the outbox bundle and validates the artifacts.
5. Editor packages the artifact-backed textured cube grid.
6. Vita3K and real Vita render the textured cube grid without white-texture fallback or CPU 3D rendering.
