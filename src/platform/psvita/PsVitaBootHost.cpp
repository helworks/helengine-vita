#include "platform/psvita/PsVitaBootHost.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <stdexcept>
#include <string>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
#include "Core.hpp"
#include "CameraClearSettings.hpp"
#include "CoreInitializationOptions.hpp"
#include "ICamera.hpp"
#include "ObjectManager.hpp"
#include "PlatformInfo.hpp"
#include "RuntimeSceneLoadService.hpp"
#include "SceneManager.hpp"
#include "platform/psvita/PsVitaInputBackend.hpp"
#include "platform/psvita/PsVitaRuntimeDiagnosticsProvider.hpp"
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

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
        /// Appends the latest generated-core scene manager trace state when one scene transition fails.
        void AppendSceneManagerTrace(PsVitaBootHost* bootHost, Core* engineCore) {
            if (bootHost == nullptr || engineCore == nullptr || engineCore->get_SceneManager() == nullptr) {
                return;
            }

            SceneManager* sceneManager = engineCore->get_SceneManager();
            std::FILE* file = std::fopen(BootTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::string stageMessage = "SceneManagerTrace: stage=" + sceneManager->get_LastTraceStage();
            std::string sceneMessage = "SceneManagerTrace: scene=" + sceneManager->get_LastTraceSceneId();
            std::fputs(stageMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(sceneMessage.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }

        /// Appends the latest generated-core scene load trace state when one scene materialization fails.
        void AppendSceneLoadTrace(PsVitaBootHost* bootHost, Core* engineCore) {
            if (bootHost == nullptr || engineCore == nullptr || engineCore->get_SceneLoadService() == nullptr) {
                return;
            }

            RuntimeSceneLoadService* sceneLoadService = engineCore->get_SceneLoadService();
            std::FILE* file = std::fopen(BootTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::string stageMessage = "SceneLoadTrace: stage=" + sceneLoadService->get_LastTraceStage();
            std::string rootEntityMessage = "SceneLoadTrace: rootEntityIndex=" + std::to_string(sceneLoadService->get_LastTraceRootEntityIndex());
            std::string entityDepthMessage = "SceneLoadTrace: entityDepth=" + std::to_string(sceneLoadService->get_LastTraceEntityDepth());
            std::string componentTypeMessage = "SceneLoadTrace: componentType=" + sceneLoadService->get_LastTraceComponentTypeId();
            std::string textStageMessage = "SceneLoadTrace: textLoadStage=" + sceneLoadService->get_LastTextLoadStage();
            std::string textFontMessage = "SceneLoadTrace: textFont=" + sceneLoadService->get_LastTextFontRelativePath();
            std::string textureStageMessage = "SceneLoadTrace: textureLoadStage=" + sceneLoadService->get_LastTextureLoadStage();
            std::string texturePathMessage = "SceneLoadTrace: texturePath=" + sceneLoadService->get_LastTextureRelativePath();
            std::string fontDeserializeMessage = "SceneLoadTrace: fontDeserializeStage=" + sceneLoadService->get_LastFontDeserializeStage();

            std::fputs(stageMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(rootEntityMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(entityDepthMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(componentTypeMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(textStageMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(textFontMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(textureStageMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(texturePathMessage.c_str(), file);
            std::fputc('\n', file);
            std::fputs(fontDeserializeMessage.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }
#endif
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
        , EngineRuntimeDiagnosticsProvider(nullptr)
        , GxmRenderer(nullptr)
#endif
    {
    }

    /// Initializes the PS Vita display path and keeps the first boot frame visible until shutdown.
    int PsVitaBootHost::Run() {
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
        } catch (const std::exception& ex) {
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
            AppendSceneManagerTrace(this, EngineCore);
            AppendSceneLoadTrace(this, EngineCore);
#endif
            AppendBootTrace(("Run: exception: " + std::string(ex.what())).c_str());
            return 1;
        } catch (...) {
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
            AppendSceneManagerTrace(this, EngineCore);
            AppendSceneLoadTrace(this, EngineCore);
#endif
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

        PsVitaRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;
        EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());

        EngineRenderManager3D = new PsVitaRenderManager3D();
        EngineRenderManager2D = new PsVitaRenderManager2D();
        static_cast<PsVitaRenderManager3D*>(EngineRenderManager3D)->SetGxmRenderer(GxmRenderer);
        static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->SetGxmRenderer(GxmRenderer);
        EngineInputBackend = new PsVitaInputBackend();
        EnginePlatformInfo = new PlatformInfo("psvita", "1");
        EngineRuntimeDiagnosticsProvider = new PsVitaRuntimeDiagnosticsProvider();
        EngineRenderManager3D->AddWindow(0, ScreenWidth, ScreenHeight);
        EngineOptions->set_RuntimeDiagnosticsProvider(EngineRuntimeDiagnosticsProvider);
        EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo);
        AppendBootTrace("InitializeCore: success");
    }

    /// Loads the packaged startup scene through the generated-core scene manager when runtime manifests are available.
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
        AppendBootTrace("RunMainLoop: first update");
        EngineCore->Update(FrameDeltaSeconds);
        AppendBootTrace("RunMainLoop: first begin frame");
        GxmRenderer->BeginFrame(ResolveActiveCameraClearColorAbgr());
        AppendBootTrace("RunMainLoop: first commit pending");
        if (EngineCore->get_SceneManager() != nullptr) {
            EngineCore->get_SceneManager()->CommitPendingOperationsAtFrameBoundary();
        }
        AppendBootTrace("RunMainLoop: first commit pending success");
        AppendBootTrace("RunMainLoop: first render manager 3d draw");
        EngineRenderManager3D->Draw();
        AppendBootTrace("RunMainLoop: first render manager 3d draw success");
        AppendBootTrace("RunMainLoop: first draw2d");
        EngineRenderManager2D->Draw();
        AppendBootTrace("RunMainLoop: first present");
        PresentFrame();
        AppendBootTrace("RunMainLoop: first present complete");

        while (true) {
            EngineCore->Update(FrameDeltaSeconds);
            GxmRenderer->BeginFrame(ResolveActiveCameraClearColorAbgr());
            EngineCore->Draw();
            EngineRenderManager2D->Draw();
            PresentFrame();
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
            ::ICamera* camera = (*cameras).get_Item(cameraIndex);
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
