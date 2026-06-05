#include "platform/psvita/PsVitaBootHost.hpp"

#include <cstring>
#include <malloc.h>
#include <stdexcept>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "PlatformInfo.hpp"
#include "SceneManager.hpp"
#include "platform/psvita/PsVitaInputBackend.hpp"
#include "platform/psvita/PsVitaRuntimeSceneCatalogFactory.hpp"
#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager3D.hpp"
#include "SceneLoadMode.hpp"
#if __has_include("runtime/runtime_scene_catalog_manifest.hpp") && __has_include("runtime/runtime_startup_manifest.hpp")
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 1
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#else
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 0
#endif
#endif

namespace helengine::psvita {
    namespace {
        constexpr double FrameDeltaSeconds = 1.0 / 60.0;
        constexpr const char* RuntimeContentRootPath = "app0:/";
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
        , Pixels(nullptr)
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        , EngineCore(nullptr)
        , EngineOptions(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputBackend(nullptr)
        , EnginePlatformInfo(nullptr)
#endif
    {
    }

    /// Initializes the PS Vita display path and keeps the first boot frame visible until shutdown.
    int PsVitaBootHost::Run() {
        if (!InitializeGraphics()) {
            return 1;
        }

        ClearFrameBuffer();

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        InitializeCore();
        LoadStartupScene();
        RunMainLoop();
#else
        while (true) {
            PresentFrame();
        }
#endif

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

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
    /// Initializes the generated-core runtime with temporary Vita-native backend stubs.
    void PsVitaBootHost::InitializeCore() {
        EngineCore = new Core();
        EngineOptions = EngineCore->get_InitializationOptions();
        EngineOptions->set_ContentRootPath(RuntimeContentRootPath);
        EngineOptions->set_UpdateOrderLayers(4);
        EngineOptions->set_RenderOrderLayers3D(4);
        EngineOptions->set_UpdateListInitialCapacity(64);
        EngineOptions->set_RenderList2DInitialCapacity(8);
        EngineOptions->set_RenderList3DInitialCapacity(64);

        PsVitaRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;
        EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());

        EngineRenderManager3D = new PsVitaRenderManager3D();
        EngineRenderManager2D = new PsVitaRenderManager2D();
        EngineInputBackend = new PsVitaInputBackend();
        EnginePlatformInfo = new PlatformInfo("psvita", "1");
        EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);
        EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo);
    }

    /// Loads the packaged startup scene through the generated-core scene manager when runtime manifests are available.
    void PsVitaBootHost::LoadStartupScene() {
#if HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS
        const char* configuredStartupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
        if (configuredStartupSceneRelativePath == nullptr || configuredStartupSceneRelativePath[0] == '\0') {
            throw std::runtime_error("PS Vita runtime startup manifest did not define a startup scene.");
        }

        std::size_t runtimeSceneCount = 0;
        const HERuntimeSceneCatalogEntry* runtimeSceneEntries = he_runtime_scene_catalog_entries(&runtimeSceneCount);
        if (runtimeSceneEntries == nullptr || runtimeSceneCount == 0) {
            throw std::runtime_error("PS Vita runtime scene catalog manifest did not contain any entries.");
        }

        std::string startupSceneId;
        for (std::size_t index = 0; index < runtimeSceneCount; index++) {
            const HERuntimeSceneCatalogEntry& runtimeSceneEntry = runtimeSceneEntries[index];
            if (runtimeSceneEntry.CookedRelativePath != nullptr && std::string(runtimeSceneEntry.CookedRelativePath) == configuredStartupSceneRelativePath) {
                startupSceneId = runtimeSceneEntry.SceneId;
                break;
            }
        }

        if (startupSceneId.empty()) {
            throw std::runtime_error("PS Vita runtime startup scene path was not found in the runtime scene catalog manifest.");
        }

        if (EngineCore->get_SceneManager() == nullptr) {
            throw std::runtime_error("PS Vita runtime scene manager was not initialized before startup scene loading.");
        }

        EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
#endif
    }

    /// Advances the runtime update and draw loop while preserving the milestone boot frame.
    void PsVitaBootHost::RunMainLoop() {
        while (true) {
            EngineCore->Update(FrameDeltaSeconds);
            ClearFrameBuffer();
            EngineCore->Draw();
            PresentFrame();
        }
    }
#endif

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
