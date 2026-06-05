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
        Assert.Contains("void InitializeCore();", headerSource, StringComparison.Ordinal);
        Assert.Contains("void LoadStartupScene();", headerSource, StringComparison.Ordinal);
        Assert.Contains("void RunMainLoop();", headerSource, StringComparison.Ordinal);
        Assert.Contains("#if HELENGINE_PSVITA_HAS_GENERATED_CORE", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"Core.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_scene_catalog_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_startup_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InitializeCore();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("LoadStartupScene();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("RunMainLoop();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo = new PlatformInfo(\"psvita\", \"1\");", sourceCode, StringComparison.Ordinal);
        Assert.Contains("he_get_runtime_startup_scene_relative_path()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("he_runtime_scene_catalog_entries(&runtimeSceneCount)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);", sourceCode, StringComparison.Ordinal);
    }
}
