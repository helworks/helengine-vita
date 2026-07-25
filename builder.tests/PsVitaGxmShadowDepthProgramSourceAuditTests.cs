using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the native depth-only GXM program used by the Vita shadow caster pass.
/// </summary>
public sealed class PsVitaGxmShadowDepthProgramSourceAuditTests {
    /// <summary>
    /// Verifies the depth program consumes compiled artifacts, reflects the light transform, and is included in the Vita build.
    /// </summary>
    [Fact]
    public void Source_whenDrawingShadowCasters_usesArtifactBackedDepthOnlyGxmProgram() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmShadowDepthProgram.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmShadowDepthProgram.cpp");
        string rendererHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.hpp");
        string rendererSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");

        string source = File.ReadAllText(sourcePath);
        string rendererHeader = File.ReadAllText(rendererHeaderPath);
        string rendererSource = File.ReadAllText(rendererSourcePath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.True(File.Exists(headerPath), "Expected one PS Vita depth-only GXM program header.");
        Assert.Contains("PsVitaShaderArtifactReader::TryReadBytes", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightViewProjection", source, StringComparison.Ordinal);
        Assert.Contains("SCE_GXM_PARAMETER_SEMANTIC_POSITION", source, StringComparison.Ordinal);
        Assert.Contains("vita2d_get_context", source, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_context", source, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmShadowDepthProgram.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("bool BeginShadowDepthPass()", rendererHeader, StringComparison.Ordinal);
        Assert.Contains("bool DrawShadowDepthMesh(", rendererHeader, StringComparison.Ordinal);
        Assert.Contains("ShadowDepthProgram.Initialize", rendererSource, StringComparison.Ordinal);
        Assert.Contains("ShadowMap.BeginDepthPass", rendererSource, StringComparison.Ordinal);
    }
}
