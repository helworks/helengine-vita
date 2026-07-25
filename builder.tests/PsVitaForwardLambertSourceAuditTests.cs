using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the GPU forward-Lambert shader contract and its explicit material variant.
/// </summary>
public sealed class PsVitaForwardLambertSourceAuditTests {
    /// <summary>
    /// Verifies that the shared shader source declares the GPU lighting inputs and Lambert equation.
    /// </summary>
    [Fact]
    public void Source_whenDefiningForwardLambertShader_containsGpuLightingContract() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "ForwardLambertShaderSource.cpp");
        string source = File.Exists(sourcePath) ? File.ReadAllText(sourcePath) : string.Empty;

        Assert.True(File.Exists(sourcePath), "Expected one forward-Lambert shader source file.");
        Assert.Contains("HelengineWorldViewProjection", source, StringComparison.Ordinal);
        Assert.Contains("HelengineNormalTransform", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightDirection", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightColor", source, StringComparison.Ordinal);
        Assert.Contains("HelengineAmbient", source, StringComparison.Ordinal);
        Assert.Contains("max(dot", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the source uses Vita's Cg entry-point and semantic conventions rather than desktop HLSL-only syntax.
    /// </summary>
    [Fact]
    public void Source_whenCompilingThroughShaccCg_usesCgSyntax() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "ForwardLambertShaderSource.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("uniform float4x4", source, StringComparison.Ordinal);
        Assert.Contains(") : COLOR", source, StringComparison.Ordinal);
        Assert.DoesNotContain("cbuffer", source, StringComparison.Ordinal);
        Assert.DoesNotContain("SV_POSITION", source, StringComparison.Ordinal);
        Assert.DoesNotContain("SV_TARGET", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the builder names the production GPU Lambert variant explicitly.
    /// </summary>
    [Fact]
    public void Source_whenCookingForwardLambertMaterial_containsExplicitVariantName() {
        string builderSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaPlatformAssetBuilder.cs"));
        string materialSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaCompiledShaderMaterialAsset.cs"));

        Assert.Contains("ForwardLambertOpaque", builderSource, StringComparison.Ordinal);
        Assert.Contains("ParameterContractVersion", materialSource, StringComparison.Ordinal);
        Assert.DoesNotContain("VertexArtifactHash", materialSource, StringComparison.Ordinal);
        Assert.DoesNotContain("FragmentArtifactHash", materialSource, StringComparison.Ordinal);
    }
}
