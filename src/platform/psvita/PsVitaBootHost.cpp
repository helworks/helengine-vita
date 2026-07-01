#include "platform/psvita/PsVitaBootHost.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <psp2/io/stat.h>
#include <stdexcept>
#include <string>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CameraClearSettings.hpp"
#include "ContentManager.hpp"
#include "CoreInitializationOptions.hpp"
#include "InputControlId.hpp"
#include "InputControlKind.hpp"
#include "InputDeviceKind.hpp"
#include "ICamera.hpp"
#include "ObjectManager.hpp"
#include "PlatformInfo.hpp"
#include "RuntimeContentProcessorIds.hpp"
#include "RuntimeSceneLoadService.hpp"
#include "SceneAsset.hpp"
#include "SceneManager.hpp"
#include "StandardPlatformAction.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "runtime/native_exceptions.hpp"
#include "platform/psvita/PsVitaInputBackend.hpp"
#include "platform/psvita/PsVitaRuntimeDiagnosticsProvider.hpp"
#include "platform/psvita/PsVitaRuntimeSceneCatalogFactory.hpp"
#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager3D.hpp"
#if __has_include("runtime/native_list.hpp") && __has_include("runtime/runtime_scene_catalog_manifest.hpp") && __has_include("runtime/runtime_standard_platform_input_manifest.hpp") && __has_include("runtime/runtime_startup_manifest.hpp")
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 1
#include "runtime/native_list.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#else
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 0
#endif
#endif

namespace helengine::psvita {
    namespace {
        constexpr double FrameDeltaSeconds = 1.0 / 60.0;
        constexpr const char* RuntimeContentRootPath = "app0:/";
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";
        constexpr unsigned int BufferAlignment = 0x40000;
        constexpr unsigned int ScreenWidth = 960;
        constexpr unsigned int ScreenHeight = 544;
        constexpr unsigned int PixelStride = ScreenWidth;
        constexpr unsigned int BytesPerPixel = sizeof(unsigned int);
        constexpr unsigned int PixelCount = PixelStride * ScreenHeight;
        constexpr unsigned int PixelBytes = PixelCount * BytesPerPixel;
        constexpr unsigned int AlignedPixelBytes = ((PixelBytes + BufferAlignment - 1) / BufferAlignment) * BufferAlignment;
        constexpr unsigned int CornflowerBlue = 0xFFED9564;

        /// Builds one runtime standard-platform input configuration from the generated manifest entries.
        ::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration() {
#if HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS
            std::size_t count = 0;
            const HERuntimeStandardPlatformActionEntry* manifestEntries = he_runtime_standard_platform_action_entries(&count);
            if (manifestEntries == nullptr || count == 0) {
                return ::StandardPlatformInputConfiguration::get_Empty();
            }

            List<::StandardPlatformActionBinding*>* bindings = new List<::StandardPlatformActionBinding*>(static_cast<int32_t>(count));
            for (int32_t index = 0; index < static_cast<int32_t>(count); index++) {
                const HERuntimeStandardPlatformActionEntry& manifestEntry = manifestEntries[index];
                bindings->Add(new ::StandardPlatformActionBinding(
                    static_cast<::StandardPlatformAction>(manifestEntry.ActionId),
                    ::InputControlId(
                        static_cast<::InputDeviceKind>(manifestEntry.DeviceKind),
                        static_cast<::InputControlKind>(manifestEntry.ControlKind),
                        manifestEntry.DeviceIndex,
                        manifestEntry.ControlIndex)));
            }

            return new ::StandardPlatformInputConfiguration(bindings);
#else
            return ::StandardPlatformInputConfiguration::get_Empty();
#endif
        }
    }

    /// Ensures the PS Vita shared data directory exists before any boot trace writes attempt to create files inside it.
    void PsVitaBootHost::EnsureBootTraceDirectoryExists() {
        sceIoMkdir("ux0:/data", 0777);
    }

    /// Creates the PS Vita boot host with no initialized frame buffer state.
    PsVitaBootHost::PsVitaBootHost()
        : FrameBuffer()
        , Pixels(nullptr)
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        , EngineCore(nullptr)
        , EngineOptions(nullptr)
        , EnginePlatformInfo(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputBackend(nullptr)
        , EngineRuntimeDiagnosticsProvider(nullptr)
        , GxmRenderer(nullptr)
#endif
    {
    }

    /// Initializes the PS Vita display path and keeps the first boot frame visible until shutdown.
    int PsVitaBootHost::Run() {
        EnsureBootTraceDirectoryExists();
        ResetBootTrace();
        AppendBootTrace("Run: begin");

        try {
            AppendBootTrace("Run: InitializeGraphics");
            if (!InitializeGraphics()) {
                AppendBootTrace("Run: InitializeGraphics failed");
                return 1;
            }

            AppendBootTrace("Run: ClearFrameBuffer");
            ClearFrameBuffer();

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
            AppendBootTrace("Run: InitializeCore");
            InitializeCore();
            AppendBootTrace("Run: LoadStartupScene");
            LoadStartupScene();
            AppendBootTrace("Run: RunMainLoop");
            RunMainLoop();
#else
            while (true) {
                PresentFrame();
            }
#endif
        } catch (const Exception* ex) {
            AppendBootTrace(("Run: managed exception: " + std::string(ex != nullptr ? ex->what() : "null managed exception")).c_str());
            return 1;
        } catch (const std::exception& ex) {
            AppendBootTrace(("Run: exception: " + std::string(ex.what())).c_str());
            return 1;
        } catch (...) {
            AppendBootTrace("Run: unknown exception");
            return 1;
        }

        return 0;
    }

    /// Clears the persisted Vita boot trace file before one new startup attempt.
    void PsVitaBootHost::ResetBootTrace() {
        std::FILE* file = std::fopen(BootTracePath, "w");
        if (file != nullptr) {
            std::fclose(file);
        }
    }

    /// Appends one line to the persisted Vita boot trace file.
    void PsVitaBootHost::AppendBootTrace(const char* message) {
        if (message == nullptr) {
            return;
        }

        std::FILE* file = std::fopen(BootTracePath, "a");
        if (file == nullptr) {
            return;
        }

        std::fputs(message, file);
        std::fputc('\n', file);
        std::fclose(file);
    }

    /// Initializes the display buffer configuration required for the first native frame presentation.
    bool PsVitaBootHost::InitializeGraphics() {
        AppendBootTrace("InitializeGraphics: begin");
        Pixels = static_cast<unsigned int*>(memalign(BufferAlignment, AlignedPixelBytes));
        if (Pixels == nullptr) {
            AppendBootTrace("InitializeGraphics: memalign failed");
            return false;
        }

        std::memset(&FrameBuffer, 0, sizeof(FrameBuffer));
        FrameBuffer.size = sizeof(SceDisplayFrameBuf);
        FrameBuffer.base = Pixels;
        FrameBuffer.pitch = PixelStride;
        FrameBuffer.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        FrameBuffer.width = ScreenWidth;
        FrameBuffer.height = ScreenHeight;

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        GxmRenderer = new rendering::PsVitaGxmRenderer();
        if (!GxmRenderer->Initialize()) {
            AppendBootTrace("InitializeGraphics: GxmRenderer initialize failed");
            return false;
        }
#endif

        AppendBootTrace("InitializeGraphics: success");
        return true;
    }

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
    /// Initializes the generated-core runtime with temporary Vita-native backend stubs.
    void PsVitaBootHost::InitializeCore() {
        AppendBootTrace("InitializeCore: begin");
        EngineCore = new Core();
        EngineOptions = EngineCore->get_InitializationOptions();
        EngineOptions->set_ContentRootPath(RuntimeContentRootPath);
        EngineOptions->set_UpdateOrderLayers(4);
        EngineOptions->set_RenderOrderLayers3D(4);
        EngineOptions->set_UpdateListInitialCapacity(64);
        EngineOptions->set_RenderList2DInitialCapacity(8);
        EngineOptions->set_RenderList3DInitialCapacity(64);
        EngineOptions->set_CommitPendingSceneOperationsDuringDraw(false);
        EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());
        EngineRuntimeDiagnosticsProvider = new PsVitaRuntimeDiagnosticsProvider();
        EngineOptions->set_RuntimeDiagnosticsProvider(EngineRuntimeDiagnosticsProvider);
        PsVitaRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;
        EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());

        EnginePlatformInfo = new PlatformInfo("PS Vita", "homebrew");
        EngineRenderManager3D = new PsVitaRenderManager3D();
        EngineRenderManager2D = new PsVitaRenderManager2D();
        static_cast<PsVitaRenderManager3D*>(EngineRenderManager3D)->SetGxmRenderer(GxmRenderer);
        static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->SetGxmRenderer(GxmRenderer);
        EngineInputBackend = new PsVitaInputBackend();
        EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);
        EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, EngineOptions);
        AppendBootTrace("InitializeCore: success");
    }

    /// Loads the packaged startup scene through the generated-core content and runtime scene-load services when runtime manifests are available.
    void PsVitaBootHost::LoadStartupScene() {
#if HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS
        AppendBootTrace("LoadStartupScene: begin");
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
        AppendBootTrace("LoadStartupScene: success");
#endif
    }

    /// Advances the runtime update and draw loop while preserving the milestone boot frame.
    void PsVitaBootHost::RunMainLoop() {
        bool firstFrame = true;
        while (true) {
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first update");
            }
            EngineCore->Update();
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first begin frame");
            }
            GxmRenderer->BeginFrame(ResolveActiveCameraClearColorAbgr());
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first draw3d");
            }
            EngineCore->Draw();
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first draw2d");
            }
            EngineRenderManager2D->Draw();
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first present");
            }
            PresentFrame();
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first present complete");
                AppendBootTrace("RunMainLoop: first release flush");
            }
            if (EngineRenderManager2D != nullptr) {
                static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->NotifyFramePresented();
                EngineRenderManager2D->FlushReleasedTextures();
            }
            if (firstFrame) {
                AppendBootTrace("RunMainLoop: first frame boundary");
            }
            EngineCore->CompleteFrameBoundary();
            if (firstFrame) {
                firstFrame = false;
            }
        }
    }

    /// Resolves the clear color for the active runtime camera, falling back to the bootstrap color when none is authored.
    unsigned int PsVitaBootHost::ResolveActiveCameraClearColorAbgr() {
        if (EngineCore == nullptr || EngineCore->ObjectManager == nullptr) {
            return CornflowerBlue;
        }

        List<::ICamera*>* cameras = EngineCore->ObjectManager->get_Cameras();
        if (cameras == nullptr) {
            return CornflowerBlue;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ::ICamera* camera = (*cameras)[cameraIndex];
            if (camera == nullptr) {
                continue;
            }

            CameraClearSettings clearSettings = camera->get_ClearSettings();
            if (!clearSettings.get_ClearColorEnabled()) {
                continue;
            }

            float4 clearColor = clearSettings.get_ClearColor();
            std::uint32_t red = static_cast<std::uint32_t>(std::lround(std::clamp(clearColor.X, 0.0f, 1.0f) * 255.0f));
            std::uint32_t green = static_cast<std::uint32_t>(std::lround(std::clamp(clearColor.Y, 0.0f, 1.0f) * 255.0f));
            std::uint32_t blue = static_cast<std::uint32_t>(std::lround(std::clamp(clearColor.Z, 0.0f, 1.0f) * 255.0f));
            std::uint32_t alpha = static_cast<std::uint32_t>(std::lround(std::clamp(clearColor.W, 0.0f, 1.0f) * 255.0f));
            return (alpha << 24)
                | (blue << 16)
                | (green << 8)
                | red;
        }

        return CornflowerBlue;
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
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        if (GxmRenderer != nullptr) {
            GxmRenderer->PresentFrame();
            return;
        }
#endif

        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(&FrameBuffer, SCE_DISPLAY_SETBUF_NEXTFRAME);
    }
}
