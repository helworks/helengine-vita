#pragma once

#include <psp2/display.h>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
class Core;
class CoreInitializationOptions;
class RenderManager3D;
class RenderManager2D;
class IInputBackend;
class PlatformInfo;
namespace helengine::psvita::rendering {
    class PsVitaGxmRenderer;
}
#endif

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

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        /// Initializes the generated-core runtime with temporary Vita-native backend stubs.
        void InitializeCore();

        /// Loads the packaged startup scene through the generated-core scene manager when runtime manifests are available.
        void LoadStartupScene();

        /// Advances the runtime update and draw loop while preserving the milestone boot frame.
        void RunMainLoop();
#endif

        /// Fills the front buffer with the milestone cornflower blue color.
        void ClearFrameBuffer();

        /// Presents the configured frame buffer to the display on the next vertical blank.
        void PresentFrame();

        /// Stores the display metadata passed into `sceDisplaySetFrameBuf`.
        SceDisplayFrameBuf FrameBuffer;

        /// Stores the owned pixel memory for the active frame buffer.
        unsigned int* Pixels;

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        /// Stores the generated-core runtime instance used by the PS Vita bootstrap loop.
        ::Core* EngineCore;

        /// Stores the generated-core initialization options used during PS Vita runtime startup.
        ::CoreInitializationOptions* EngineOptions;

        /// Stores the temporary 3D backend passed into generated-core startup.
        ::RenderManager3D* EngineRenderManager3D;

        /// Stores the temporary 2D backend passed into generated-core startup.
        ::RenderManager2D* EngineRenderManager2D;

        /// Stores the temporary input backend passed into generated-core startup.
        ::IInputBackend* EngineInputBackend;

        /// Stores the runtime platform descriptor required by generated-core startup.
        ::PlatformInfo* EnginePlatformInfo;

        /// Stores the native PS Vita GXM renderer that owns generated-core frame submission and presentation.
        rendering::PsVitaGxmRenderer* GxmRenderer;
#endif
    };
}
