using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita native renderer source so the first programmable solid-color mesh path is present before runtime integration begins.
/// </summary>
public sealed class PsVitaGxmSolidColorProgramSourceAuditTests {
    /// <summary>
    /// Verifies the repository contains one dedicated solid-color shader program wrapper that owns both PS Vita vertex and fragment program handles.
    /// </summary>
    [Fact]
    public void Source_whenAddingSolidColorShaderPath_containsDedicatedProgramHandles() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.hpp");

        Assert.True(File.Exists(headerPath), "Expected one dedicated PS Vita solid-color shader program header.");

        string headerSource = File.ReadAllText(headerPath);
        Assert.Contains("class PsVitaGxmSolidColorProgram", headerSource, StringComparison.Ordinal);
        Assert.Contains("SceGxmVertexProgram*", headerSource, StringComparison.Ordinal);
        Assert.Contains("SceGxmFragmentProgram*", headerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the dedicated solid-color shader wrapper creates real GXM shader programs instead of routing through the existing vita2d immediate draw helpers.
    /// </summary>
    [Fact]
    public void Source_whenAddingSolidColorShaderPath_createsRealGxmPrograms() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.cpp");

        Assert.True(File.Exists(sourcePath), "Expected one dedicated PS Vita solid-color shader program implementation.");

        string sourceCode = File.ReadAllText(sourcePath);
        Assert.Contains("sceGxmShaderPatcherCreateVertexProgram", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreateFragmentProgram", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("vita2d_draw_array", sourceCode, StringComparison.Ordinal);
    }
}
