using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the runnable Vita command that produces artifacts for host retrieval.
/// </summary>
public sealed class PsVitaShaderArtifactExportCommandSourceAuditTests {
    /// <summary>
    /// Verifies that the normal Vita executable exposes a marker-file trigger for forward-Lambert export.
    /// </summary>
    [Fact]
    public void Source_whenLaunchingExportCommand_writesBothStageArtifactsToVitaData() {
        string mainSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "main.cpp"));
        string commandSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactExportCommand.cpp"));
        string cmakeSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt"));

        Assert.Contains("export-forward-lambert.flag", mainSource, StringComparison.Ordinal);
        Assert.Contains("sceIoGetstat", mainSource, StringComparison.Ordinal);
        Assert.Contains("ForwardLambertShader.vp.pvsa", commandSource, StringComparison.Ordinal);
        Assert.Contains("ForwardLambertShader.fp.pvsa", commandSource, StringComparison.Ordinal);
        Assert.Contains("sceIoMkdir", commandSource, StringComparison.Ordinal);
        Assert.Contains("shader-export.log", commandSource, StringComparison.Ordinal);
        Assert.Contains("AppendLog", commandSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaShaderArtifactExportCommand.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("if (result == 0)", mainSource, StringComparison.Ordinal);
    }
}
