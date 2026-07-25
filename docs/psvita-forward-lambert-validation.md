# PS Vita Forward-Lambert Validation

## Managed validation

Run the focused implementation checks:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj --filter "FullyQualifiedName~PsVitaShaderArtifact|FullyQualifiedName~PsVitaForwardLambert|FullyQualifiedName~PsVitaGxmForwardLambert|FullyQualifiedName~PsVitaGxmSolidColorProgram" --no-restore
```

These checks cover artifact serialization, source/compiler boundaries, GXM program ownership, vertex layout, uniform binding, and fallback policy.

## Vita3K smoke test

Use Vita3K for fast package/material wiring checks. Confirm that the VPK contains `cooked/shaders/*.pvsa`, the material contains matching artifact hashes, and missing artifacts produce an explicit initialization failure.

Vita3K timing is not used as evidence of SGX543MP4+ performance.

## Real Vita test

Run one opaque test mesh with opposing normals and one directional light. Verify that the opposing faces receive different intensities, reversing the light direction reverses the result, and the CPU Lambert-evaluation counter remains zero for the GPU material.

Record:

- Vita firmware/runtime;
- `sceShaccCg` compiler version used to produce the artifacts;
- vertex and fragment artifact hashes;
- shader initialization count;
- draw count and scene-break count;
- CPU submission time;
- GPU frame time and parameter-buffer usage.

## Rejection tests

Test a corrupted byte, wrong profile, missing artifact, truncated artifact, and stale compiler identity. Each case must fail deterministically and identify the affected artifact or material.
