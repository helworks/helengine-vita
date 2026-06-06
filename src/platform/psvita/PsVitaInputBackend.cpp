#include "platform/psvita/PsVitaInputBackend.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    /// Returns whether the temporary Vita backend receives input while the app is in the background.
    bool PsVitaInputBackend::get_ReceiveInputInBackground() {
        return ReceiveInputInBackground;
    }

    /// Records whether the temporary Vita backend receives input while the app is in the background.
    void PsVitaInputBackend::set_ReceiveInputInBackground(bool value) {
        ReceiveInputInBackground = value;
    }

    /// Captures the next runtime input frame as an empty state so the engine bootstrap can advance safely.
    ::InputFrameState PsVitaInputBackend::CaptureFrame() {
        return ::InputFrameState();
    }
}

#endif
