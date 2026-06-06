# Helengine PS Vita Host

This repository contains the native PS Vita host scaffold for Helengine.

## Current milestone

- Docker-only build using the VitaSDK image
- Standard Vita homebrew packaging ending in `.vpk`
- First boot check with a solid cornflower blue screen in Vita3K

## Build

```bash
docker build -t helengine-psvita .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psvita .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make
```

The build emits `build/helengine_psvita.vpk`.

## Editor build

Use the shared Helengine platform build script when building the City project for PS Vita through the editor pipeline:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 `
    -Project C:\dev\helprojs\city\project.heproj `
    -Platform psvita `
    -Output C:\dev\helprojs\city\vita-build
```

Use the standardized Vita3K launcher script to run that exact build artifact in the emulator:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\launch-vita3k.ps1 `
    -VpkPath C:\dev\helprojs\city\vita-build\helengine_psvita.vpk
```

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/helengine_psvita.vpk` in Vita3K. The expected result for this milestone is a stable solid cornflower blue frame with no immediate crash or return to the launcher.
