#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "IInputBackend.hpp"

namespace helengine::psvita {
    /// Provides the temporary PS Vita runtime input bridge while platform-specific controls are not wired yet.
    class PsVitaInputBackend final : public ::IInputBackend {
    public:
        /// Returns whether the temporary Vita backend receives input while the app is in the background.
        bool get_ReceiveInputInBackground();

        /// Records whether the temporary Vita backend receives input while the app is in the background.
        void set_ReceiveInputInBackground(bool value);

        /// Captures the next runtime input frame as an empty state so the engine bootstrap can advance safely.
        ::InputFrameState CaptureFrame() override;

    private:
        /// Stores whether background input capture is enabled for the temporary Vita backend.
        bool ReceiveInputInBackground = false;
    };
}

#endif
