#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>

#include "IInputBackend.hpp"
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

        /// Updates the active shared gamepad slot from the current Vita controller state.
        void UpdateGamepadState();

        /// Stores whether background input capture is enabled for the temporary Vita backend.
        bool ReceiveInputInBackground = false;

        /// Stores two persistent gamepad arrays so consecutive frame snapshots remain independent.
        Array<InputGamepadState>* GamepadBuffers[2] = { nullptr, nullptr };

        /// Stores the gamepad buffer written by the next capture.
        int ActiveGamepadBufferIndex;

    };
}

#endif
