#pragma once

#include <psp2/display.h>

namespace helengine::psvita {
    /// Owns the first PS Vita native display bootstrap and boot-frame presentation loop.
    class PsVitaBootHost {
    public:
        /// Creates the PS Vita boot host with no initialized frame buffer state.
        PsVitaBootHost();

        /// Initializes the PS Vita display path and keeps the first boot frame visible until shutdown.
        int Run();

    private:
        /// Initializes the display buffer configuration required for the first native frame presentation.
        bool InitializeGraphics();

        /// Fills the front buffer with the milestone cornflower blue color.
        void ClearFrameBuffer();

        /// Presents the configured frame buffer to the display on the next vertical blank.
        void PresentFrame();

        /// Stores the display metadata passed into `sceDisplaySetFrameBuf`.
        SceDisplayFrameBuf FrameBuffer;

        /// Stores the owned pixel memory for the active frame buffer.
        unsigned int* Pixels;
    };
}
