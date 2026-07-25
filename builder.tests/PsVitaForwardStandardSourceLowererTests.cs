using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the first Vita lowering profile for the shared Forward Standard Shader.
/// </summary>
public sealed class PsVitaForwardStandardSourceLowererTests {
    /// <summary>
    /// Ensures the vertex lowering preserves the native row-vector world-view-projection convention.
    /// </summary>
    [Fact]
    public void LowerVertex_WhenForwardStandardIsRequested_MultipliesPositionByWorldViewProjectionOnTheRight() {
        string source = new PsVitaForwardStandardSourceLowerer().LowerVertex();

        Assert.Contains("mul(float4(input.position, 1.0), HelengineWorldViewProjection)", source, StringComparison.Ordinal);
        Assert.DoesNotContain("mul(HelengineWorldViewProjection, float4(input.position, 1.0))", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the vertex lowering transforms normals with the native row-vector convention.
    /// </summary>
    [Fact]
    public void LowerVertex_WhenForwardStandardIsRequested_MultipliesNormalByNormalTransformOnTheRight() {
        string source = new PsVitaForwardStandardSourceLowerer().LowerVertex();

        Assert.Contains("mul(input.normal, (float3x3)HelengineNormalTransform)", source, StringComparison.Ordinal);
        Assert.DoesNotContain("mul((float3x3)HelengineNormalTransform, input.normal)", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the fragment lowering uses the expected diffuse sampler and Lambert uniform contract.
    /// </summary>
    [Fact]
    public void LowerFragment_WhenForwardStandardIsRequested_EmitsDiffuseTextureLambertCg() {
        string source = new PsVitaForwardStandardSourceLowerer().LowerFragment();

        Assert.Contains("sampler2D HelengineDiffuseTexture", source, StringComparison.Ordinal);
        Assert.Contains("tex2D(HelengineDiffuseTexture", source, StringComparison.Ordinal);
        Assert.Contains("HelengineLightDirection", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the shadow-receiving Standard Shader vertex contract keeps the normal and texture streams while supplying light-space coordinates.
    /// </summary>
    [Fact]
    public void LowerShadowedVertex_WhenForwardStandardIsRequested_EmitsLightViewProjectionCoordinates() {
        string source = new PsVitaForwardStandardSourceLowerer().LowerShadowedVertex();

        Assert.Contains("float4x4 HelengineLightViewProjection", source, StringComparison.Ordinal);
        Assert.Contains("float4 shadowPosition : TEXCOORD2", source, StringComparison.Ordinal);
        Assert.Contains("mul(float4(input.position, 1.0), HelengineLightViewProjection)", source, StringComparison.Ordinal);
        Assert.Contains("mul(input.normal, (float3x3)HelengineNormalTransform)", source, StringComparison.Ordinal);
        Assert.Contains("float2 texCoord : TEXCOORD1", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the shadow-receiving Standard Shader fragment contract samples both authored diffuse color and the renderer-owned shadow map.
    /// </summary>
    [Fact]
    public void LowerShadowedFragment_WhenForwardStandardIsRequested_EmitsHardShadowSamplingContract() {
        string source = new PsVitaForwardStandardSourceLowerer().LowerShadowedFragment();

        Assert.Contains("sampler2D HelengineDiffuseTexture", source, StringComparison.Ordinal);
        Assert.Contains("sampler2D HelengineShadowTexture", source, StringComparison.Ordinal);
        Assert.Contains("float4 HelengineShadowBias", source, StringComparison.Ordinal);
        Assert.Contains("float2 packedShadowDepth", source, StringComparison.Ordinal);
        Assert.Contains("dot(packedShadowDepth, float2(1.0, 1.0 / 255.0))", source, StringComparison.Ordinal);
        Assert.Contains("(1.0 - shadowCoordinate.y) * 0.5", source, StringComparison.Ordinal);
        Assert.Contains("shadowCoordinate.x >= -1.0", source, StringComparison.Ordinal);
        Assert.Contains("float directLightVisibility = shadowMapContainsCoordinate > 0.5", source, StringComparison.Ordinal);
        Assert.Contains("HelengineAmbient", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the depth-only caster contract is independent from authored material texture and lighting inputs.
    /// </summary>
    [Fact]
    public void LowerShadowDepthStages_WhenForwardStandardIsRequested_EmitOnlyLightSpacePosition() {
        PsVitaForwardStandardSourceLowerer lowerer = new();
        string vertexSource = lowerer.LowerShadowDepthVertex();
        string fragmentSource = lowerer.LowerShadowDepthFragment();

        Assert.Contains("float4x4 HelengineLightViewProjection", vertexSource, StringComparison.Ordinal);
        Assert.Contains("mul(float4(input.position, 1.0), HelengineLightViewProjection)", vertexSource, StringComparison.Ordinal);
        Assert.Contains("output.depth = output.position.z / max(output.position.w, 0.0001)", vertexSource, StringComparison.Ordinal);
        Assert.Contains("float2 packedDepth = frac(input.depth * float2(1.0, 255.0));", fragmentSource, StringComparison.Ordinal);
        Assert.Contains("packedDepth.x -= packedDepth.y * (1.0 / 255.0);", fragmentSource, StringComparison.Ordinal);
        Assert.Contains("return float4(packedDepth, 0.0, 1.0);", fragmentSource, StringComparison.Ordinal);
        Assert.DoesNotContain("HelengineDiffuseTexture", vertexSource, StringComparison.Ordinal);
        Assert.DoesNotContain("HelengineDiffuseTexture", fragmentSource, StringComparison.Ordinal);
        Assert.DoesNotContain("HelengineLightDirection", fragmentSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the Vita lowering consumes the shared Standard Shader depth variant instead of choosing a Vita-only variant identity.
    /// </summary>
    [Fact]
    public void Lower_WhenSharedShadowDepthVariantIsRequested_EmitsTheDepthOnlyCgContract() {
        StandardShaderVariant variant = Assert.Single(StandardShaderVariants.All, candidate => candidate.Name == "ShadowDepth");

        PsVitaForwardStandardSourcePair sourcePair = new PsVitaForwardStandardSourceLowerer().Lower(variant);

        Assert.Contains("float4x4 HelengineLightViewProjection", sourcePair.VertexSourceText, StringComparison.Ordinal);
        Assert.DoesNotContain("HelengineDiffuseTexture", sourcePair.FragmentSourceText, StringComparison.Ordinal);
    }
}
