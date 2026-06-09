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
        Assert.Contains("#include \"platform/psvita/rendering/PsVitaGxmRenderer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_scene_catalog_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_startup_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InitializeCore();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("LoadStartupScene();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("RunMainLoop();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer = new rendering::PsVitaGxmRenderer();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("static_cast<PsVitaRenderManager2D*>(EngineRenderManager2D)->SetGxmRenderer(GxmRenderer);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo = new PlatformInfo(\"psvita\", \"01.00\");", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, EngineOptions);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("he_get_runtime_startup_scene_relative_path()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("contentManager->Load<SceneAsset*>(configuredStartupSceneRelativePath, RuntimeContentProcessorIds::SceneAsset);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceneLoadService->Load(startupScene);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("unsigned int ResolveActiveCameraClearColorAbgr();", headerSource, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer->BeginFrame(ResolveActiveCameraClearColorAbgr());", sourceCode, StringComparison.Ordinal);
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
}
