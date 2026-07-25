using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the forward-Lambert uniform binding boundary so lighting values are uploaded to GXM instead of computed per vertex on the CPU.
/// </summary>
public sealed class PsVitaForwardLambertUniformSourceAuditTests {
    /// <summary>
    /// Verifies that the uniform binder writes all matrices, material values, and light values through GXM uniform buffers.
    /// </summary>
    [Fact]
    public void Source_whenBindingForwardLambertUniforms_writesCompleteGpuContract() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaForwardLambertUniformBinder.cpp");
        string source = File.Exists(sourcePath) ? File.ReadAllText(sourcePath) : string.Empty;

        Assert.True(File.Exists(sourcePath), "Expected one forward-Lambert uniform binder source file.");
        Assert.Contains("sceGxmReserveVertexDefaultUniformBuffer", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmReserveFragmentDefaultUniformBuffer", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmSetUniformDataF", source, StringComparison.Ordinal);
        Assert.Contains("normalTransformParameter", source, StringComparison.Ordinal);
        Assert.Contains("lightDirectionParameter", source, StringComparison.Ordinal);
        Assert.Contains("lightColorParameter", source, StringComparison.Ordinal);
        Assert.Contains("ambientParameter", source, StringComparison.Ordinal);
        Assert.DoesNotContain("BuildLambertVertexColor", source, StringComparison.Ordinal);
    }
}
