using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita native renderer source so the solid-color mesh path stays aligned with the shared shader pipeline architecture.
/// </summary>
public sealed class PsVitaGxmSolidColorProgramSourceAuditTests {
    /// <summary>
    /// Verifies the repository contains one dedicated solid-color shader program wrapper file that remains part of the native build.
    /// </summary>
    [Fact]
    public void Source_whenAddingSolidColorShaderPath_containsDedicatedSolidColorProgramFiles() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.hpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");

        Assert.True(File.Exists(headerPath), "Expected one dedicated PS Vita solid-color shader program header.");

        string headerSource = File.ReadAllText(headerPath);
        string cmakeSource = File.ReadAllText(cmakePath);
        Assert.Contains("class PsVitaGxmSolidColorProgram", headerSource, StringComparison.Ordinal);
        Assert.Contains("SceGxmContext*", headerSource, StringComparison.Ordinal);
        Assert.Contains("SceGxmVertexProgram*", headerSource, StringComparison.Ordinal);
        Assert.Contains("SceGxmFragmentProgram*", headerSource, StringComparison.Ordinal);
        Assert.Contains("const SceGxmProgramParameter*", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmSolidColorProgram.cpp", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the programmable PS Vita mesh path does not borrow vita2d shader globals or route 3D meshes through vita2d array overrides.
    /// </summary>
    [Fact]
    public void Source_whenUsingProgrammableVitaMeshPath_doesNotBorrowVita2dShaderGlobals() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.cpp");

        Assert.True(File.Exists(sourcePath), "Expected one dedicated PS Vita solid-color shader program implementation.");

        string rendererSource = File.ReadAllText(GetRendererPath());
        string drawSolidColorMeshSource = GetDrawSolidColorMeshSource();
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("_vita2d_colorVertexProgram", rendererSource, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorFragmentProgram", rendererSource, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorWvpParam", rendererSource, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_ortho_matrix", rendererSource, StringComparison.Ordinal);
        Assert.DoesNotContain("vita2d_draw_array(", drawSolidColorMeshSource, StringComparison.Ordinal);

        Assert.DoesNotContain("_vita2d_context", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorVertexProgram", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorFragmentProgram", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorWvpParam", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("vita2d_draw_array", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the temporary solid-color mesh path has been reset to the explicit false-return placeholder until real compiled GXM programs are wired in.
    /// </summary>
    [Fact]
    public void Source_whenSolidColorMeshShaderPipelineIsNotImplemented_returnsFalsePlaceholder() {
        string sourceCode = File.ReadAllText(GetRendererPath());

        Assert.Contains("DrawSolidColorMesh", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)worldViewProjection;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)positions;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)positionCount;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)indices;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)indexCount;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("(void)colorAbgr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("return false;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Resolves the PS Vita GXM renderer source path used by the architecture audits.
    /// </summary>
    private static string GetRendererPath() {
        return PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");
    }

    /// <summary>
    /// Extracts the PS Vita solid-color mesh function body so architecture assertions can target the 3D programmable path without matching unrelated 2D renderer code.
    /// </summary>
    private static string GetDrawSolidColorMeshSource() {
        string rendererSource = File.ReadAllText(GetRendererPath());
        const string methodSignature = "bool PsVitaGxmRenderer::DrawSolidColorMesh(";
        const string nextMethodSummary = "    /// Presents the current frame through the PS Vita display path.";

        int methodStart = rendererSource.IndexOf(methodSignature, StringComparison.Ordinal);
        Assert.True(methodStart >= 0, "Expected one PS Vita solid-color mesh function definition.");

        int methodEnd = rendererSource.IndexOf(nextMethodSummary, methodStart, StringComparison.Ordinal);
        Assert.True(methodEnd >= 0, "Expected one method boundary after the PS Vita solid-color mesh function.");

        return rendererSource.Substring(methodStart, methodEnd - methodStart);
    }
}
