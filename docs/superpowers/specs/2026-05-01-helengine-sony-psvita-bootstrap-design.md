# Sony PS Vita Bootstrap Design

## Goal

Create the first working `helengine-psvita` bootstrap that builds in Docker, packages as normal Vita homebrew that Vita3K can run, and boots to an immediate solid cornflower blue screen with no input dependency.

## Constraints

- The repository should follow the same overall shape as the recent console host repos.
- The first milestone should preserve the generated-core seam used across the platform repos.
- The first runtime result should be minimal and deterministic: initialize, render cornflower blue, and keep running.
- The rendering path should prepare for future 3D work rather than depending on 2D-only convenience layers.

## Chosen Approach

Use the real Vita rendering path from the beginning.

This keeps the first milestone visually simple while ensuring the bootstrap exercises the graphics stack that future 3D work will depend on. The initial code should not build a text console layer, menu layer, or 2D-only abstraction. It should stand up the real display/render loop, clear to cornflower blue, and remain stable in the emulator.

## Repository Shape

The initial scaffold should include:

- `Dockerfile`
- `Makefile`
- `CMakeLists.txt`
- `README.md`
- `src/main.cpp`
- `src/platform/psvita/PsVitaBootHost.hpp`
- `src/platform/psvita/PsVitaBootHost.cpp`

This matches the established repo pattern and keeps the platform-specific boundary explicit at the boot host layer.

## Build Design

The build should be Dockerized and should produce normal Vita homebrew output that Vita3K can run.

The build files should:

- target runnable Vita homebrew packaging, not a raw ELF-only workflow
- preserve `HELENGINE_CORE_CPP_ROOT ?=`
- define `HELENGINE_PSVITA_HAS_GENERATED_CORE=0/1`
- compile the minimal source set for the first platform host

The generated-core seam is reserved for future `cs2.cpp` integration, but no generated core code is required for this milestone.

## Runtime Design

`main.cpp` should hand off immediately to a Vita-specific boot host.

`PsVitaBootHost` should:

- initialize the Vita rendering/display path needed for a future 3D-capable renderer
- allocate and configure the first buffers / command structures needed by that path
- clear the rendered output to a solid cornflower blue color
- present the frame continuously so Vita3K shows a stable result

No controller input, text rendering, UI system, audio, or filesystem work is required for this milestone.

## Packaging

The first runnable artifact should be the normal Vita homebrew packaging that Vita3K accepts, not just a loose ELF. The exact artifact naming and layout should be derived from the active Vita SDK/toolchain used in the Docker image.

## Platform Boundary

This repository is Vita-specific and should not be diluted by cross-platform abstractions at bootstrap time. Shared engine code can come later above the platform host boundary, but the initial boot path should remain directly Vita-oriented.

## Verification

Success means:

1. the Docker image builds successfully
2. the Vita project builds successfully inside that container
3. the build emits runnable Vita homebrew output for Vita3K
4. Vita3K boots the artifact and shows a stable solid cornflower blue screen immediately

## Non-Goals

- text console output
- menu/UI setup
- input handling
- 2D-only rendering helpers
- gameplay or engine runtime beyond the generated-core seam
