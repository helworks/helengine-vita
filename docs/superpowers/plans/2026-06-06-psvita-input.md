# PS Vita Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement shared-runtime PS Vita buttons, both analog sticks, and front touch input with zero per-frame backend-owned heap allocation.

**Architecture:** Extend `PsVitaInputBackend` from an empty-frame stub into a persistent-storage backend that captures Vita controller state into one shared gamepad slot and translates front touch into the engine's existing mouse/pointer path. Keep the implementation aligned with the current `InputFrameState` contract so `InputSystem` continues to derive pointer behavior from `MouseState` without any core-engine changes.

**Tech Stack:** C++, PS Vita SDK control/touch APIs, existing generated-core input types, xUnit source-audit tests, real editor CLI builds, Vita3K verification.

---

## File Map

- Modify: `builder.tests/PsVitaBootHostSourceAuditTests.cs`
  - Extend source-level verification only if boot-host/input wiring needs explicit audit coverage.
- Create or Modify: `builder.tests/PsVitaInputBackendSourceAuditTests.cs`
  - Primary source-audit coverage for the Vita input backend behavior and zero-garbage requirements.
- Modify: `src/platform/psvita/PsVitaInputBackend.hpp`
  - Add persistent input-storage members and helper declarations.
- Modify: `src/platform/psvita/PsVitaInputBackend.cpp`
  - Implement controller capture, analog conversion, front-touch translation, and zero-garbage frame reuse.
- Modify: `CMakeLists.txt`
  - Only if additional Vita input SDK linkage or source registration is required.

## Task 1: Lock the input contract with failing source audits

**Files:**
- Create or Modify: `builder.tests/PsVitaInputBackendSourceAuditTests.cs`
- Test: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Write the failing source-audit test for native controller and touch capture**

```csharp
using System;
using System.IO;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita input backend source so runtime input support cannot regress back to the empty bootstrap stub.
/// </summary>
public sealed class PsVitaInputBackendSourceAuditTests {
    /// <summary>
    /// Verifies the backend no longer returns an empty default frame and instead references native control and touch polling.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_usesNativeControllerAndFrontTouchPolling() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("return ::InputFrameState();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceCtrl", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceTouch", sourceCode, StringComparison.Ordinal);
        Assert.Contains("CaptureFrame()", sourceCode, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 2: Write the failing source-audit test for persistent gamepad storage**

```csharp
    /// <summary>
    /// Verifies the backend owns persistent input storage instead of allocating new frame-owned gamepad arrays every frame.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_reusesPersistentGamepadStorageWithoutPerFrameAllocation() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("Array<InputGamepadState>* PersistentGamepads;", headerSource, StringComparison.Ordinal);
        Assert.Contains("::InputFrameState CachedFrame;", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("new Array<InputGamepadState>(1)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PersistentGamepads", sourceCode, StringComparison.Ordinal);
    }
```

- [ ] **Step 3: Write the failing source-audit test for Vita button mapping and touch-to-mouse translation**

```csharp
    /// <summary>
    /// Verifies the backend maps Vita pad buttons into the shared gamepad contract and translates front touch into the mouse-backed pointer path.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_mapsSharedButtonsAndTranslatesTouchIntoMouseState() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("InputGamepadButton::DPadUp", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::South", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::East", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::Start", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_LeftStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_RightStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("MouseState(", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ButtonState::Pressed", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ButtonState::Released", sourceCode, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 4: Run the tests to verify they fail for the right reason**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-input-red-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-input-red-obj\
```

Expected:

- FAIL in `PsVitaInputBackendSourceAuditTests`
- failure should show the backend is still returning `::InputFrameState()` and lacks the required native polling/storage strings

- [ ] **Step 5: Commit the red tests**

```bash
git add builder.tests/PsVitaInputBackendSourceAuditTests.cs
git commit -m "Add PS Vita input backend source audits"
```

## Task 2: Implement persistent Vita controller capture

**Files:**
- Modify: `src/platform/psvita/PsVitaInputBackend.hpp`
- Modify: `src/platform/psvita/PsVitaInputBackend.cpp`
- Test: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Add persistent storage members and helper declarations**

Update `src/platform/psvita/PsVitaInputBackend.hpp` to include persistent frame storage and helper declarations:

```cpp
#pragma once

#include "IInputBackend.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "MouseState.hpp"
#include "runtime/array.hpp"

namespace helengine::psvita {
    class PsVitaInputBackend final : public ::IInputBackend {
    public:
        PsVitaInputBackend();
        ~PsVitaInputBackend() override;

        bool get_ReceiveInputInBackground();
        void set_ReceiveInputInBackground(bool value);
        ::InputFrameState CaptureFrame() override;

    private:
        static short ConvertAnalogAxis(std::uint8_t value);
        static int ConvertFrontTouchX(int rawX);
        static int ConvertFrontTouchY(int rawY);
        void UpdateGamepadState();
        void UpdateFrontTouchMouseState();

        bool ReceiveInputInBackground;
        Array<InputGamepadState>* PersistentGamepads;
        ::InputFrameState CachedFrame;
        ::MouseState CachedMouseState;
        int PreviousTouchX;
        int PreviousTouchY;
        bool WasTouchActiveLastFrame;
    };
}

#endif
```

- [ ] **Step 2: Run the red tests again after the header change**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-input-header-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-input-header-obj\
```

Expected:

- still FAIL
- failures should move from missing header members toward missing implementation details

- [ ] **Step 3: Implement controller initialization and persistent storage construction**

Update the start of `src/platform/psvita/PsVitaInputBackend.cpp`:

```cpp
#include "platform/psvita/PsVitaInputBackend.hpp"

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <algorithm>
#include <cstdint>

#include "ButtonState.hpp"
#include "InputGamepadButton.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    PsVitaInputBackend::PsVitaInputBackend()
        : ReceiveInputInBackground(false)
        , PersistentGamepads(new Array<InputGamepadState>(1))
        , CachedFrame()
        , CachedMouseState(0, 0, 0, ButtonState::Released, ButtonState::Released, ButtonState::Released, ButtonState::Released, ButtonState::Released)
        , PreviousTouchX(0)
        , PreviousTouchY(0)
        , WasTouchActiveLastFrame(false) {
        sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
        sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

        CachedFrame.set_Gamepads(PersistentGamepads);
        CachedFrame.set_GamepadCount(1);
        CachedFrame.set_Mouse(CachedMouseState);
    }

    PsVitaInputBackend::~PsVitaInputBackend() {
        delete PersistentGamepads;
        PersistentGamepads = nullptr;
    }
```

- [ ] **Step 4: Implement shared gamepad button and analog mapping**

Continue `src/platform/psvita/PsVitaInputBackend.cpp` with:

```cpp
    short PsVitaInputBackend::ConvertAnalogAxis(std::uint8_t value) {
        int centered = static_cast<int>(value) - 127;
        int scaled = centered * 256;
        scaled = std::clamp(scaled, -32768, 32767);
        return static_cast<short>(scaled);
    }

    void PsVitaInputBackend::UpdateGamepadState() {
        SceCtrlData padData {};
        sceCtrlPeekBufferPositive(0, &padData, 1);

        InputGamepadState gamepadState;
        gamepadState.set_Connected(true);

        gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (padData.buttons & SCE_CTRL_UP) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (padData.buttons & SCE_CTRL_DOWN) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (padData.buttons & SCE_CTRL_LEFT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (padData.buttons & SCE_CTRL_RIGHT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::South, (padData.buttons & SCE_CTRL_CROSS) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::East, (padData.buttons & SCE_CTRL_CIRCLE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::West, (padData.buttons & SCE_CTRL_SQUARE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::North, (padData.buttons & SCE_CTRL_TRIANGLE) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::LeftShoulder, (padData.buttons & SCE_CTRL_LTRIGGER) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::RightShoulder, (padData.buttons & SCE_CTRL_RTRIGGER) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Start, (padData.buttons & SCE_CTRL_START) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Select, (padData.buttons & SCE_CTRL_SELECT) != 0);
        gamepadState.set_LeftStickX(ConvertAnalogAxis(padData.lx));
        gamepadState.set_LeftStickY(ConvertAnalogAxis(padData.ly));
        gamepadState.set_RightStickX(ConvertAnalogAxis(padData.rx));
        gamepadState.set_RightStickY(ConvertAnalogAxis(padData.ry));
        gamepadState.set_LeftTrigger(0);
        gamepadState.set_RightTrigger(0);

        (*PersistentGamepads)[0] = gamepadState;
    }
```

- [ ] **Step 5: Run the tests to verify progress is still red only on touch behavior**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-input-gamepad-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-input-gamepad-obj\
```

Expected:

- still FAIL if touch/mouse translation or final `CaptureFrame()` behavior is not yet present
- gamepad/storage string expectations should now pass

- [ ] **Step 6: Commit the gamepad implementation**

```bash
git add src/platform/psvita/PsVitaInputBackend.hpp src/platform/psvita/PsVitaInputBackend.cpp
git commit -m "Implement PS Vita gamepad input capture"
```

## Task 3: Implement front touch translation through the shared mouse/pointer path

**Files:**
- Modify: `src/platform/psvita/PsVitaInputBackend.cpp`
- Test: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Implement front-touch coordinate conversion helpers**

Add helper implementations in `src/platform/psvita/PsVitaInputBackend.cpp`:

```cpp
    int PsVitaInputBackend::ConvertFrontTouchX(int rawX) {
        constexpr int FrontTouchWidth = 1920;
        constexpr int RuntimeWidth = 960;
        int scaled = (rawX * RuntimeWidth) / FrontTouchWidth;
        return std::clamp(scaled, 0, RuntimeWidth - 1);
    }

    int PsVitaInputBackend::ConvertFrontTouchY(int rawY) {
        constexpr int FrontTouchHeight = 1088;
        constexpr int RuntimeHeight = 544;
        int scaled = (rawY * RuntimeHeight) / FrontTouchHeight;
        return std::clamp(scaled, 0, RuntimeHeight - 1);
    }
```

- [ ] **Step 2: Implement front-touch to mouse-state translation**

Add the touch update method:

```cpp
    void PsVitaInputBackend::UpdateFrontTouchMouseState() {
        SceTouchData touchData {};
        int touchReadCount = sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touchData, 1);
        bool isTouchActive = touchReadCount > 0 && touchData.reportNum > 0;

        int pointerX = PreviousTouchX;
        int pointerY = PreviousTouchY;
        ButtonState leftButtonState = ButtonState::Released;

        if (isTouchActive) {
            pointerX = ConvertFrontTouchX(touchData.report[0].x);
            pointerY = ConvertFrontTouchY(touchData.report[0].y);
            leftButtonState = ButtonState::Pressed;
        }

        CachedMouseState = MouseState(
            pointerX,
            pointerY,
            0,
            leftButtonState,
            ButtonState::Released,
            ButtonState::Released,
            ButtonState::Released,
            ButtonState::Released);

        PreviousTouchX = pointerX;
        PreviousTouchY = pointerY;
        WasTouchActiveLastFrame = isTouchActive;
        CachedFrame.set_Mouse(CachedMouseState);
    }
```

- [ ] **Step 3: Implement the final frame capture path without per-frame allocation**

Finish `CaptureFrame()`:

```cpp
    ::InputFrameState PsVitaInputBackend::CaptureFrame() {
        UpdateGamepadState();
        UpdateFrontTouchMouseState();
        CachedFrame.set_GamepadCount(1);
        CachedFrame.set_Gamepads(PersistentGamepads);
        return CachedFrame;
    }
}

#endif
```

- [ ] **Step 4: Run the tests to verify they pass**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\psvita-input-green-bin\ -p:BaseIntermediateOutputPath=C:\tmp\psvita-input-green-obj\
```

Expected:

- PASS for the new `PsVitaInputBackendSourceAuditTests`
- PASS overall for the builder test project

- [ ] **Step 5: Commit the touch/pointer implementation**

```bash
git add src/platform/psvita/PsVitaInputBackend.cpp
git commit -m "Add PS Vita front touch pointer input"
```

## Task 4: Verify the real editor build and Vita3K runtime behavior

**Files:**
- Modify: none unless verification exposes a real bug
- Test: real editor build output under `C:\tmp`

- [ ] **Step 1: Rebuild the PS Vita package through the real editor CLI path**

Run:

```powershell
dotnet C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\bin\Debug\net9.0-windows\helengine.editor.app.dll --project C:\dev\helprojs\city\project.heproj --build psvita --output C:\tmp\city-psvita-input
```

Expected:

- `Build completed for platform 'psvita': C:\tmp\city-psvita-input`

- [ ] **Step 2: Delete the previous Vita3K install so the new VPK is the one under test**

Run:

```powershell
& 'C:\dev\helworks\emus\vita-3k\Vita3K.exe' -d HLEN00001
```

Expected:

- Vita3K reports the title deletion for `HLEN00001`

- [ ] **Step 3: Install and launch the rebuilt VPK in Vita3K**

Run:

```powershell
& 'C:\dev\helworks\emus\vita-3k\Vita3K.exe' 'C:\tmp\city-psvita-input\helengine_psvita.vpk'
```

Expected:

- Vita3K installs the package and launches the title
- menu appears

- [ ] **Step 4: Verify controller and touch behavior manually**

Manual checks:

- D-pad changes menu selection
- analog stick changes menu selection where bindings support it
- `Cross` activates the focused menu item
- `Circle` performs back where the menu scene exposes it
- front touch can hover and press menu interactables through the existing pointer interaction path

- [ ] **Step 5: Inspect the boot log if interaction fails**

Run:

```powershell
Get-Content 'C:\Users\Helena\AppData\Roaming\Vita3K\Vita3K\ux0\data\helengine_psvita_boot.log' -Tail 200
```

Expected:

- use the log only for diagnosing regressions
- no new repeated input-path failure spam

- [ ] **Step 6: Commit verification-complete input support**

```bash
git add builder.tests/PsVitaInputBackendSourceAuditTests.cs src/platform/psvita/PsVitaInputBackend.hpp src/platform/psvita/PsVitaInputBackend.cpp
git commit -m "Implement PS Vita runtime input support"
```

## Self-Review

Spec coverage:

- buttons and both analog sticks are implemented in Task 2
- front touch through shared pointer behavior is implemented in Task 3
- zero per-frame backend allocation is covered by Task 1 audits and Task 2 persistent storage
- real editor CLI and Vita3K verification are covered in Task 4

Placeholder scan:

- no `TODO`, `TBD`, or deferred implementation markers remain
- all tasks include exact files and commands

Type consistency:

- plan consistently uses `PsVitaInputBackend`, `InputFrameState`, `Array<InputGamepadState>`, and `MouseState`
- button mapping names match existing generated/native C++ naming used elsewhere in the repo
