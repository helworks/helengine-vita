#include "platform/psvita/PsVitaInputBackend.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <algorithm>
#include <cstdint>

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include "ButtonState.hpp"
#include "InputGamepadButton.hpp"

namespace helengine::psvita {
    namespace {
        constexpr int FrontTouchWidth = 1920;
        constexpr int FrontTouchHeight = 1088;
        constexpr int RuntimeWidth = 960;
        constexpr int RuntimeHeight = 544;
    }

    /// Creates the backend and allocates the persistent gamepad storage reused every frame.
    PsVitaInputBackend::PsVitaInputBackend()
        : CachedMouseState(0, 0, 0, ButtonState::Released, ButtonState::Released, ButtonState::Released, ButtonState::Released, ButtonState::Released) {
        sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
        sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

        PersistentGamepads = new Array<InputGamepadState>(1);
        CachedFrame.set_Gamepads(PersistentGamepads);
        CachedFrame.set_GamepadCount(1);
        CachedFrame.set_Mouse(CachedMouseState);
    }

    /// Releases the persistent gamepad storage owned by the backend.
    PsVitaInputBackend::~PsVitaInputBackend() {
        delete PersistentGamepads;
        PersistentGamepads = nullptr;
    }

    /// Returns whether the temporary Vita backend receives input while the app is in the background.
    bool PsVitaInputBackend::get_ReceiveInputInBackground() {
        return ReceiveInputInBackground;
    }

    /// Records whether the temporary Vita backend receives input while the app is in the background.
    void PsVitaInputBackend::set_ReceiveInputInBackground(bool value) {
        ReceiveInputInBackground = value;
    }

    /// Converts one unsigned Vita analog axis into the shared signed stick range.
    short PsVitaInputBackend::ConvertAnalogAxis(std::uint8_t value) {
        int centered = static_cast<int>(value) - 127;
        int scaled = centered * 256;
        scaled = std::clamp(scaled, -32768, 32767);
        return static_cast<short>(scaled);
    }

    /// Converts one raw front-touch X coordinate into runtime window-space coordinates.
    int PsVitaInputBackend::ConvertFrontTouchX(int rawX) {
        int scaled = (rawX * RuntimeWidth) / FrontTouchWidth;
        return std::clamp(scaled, 0, RuntimeWidth - 1);
    }

    /// Converts one raw front-touch Y coordinate into runtime window-space coordinates.
    int PsVitaInputBackend::ConvertFrontTouchY(int rawY) {
        int scaled = (rawY * RuntimeHeight) / FrontTouchHeight;
        return std::clamp(scaled, 0, RuntimeHeight - 1);
    }

    /// Updates the persistent shared gamepad slot from the current Vita controller state.
    void PsVitaInputBackend::UpdateGamepadState() {
        if (PersistentGamepads == nullptr) {
            return;
        }

        SceCtrlData padData {};
        int readCount = sceCtrlPeekBufferPositive(0, &padData, 1);

        InputGamepadState gamepadState;
        if (readCount > 0) {
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
        }

        gamepadState.set_LeftTrigger(0);
        gamepadState.set_RightTrigger(0);
        (*PersistentGamepads)[0] = gamepadState;
    }

    /// Updates the cached mouse state from the current front-touch state so the shared pointer pipeline can consume it.
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
        CachedFrame.set_Mouse(CachedMouseState);
    }

    /// Captures the next runtime input frame using the persistent controller and front-touch storage.
    ::InputFrameState PsVitaInputBackend::CaptureFrame() {
        UpdateGamepadState();
        UpdateFrontTouchMouseState();
        CachedFrame.set_GamepadCount(1);
        CachedFrame.set_Gamepads(PersistentGamepads);
        return CachedFrame;
    }
}

#endif
