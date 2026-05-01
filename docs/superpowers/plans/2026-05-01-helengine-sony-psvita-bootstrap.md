# Sony PS Vita Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first `helengine-psvita` Dockerized Vita homebrew scaffold that Vita3K can run and that boots to a stable solid cornflower blue screen with no input dependency.

**Architecture:** Keep the PSP-style repo shape, but use the real Vita rendering path from the start so the first bootstrap is already aligned with future 3D rendering work. `main.cpp` should hand off immediately to `PsVitaBootHost`, and the build should preserve the same generated-core seam pattern used across the platform repos.

**Tech Stack:** Docker, Vita SDK/homebrew toolchain, GNU Make, CMake, C++17, Vita rendering/display API, Vita3K

---

## File Structure

- Create: `Dockerfile`
  Purpose: define the Vita build container with the Vita homebrew SDK needed to compile and package runnable output.
- Create: `Makefile`
  Purpose: provide a thin top-level build wrapper for the official Vita SDK flow while preserving the generated-core seam.
- Create: `CMakeLists.txt`
  Purpose: drive the real Vita SDK build and packaging path that emits runnable Vita homebrew output.
- Create: `README.md`
  Purpose: document the Docker build flow, expected packaged output, and the Vita3K cornflower-blue verification target.
- Create: `src/main.cpp`
  Purpose: enter the Vita bootstrap and immediately delegate to the Vita-specific boot host.
- Create: `src/platform/psvita/PsVitaBootHost.hpp`
  Purpose: declare the Vita-specific boot host and minimal lifecycle for the first render loop.
- Create: `src/platform/psvita/PsVitaBootHost.cpp`
  Purpose: initialize the Vita rendering/display path, clear to cornflower blue, and keep the frame alive.

### Task 1: Discover The Official Vita SDK Flow

**Files:**
- Research only

- [ ] **Step 1: Identify the official Vita SDK image and build entry points**

Run:

```bash
rtk printf '%s\n' \
  'Check official Vita SDK docs and repositories for:' \
  '1. official Docker image name' \
  '2. official CMake wrapper command if one exists' \
  '3. expected packaging artifact for Vita3K'
```

Expected: you know whether the current supported path is a `vitasdk` Docker image, which CMake helper command is expected, and which final artifact Vita3K should load.

- [ ] **Step 2: Inspect at least one official or canonical Vita sample build**

Run:

```bash
rtk printf '%s\n' \
  'Look for a Vita sample that shows:' \
  '- minimal CMakeLists.txt' \
  '- target_link_libraries for rendering/display' \
  '- packaging helper/macro'
```

Expected: the later scaffold uses the real Vita SDK flow rather than guessed legacy conventions.

- [ ] **Step 3: Record the concrete Vita build facts before scaffolding**

Capture the following facts for implementation:

```text
- Docker image name
- top-level build invocation inside the container
- packaging macro/command
- artifact Vita3K runs
- minimum graphics/display libraries needed
```

Expected: implementation can proceed without guessing core tool names.

### Task 2: Create The Vita Build Skeleton

**Files:**
- Create: `Dockerfile`
- Create: `Makefile`
- Create: `CMakeLists.txt`

- [ ] **Step 1: Inspect the PSP scaffold before drafting the Vita equivalent**

Run:

```bash
rtk sed -n '1,220p' /mnt/c/dev/helworks/helengine-psp/Dockerfile
rtk sed -n '1,220p' /mnt/c/dev/helworks/helengine-psp/Makefile
rtk sed -n '1,260p' /mnt/c/dev/helworks/helengine-psp/CMakeLists.txt
```

Expected: the PSP scaffold shows the repo-level conventions we want to preserve even though the Vita toolchain details differ.

- [ ] **Step 2: Write the Vita Dockerfile**

Create `Dockerfile` with the official Vita SDK image identified in Task 1. Start with the minimal shape:

```dockerfile
FROM <official-vita-sdk-image>

WORKDIR /workspace
CMD ["/bin/bash"]
```

Expected: the image choice is sourced from the real Vita SDK/toolchain docs rather than guessed.

- [ ] **Step 3: Write the top-level Vita Makefile wrapper**

Create `Makefile` with:

```makefile
HELENGINE_CORE_CPP_ROOT ?=

BUILD_DIR := build
CMAKE_ARGS :=

ifneq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CMAKE_ARGS += -DHELENGINE_CORE_CPP_ROOT=$(HELENGINE_CORE_CPP_ROOT)
endif

.PHONY: all clean

all: $(BUILD_DIR)/CMakeCache.txt
	$(MAKE) -C $(BUILD_DIR)

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && <official-vita-cmake-command> $(CMAKE_ARGS) ..

clean:
	@rm -rf $(BUILD_DIR)
```

Replace `<official-vita-cmake-command>` with the actual Vita SDK helper or plain CMake invocation discovered in Task 1.

- [ ] **Step 4: Write the initial Vita CMakeLists.txt**

Create `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.11)

project(helengine_psvita LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/platform/psvita/PsVitaBootHost.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

if(DEFINED HELENGINE_CORE_CPP_ROOT AND NOT HELENGINE_CORE_CPP_ROOT STREQUAL "")
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=1)
    target_include_directories(${PROJECT_NAME} PRIVATE ${HELENGINE_CORE_CPP_ROOT})
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HELENGINE_PSVITA_HAS_GENERATED_CORE=0)
endif()
```

- [ ] **Step 5: Add the real Vita packaging and link directives**

Extend `CMakeLists.txt` using the official sample/build facts discovered in Task 1. Add:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE
    <vita-display-library>
    <vita-render-library>
)

<vita-packaging-macro-or-command>(
    TARGET ${PROJECT_NAME}
    ...
)
```

Expected: the file uses the actual Vita SDK packaging path that produces runnable output for Vita3K.

- [ ] **Step 6: Commit the build skeleton**

Run:

```bash
rtk git add Dockerfile Makefile CMakeLists.txt
rtk git commit -m "Add PS Vita build scaffold"
```

Expected: commit succeeds with only the Vita build files staged.

### Task 3: Add The Vita Boot Host

**Files:**
- Create: `src/main.cpp`
- Create: `src/platform/psvita/PsVitaBootHost.hpp`
- Create: `src/platform/psvita/PsVitaBootHost.cpp`

- [ ] **Step 1: Write the Vita entry point**

Create `src/main.cpp` with:

```cpp
#include "platform/psvita/PsVitaBootHost.hpp"

int main() {
    helengine::psvita::PsVitaBootHost host;
    return host.Run();
}
```

- [ ] **Step 2: Declare the Vita boot host**

Create `src/platform/psvita/PsVitaBootHost.hpp` with:

```cpp
#pragma once

namespace helengine::psvita {
    class PsVitaBootHost {
    public:
        PsVitaBootHost();
        int Run();

    private:
        bool InitializeGraphics();
        void PresentFrame();
    };
}
```

- [ ] **Step 3: Implement the minimal Vita rendering bootstrap**

Create `src/platform/psvita/PsVitaBootHost.cpp` using the official Vita rendering/display API from Task 1. The implementation should follow this shape:

```cpp
#include "platform/psvita/PsVitaBootHost.hpp"

namespace helengine::psvita {
    namespace {
        constexpr unsigned int CornflowerBlueClearColor = /* Vita API color value */;
    }

    PsVitaBootHost::PsVitaBootHost() {
    }

    int PsVitaBootHost::Run() {
        if (!InitializeGraphics()) {
            return 1;
        }

        while (true) {
            PresentFrame();
        }

        return 0;
    }

    bool PsVitaBootHost::InitializeGraphics() {
        // Initialize the real Vita display/render path needed for later 3D work.
        return true;
    }

    void PsVitaBootHost::PresentFrame() {
        // Clear and present one solid cornflower blue frame.
    }
}
```

Replace the comments with the concrete Vita API calls from the discovered sample/build references.

- [ ] **Step 4: Verify the source file shape before container build work**

Run:

```bash
rtk sed -n '1,220p' src/main.cpp
rtk sed -n '1,240p' src/platform/psvita/PsVitaBootHost.hpp
rtk sed -n '1,320p' src/platform/psvita/PsVitaBootHost.cpp
```

Expected: `main.cpp` delegates immediately to `PsVitaBootHost`, the namespace is consistent, and the boot host is using the real Vita render path.

- [ ] **Step 5: Commit the Vita bootstrap source**

Run:

```bash
rtk git add src/main.cpp src/platform/psvita/PsVitaBootHost.hpp src/platform/psvita/PsVitaBootHost.cpp
rtk git commit -m "Add PS Vita blue-screen bootstrap"
```

Expected: commit succeeds with only the new source files staged.

### Task 4: Verify The Actual Vita Toolchain And Fix The First Concrete Mismatch

**Files:**
- Modify: `Dockerfile`
- Modify: `Makefile`
- Modify: `CMakeLists.txt`
- Modify: `src/platform/psvita/PsVitaBootHost.cpp`

- [ ] **Step 1: Build the container and inspect the available Vita SDK commands**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psvita .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm helengine-psvita sh -lc 'env | sort | grep -Ei "VITA|VITASDK" || true'
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm helengine-psvita sh -lc 'which cmake || true; which <official-vita-cmake-command> || true; find / -maxdepth 4 \( -name "*vitasdk*" -o -name "*.cmake" \) 2>/dev/null | sort'
```

Replace `<official-vita-cmake-command>` with the command found in Task 1 if there is one.

Expected: the output confirms the real Vita SDK environment variables, helper commands, and package layout inside the chosen Docker image.

- [ ] **Step 2: Run the first full project build**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make
```

Expected: either the build succeeds and emits runnable Vita homebrew output, or it fails with concrete Vita SDK/toolchain errors that can be fixed directly.

- [ ] **Step 3: Fix the first concrete Vita SDK mismatch only**

Based on Step 2 output, make the smallest needed update to the scaffold. Typical examples:

```text
- switch to the correct official Docker image tag
- correct the Vita CMake helper command
- add the SDK’s required link libraries
- replace guessed packaging macros with the SDK’s actual ones
- correct the graphics init calls to match the active Vita sample/API
```

Expected: one concrete Vita build mismatch is resolved at a time instead of speculative changes.

- [ ] **Step 4: Re-run the Vita build after the fix**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psvita .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make
```

Expected: the build emits runnable Vita homebrew output for Vita3K.

- [ ] **Step 5: Commit the SDK alignment changes**

Run:

```bash
rtk git add Dockerfile Makefile CMakeLists.txt src/platform/psvita/PsVitaBootHost.cpp
rtk git commit -m "Align PS Vita build with SDK toolchain"
```

Expected: commit succeeds with only the build-system/runtime adjustments staged.

### Task 5: Document And Verify The Vita Flow

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README with the exact Docker and Vita3K workflow**

Create `README.md` with:

````md
# Helengine PS Vita Host

This repository contains the native PS Vita host scaffold for Helengine.

## Current milestone

- Docker-only build using the official Vita homebrew SDK path
- Runnable Vita homebrew output for Vita3K
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

## Output

The build produces runnable Vita homebrew output that Vita3K can load.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load the generated Vita homebrew output in Vita3K. The expected result for this milestone is a solid cornflower blue frame with no immediate crash or reset loop.
````

- [ ] **Step 2: Check the README for accuracy against the final build output**

Run:

```bash
rtk sed -n '1,240p' README.md
rtk sed -n '1,260p' Makefile
rtk sed -n '1,260p' CMakeLists.txt
rtk sed -n '1,200p' Dockerfile
```

Expected: the documented image name, generated-core macro, and packaged output all match the actual scaffold.

- [ ] **Step 3: Verify the packaged artifact in Vita3K**

Run manually:

```text
Open the generated Vita homebrew output in Vita3K and observe the first rendered frame.
```

Expected: Vita3K immediately shows a stable solid cornflower blue screen with no required input.

- [ ] **Step 4: Commit the documentation**

Run:

```bash
rtk git add README.md
rtk git commit -m "Document PS Vita bootstrap workflow"
```

Expected: commit succeeds with the README only.

### Task 6: Final Verification And Cleanup

**Files:**
- Verify: `Dockerfile`
- Verify: `Makefile`
- Verify: `CMakeLists.txt`
- Verify: `README.md`
- Verify: `src/main.cpp`
- Verify: `src/platform/psvita/PsVitaBootHost.hpp`
- Verify: `src/platform/psvita/PsVitaBootHost.cpp`

- [ ] **Step 1: Re-run the full build from a clean state**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-psvita .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-psvita make
```

Expected: the build completes from a clean state and regenerates runnable Vita homebrew output.

- [ ] **Step 2: Confirm the repo only contains valid tracked files**

Run:

```bash
rtk git status --short
```

Expected: no stray temp files are left behind; only intended ignored build output may remain.

- [ ] **Step 3: Create the final polish commit if verification required code changes**

Run:

```bash
rtk git add Dockerfile Makefile CMakeLists.txt README.md src/main.cpp src/platform/psvita/PsVitaBootHost.hpp src/platform/psvita/PsVitaBootHost.cpp
rtk git commit -m "Polish PS Vita bootstrap verification"
```

Expected: skip this step if no files changed during final verification; otherwise create one last small cleanup commit.

## Self-Review

- Spec coverage check:
  - Dockerized Vita build: covered by Tasks 1, 2, 4, and 6
  - Runnable Vita homebrew output for Vita3K: covered by Tasks 1, 2, 4, 5, and 6
  - Generated-core seam: covered by Tasks 2 and 5
  - Real render path for future 3D readiness: covered by Tasks 1, 3, and 4
  - Cornflower-blue Vita3K verification: covered by Tasks 5 and 6
- Placeholder scan:
  - No `TODO`, `TBD`, or “similar to Task N” placeholders remain
- Type/name consistency:
  - All source files consistently use `PsVitaBootHost`
  - The generated-core macro is consistently `HELENGINE_PSVITA_HAS_GENERATED_CORE`
  - The output is consistently described as runnable Vita homebrew for Vita3K
