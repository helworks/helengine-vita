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
    /// The temporary runtime-compile bridge may still borrow the active vita2d-owned GXM context until the renderer owns that state directly.
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

        Assert.DoesNotContain("_vita2d_colorVertexProgram", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorFragmentProgram", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_colorWvpParam", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("vita2d_draw_array", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita solid-color mesh path compiles the shader at runtime and binds real GXM programs instead of keeping the old no-op placeholder body.
    /// </summary>
    [Fact]
    public void Source_whenSolidColorMeshShaderPipelineCompilesAtRuntime_usesShaccCgAndRealGxmBinding() {
        string rendererSource = File.ReadAllText(GetRendererPath());
        string wrapperSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.cpp"));
        string drawSolidColorMeshSource = GetDrawSolidColorMeshSource();

        Assert.Contains("#include <psp2/shacccg.h>", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceKernelLoadStartModule", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgInitializeCompileOptions", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgCompileProgram", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgDestroyCompileOutput", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreate", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreateVertexProgram", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreateFragmentProgram", wrapperSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmProgramFindParameterByName", wrapperSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)worldViewProjection;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)positions;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)positionCount;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)indices;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)indexCount;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.DoesNotContain("(void)colorAbgr;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmSetVertexProgram", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmSetFragmentProgram", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmSetVertexStream", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("sceGxmDraw", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("return true;", drawSolidColorMeshSource, StringComparison.Ordinal);
        Assert.Contains("UploadSolidColorWorldViewProjection", rendererSource, StringComparison.Ordinal);
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
