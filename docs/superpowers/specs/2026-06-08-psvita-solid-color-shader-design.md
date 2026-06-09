# PS Vita Shared Solid-Color Shader Pipeline Design

## Goal

Compile one shared `ForwardSolidColorShader` through the Helengine shader pipeline for DirectX 11, Vulkan, and PS Vita, then use the cooked PS Vita result to render `cube_test` through a real programmable shader path.

## Core Rule

Shared Helengine packages must remain renderer-agnostic.

- `helengine.shader` and `helengine.editor` may define shared shader contracts, orchestration, and package flow
- shared packages must not directly reference `helengine.directx11`, `helengine.vulkan`, PS Vita shader code, or any other renderer/backend package
- each renderer/backend package owns its own shader backend implementation
- backend registration happens explicitly from bootstrap code, not by hard-coded shared-package references
- if the editor needs PS Vita shader support, it must load the PS Vita implementation dynamically instead of taking a compile-time dependency on it

## Scope

This milestone is intentionally narrow:

- one shared authored shader: `ForwardSolidColorShader.hlsl`
- one new shared compile target name: `PsVita`
- one renderer-agnostic backend registration seam in shared Helengine code
- one PS Vita shader backend implementation in `helengine-psvita`
- one PS Vita cooked material payload for the shared solid-color shader
- one runtime programmable mesh path for `cube_test`

## Non-Goals

This milestone does not include:

- general material graphs
- textured Vita materials
- lighting for Vita materials
- automatic backend discovery
- putting renderer-specific shader code into shared Helengine packages
- keeping the old borrowed-`vita2d` shader experiment alive

## Design

### Shared Shader Authoring

`ForwardSolidColorShader.hlsl` stays in the shared built-in shader library and defines the minimal contract needed by all backends:

- position input
- world-view-projection transform
- solid base color output

The shader source is shared, but compiled bytecode remains backend-specific.

### Shared Compile Target Model

`ShaderCompileTarget.PsVita` remains a first-class shared target name so package metadata, target naming, and platform defines can identify PS Vita shader outputs consistently.

This target name is shared metadata only. It does not imply shared packages know how to compile PS Vita shaders by themselves.

### Backend Registration

Shared Helengine code must stop constructing backend implementations directly.

Instead:

1. shared code defines the compile-service contract and a backend-registration seam
2. bootstrap code creates the compile service
3. bootstrap code registers whichever backend packages are present
4. shared editor/package flows consume the populated compile service generically

This keeps `helengine.editor` free of compile-time references to DirectX 11, Vulkan, or PS Vita shader packages.

### Package Ownership

Backend implementations live with their renderer/platform packages:

- DirectX 11 backend code stays in `helengine.directx11`
- Vulkan backend code stays in `helengine.vulkan`
- PS Vita backend code must live in `helengine-psvita`

`helengine-psvita` owns PS Vita shader compilation, PS Vita staging, runtime material translation, and PS Vita runtime rendering. Shared `helengine` code only owns the renderer-agnostic seam and shared shader/package flow.

If editor code needs PS Vita shader support, it must discover and load the PS Vita implementation dynamically through the existing platform-extension path instead of adding a compile-time reference from shared/editor assemblies.

### PS Vita Build Flow

The PS Vita shader backend lives in `helengine-psvita`, compiles shared HLSL source into PS Vita-specific shader binaries through an explicit compiler wrapper, and registers itself into the shared shader pipeline only when the editor loads the PS Vita extension dynamically. Those binaries then enter the normal shared shader package flow under the `psvita` target name.

Later PS Vita staging translates the cooked shader-backed material into a Vita-native runtime material payload that names the compiled shader programs and preserves the authored solid-color data needed by the runtime.

### Runtime Integration

The PS Vita runtime path stays explicit:

- `PsVitaRenderManager3D` selects the shared solid-color material path for `cube_test`
- cooked PS Vita shader-backed material data resolves concrete compiled program identifiers
- the Vita runtime binds real compiled GXM programs
- the old borrowed `vita2d` shader path stays removed

## Verification

The milestone is complete when:

- shared/editor shader code no longer hard-references DirectX 11 or Vulkan backend assemblies
- `ForwardSolidColorShader` compiles for DirectX 11, Vulkan, and PS Vita through the same shared package flow
- PS Vita shader backend registration happens through explicit bootstrap wiring plus dynamic PS Vita extension loading
- PS Vita staging translates the shared shader material into a Vita-native cooked payload
- `cube_test` renders through real compiled PS Vita shader programs

## Follow-Up

Once this pipeline is stable, future backend packages can add more shared shader support by implementing their own backend package or external extension and registering it through the same bootstrap seam.
