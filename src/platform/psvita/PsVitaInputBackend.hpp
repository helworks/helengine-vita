#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

#include "IInputBackend.hpp"
#include "MouseState.hpp"
#include "runtime/array.hpp"

namespace helengine::psvita {
    /// Provides the PS Vita runtime input bridge for controller and front-touch capture without per-frame heap allocation.
    class PsVitaInputBackend final : public ::IInputBackend {
    public:
        /// Creates the backend and allocates the persistent gamepad storage reused every frame.
        PsVitaInputBackend();

        /// Releases the persistent gamepad storage owned by the backend.
        ~PsVitaInputBackend();

        /// Returns whether the temporary Vita backend receives input while the app is in the background.
        bool get_ReceiveInputInBackground();

        /// Records whether the temporary Vita backend receives input while the app is in the background.
        void set_ReceiveInputInBackground(bool value);

        /// Captures the next runtime input frame using the persistent controller and front-touch storage.
        ::InputFrameState CaptureFrame() override;

    private:
        /// Converts one unsigned Vita analog axis into the shared signed stick range.
        static short ConvertAnalogAxis(std::uint8_t value);

        /// Converts one raw front-touch X coordinate into runtime window-space coordinates.
        static int ConvertFrontTouchX(int rawX);

        /// Converts one raw front-touch Y coordinate into runtime window-space coordinates.
        static int ConvertFrontTouchY(int rawY);

        /// Updates the persistent shared gamepad slot from the current Vita controller state.
        void UpdateGamepadState();

        /// Updates the cached mouse state from the current front-touch state so the shared pointer pipeline can consume it.
        void UpdateFrontTouchMouseState();

        /// Stores whether background input capture is enabled for the temporary Vita backend.
        bool ReceiveInputInBackground = false;

        /// Stores the persistent shared gamepad array reused by every captured frame.
        Array<InputGamepadState>* PersistentGamepads = nullptr;

        /// Stores the persistent frame reused by every capture.
        ::InputFrameState CachedFrame;

        /// Stores the cached mouse snapshot that carries front-touch state into the shared pointer pipeline.
        ::MouseState CachedMouseState;

        /// Stores the previous front-touch X position for stable fallback when no touch is active.
        int PreviousTouchX = 0;

        /// Stores the previous front-touch Y position for stable fallback when no touch is active.
        int PreviousTouchY = 0;
    };
}

#endif
