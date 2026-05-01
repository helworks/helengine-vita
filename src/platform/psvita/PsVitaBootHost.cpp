#include "platform/psvita/PsVitaBootHost.hpp"

#include <cstring>
#include <malloc.h>

namespace helengine::psvita {
    namespace {
        constexpr unsigned int BufferAlignment = 0x40000;
        constexpr unsigned int ScreenWidth = 960;
        constexpr unsigned int ScreenHeight = 544;
        constexpr unsigned int PixelStride = ScreenWidth;
        constexpr unsigned int BytesPerPixel = sizeof(unsigned int);
        constexpr unsigned int PixelCount = PixelStride * ScreenHeight;
        constexpr unsigned int PixelBytes = PixelCount * BytesPerPixel;
        constexpr unsigned int AlignedPixelBytes = ((PixelBytes + BufferAlignment - 1) / BufferAlignment) * BufferAlignment;
        constexpr unsigned int CornflowerBlue = 0xFFED9564;
    }

    /// Creates the PS Vita boot host with no initialized frame buffer state.
    PsVitaBootHost::PsVitaBootHost()
        : FrameBuffer()
        , Pixels(nullptr) {
    }

    /// Initializes the PS Vita display path and keeps the first boot frame visible until shutdown.
    int PsVitaBootHost::Run() {
        if (!InitializeGraphics()) {
            return 1;
        }

        ClearFrameBuffer();

        while (true) {
            PresentFrame();
        }

        return 0;
    }

    /// Initializes the display buffer configuration required for the first native frame presentation.
    bool PsVitaBootHost::InitializeGraphics() {
        Pixels = static_cast<unsigned int*>(memalign(BufferAlignment, AlignedPixelBytes));
        if (Pixels == nullptr) {
            return false;
        }

        std::memset(&FrameBuffer, 0, sizeof(FrameBuffer));
        FrameBuffer.size = sizeof(SceDisplayFrameBuf);
        FrameBuffer.base = Pixels;
        FrameBuffer.pitch = PixelStride;
        FrameBuffer.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        FrameBuffer.width = ScreenWidth;
        FrameBuffer.height = ScreenHeight;

        return true;
    }

    /// Fills the front buffer with the milestone cornflower blue color.
    void PsVitaBootHost::ClearFrameBuffer() {
        for (unsigned int pixelIndex = 0; pixelIndex < PixelCount; ++pixelIndex) {
            Pixels[pixelIndex] = CornflowerBlue;
        }
    }

    /// Presents the configured frame buffer to the display on the next vertical blank.
    void PsVitaBootHost::PresentFrame() {
        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(&FrameBuffer, SCE_DISPLAY_SETBUF_NEXTFRAME);
    }
}
