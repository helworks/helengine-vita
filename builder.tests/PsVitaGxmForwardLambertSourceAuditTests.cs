using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the native artifact-backed forward-Lambert GXM program wrapper.
/// </summary>
public sealed class PsVitaGxmForwardLambertSourceAuditTests {
    /// <summary>
    /// Verifies the vertex input binding follows reflected Vita semantics instead of compiler-dependent source identifiers.
    /// </summary>
    [Fact]
    public void Source_whenBindingVertexInputs_usesPositionAndNormalSemantics() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_POSITION, 0u)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_NORMAL, 0u)", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the Lambert wrapper loads artifacts, validates programs, patches GXM state, and resolves its uniform contract.
    /// </summary>
    [Fact]
    public void Source_whenCreatingForwardLambertProgram_usesArtifactBackedGxmPipeline() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.cpp");
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.hpp");
        string cmakeSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt"));
        string source = File.Exists(sourcePath) ? File.ReadAllText(sourcePath) : string.Empty;
        string header = File.Exists(headerPath) ? File.ReadAllText(headerPath) : string.Empty;

        Assert.True(File.Exists(sourcePath), "Expected one forward-Lambert GXM program source file.");
        Assert.True(File.Exists(headerPath), "Expected one forward-Lambert GXM program header.");
        Assert.Contains("PsVitaShaderArtifactReader", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmProgramCheck", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherRegisterProgram", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreateVertexProgram", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmShaderPatcherCreateFragmentProgram", source, StringComparison.Ordinal);
        Assert.Contains("HelengineWorldViewProjection", source, StringComparison.Ordinal);
        Assert.Contains("HelengineNormalTransform", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightDirection", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightColor", source, StringComparison.Ordinal);
        Assert.Contains("HelengineAmbient", source, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmForwardLambertProgram.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("class PsVitaGxmForwardLambertProgram", header, StringComparison.Ordinal);
        string rendererSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp"));
        Assert.Contains("DrawForwardLambertMesh", rendererSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaForwardLambertUniformBinder::Bind", rendererSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaForwardLambertVertex", rendererSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the Lambert wrapper never invokes the runtime shader compiler.
    /// </summary>
    [Fact]
    public void Source_whenCreatingForwardLambertProgram_doesNotCompileAtRuntime() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.cpp"));

        Assert.DoesNotContain("sceShaccCgCompileProgram", source, StringComparison.Ordinal);
        Assert.DoesNotContain("sceKernelLoadStartModule", source, StringComparison.Ordinal);
    }
}
