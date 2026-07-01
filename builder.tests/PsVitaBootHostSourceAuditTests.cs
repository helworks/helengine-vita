using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita boot host source so generated-core builds enter Helengine runtime startup instead of remaining a permanent blue-screen loop.
/// </summary>
public sealed class PsVitaBootHostSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita boot host contains an explicit generated-core startup path and a runtime main loop entrypoint.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreIsEnabled_entersRuntimeStartupAndMainLoop() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::Core* EngineCore;", headerSource, StringComparison.Ordinal);
        Assert.Contains("::PlatformInfo* EnginePlatformInfo;", headerSource, StringComparison.Ordinal);
        Assert.Contains("void InitializeCore();", headerSource, StringComparison.Ordinal);
        Assert.Contains("void LoadStartupScene();", headerSource, StringComparison.Ordinal);
        Assert.Contains("void RunMainLoop();", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmRenderer", headerSource, StringComparison.Ordinal);
        Assert.Contains("#if HELENGINE_PSVITA_HAS_GENERATED_CORE", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"Core.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"PlatformInfo.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"SceneManager.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/psvita/rendering/PsVitaGxmRenderer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_scene_catalog_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_startup_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InitializeCore();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("LoadStartupScene();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("RunMainLoop();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer = new rendering::PsVitaGxmRenderer();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->SetGxmRenderer(GxmRenderer);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->NotifyFramePresented();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo = new PlatformInfo(\"PS Vita\", \"homebrew\");", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_CommitPendingSceneOperationsDuringDraw(false);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, EngineOptions);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("he_get_runtime_startup_scene_relative_path()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("unsigned int ResolveActiveCameraClearColorAbgr();", headerSource, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer->BeginFrame(ResolveActiveCameraClearColorAbgr());", sourceCode, StringComparison.Ordinal);
        int presentFrameIndex = sourceCode.IndexOf("PresentFrame();", StringComparison.Ordinal);
        int notifyFramePresentedIndex = sourceCode.IndexOf("static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->NotifyFramePresented();", StringComparison.Ordinal);
        int flushReleasedTexturesIndex = sourceCode.IndexOf("EngineRenderManager2D->FlushReleasedTextures();", StringComparison.Ordinal);
        int completeFrameBoundaryIndex = sourceCode.IndexOf("EngineCore->CompleteFrameBoundary();", StringComparison.Ordinal);
        Assert.True(presentFrameIndex >= 0, "PS Vita boot host must present each frame before completing the explicit scene-operation boundary.");
        Assert.True(notifyFramePresentedIndex > presentFrameIndex, "PS Vita boot host must mark one presented Vita frame before aging deferred texture releases.");
        Assert.True(flushReleasedTexturesIndex > presentFrameIndex, "PS Vita boot host must process deferred 2D texture releases only after frame presentation.");
        Assert.True(flushReleasedTexturesIndex > notifyFramePresentedIndex, "PS Vita boot host must signal the presented-frame gate before flushing deferred 2D textures.");
        Assert.True(completeFrameBoundaryIndex > presentFrameIndex, "PS Vita boot host must complete the explicit scene-operation boundary after frame presentation.");
        Assert.True(completeFrameBoundaryIndex > flushReleasedTexturesIndex, "PS Vita boot host must process deferred 2D texture releases before completing the explicit scene-operation boundary.");
        Assert.Contains("EngineRenderManager2D->Draw();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PresentFrame();", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita builds resolve the active camera clear color instead of hardcoding the bootstrap cornflower-blue clear.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreMainMenuDraws_resolvesClearColorFromTheActiveCamera() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("CameraClearSettings", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ObjectManager->get_Cameras()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_ClearSettings()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_ClearColorEnabled()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_ClearColor()", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("GxmRenderer->BeginFrame(CornflowerBlue);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita builds apply the generated standard-platform input manifest during runtime initialization so menu actions can resolve from platform mappings.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreRuntimeInitializes_appliesStandardPlatformInputManifest() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"StandardPlatformActionBinding.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformInputConfiguration.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_standard_platform_input_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const HERuntimeStandardPlatformActionEntry* manifestEntries = he_runtime_standard_platform_action_entries(&count);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita builds initialize one runtime scene manager from the packaged scene catalog and load the startup scene through that manager so menu scene transitions can resolve.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreStartupLoadsMainMenu_initializesSceneManagerBackedStartupFlow() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"SceneManager.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeSceneCatalogFactory runtimeSceneCatalogFactory;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_SceneCatalog(runtimeSceneCatalogFactory.Build());", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (EngineCore->get_SceneManager() == nullptr)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("sceneLoadService->Load(startupScene);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita builds wire the runtime diagnostics provider into core initialization so scene reload failures can be traced from the persisted boot log.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreRuntimeInitializes_wiresRuntimeSceneTransitionDiagnosticsProvider() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("class PsVitaRuntimeDiagnosticsProvider;", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeDiagnosticsProvider* EngineRuntimeDiagnosticsProvider;", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/psvita/PsVitaRuntimeDiagnosticsProvider.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRuntimeDiagnosticsProvider = new PsVitaRuntimeDiagnosticsProvider();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_RuntimeDiagnosticsProvider(EngineRuntimeDiagnosticsProvider);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita builds move queued scene operations to the explicit post-present frame boundary instead of committing them during draw.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreNeedsPostPresentSceneCommit_usesExplicitFrameBoundaryFlow() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("EngineOptions->set_CommitPendingSceneOperationsDuringDraw(false);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer->PresentFrame();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->NotifyFramePresented();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->FlushReleasedTextures();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->CompleteFrameBoundary();", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("FinalizeReleasedTexturesAfterPresent();", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies generated-core Vita native builds compile the runtime diagnostics provider implementation so the boot host can link its vtable and emit scene reload traces.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreBuildsNativeTarget_compilesRuntimeDiagnosticsProvider() {
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("src/platform/psvita/PsVitaRuntimeDiagnosticsProvider.cpp", cmakeSource, StringComparison.Ordinal);
    }
}
