using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the cooked material contract needed by the textured Forward Standard Shader profile.
/// </summary>
public sealed class PsVitaCompiledShaderMaterialTextureContractTests {
    /// <summary>
    /// Ensures the serialized material preserves the required diffuse texture identity and flag.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_WhenDiffuseTextureIsRequired_PreservesTheTextureContract() {
        PsVitaCompiledShaderMaterialAsset asset = new() {
            ShaderAssetId = "ForwardStandardShader",
            VertexProgramName = "ForwardStandardShader.vs",
            PixelProgramName = "ForwardStandardShader.ps",
            VariantName = "ForwardStandardTextured",
            ParameterContractVersion = 1u,
            BaseColorAbgr = 0xFFFFFFFFu,
            RequiresDiffuseTexture = true,
            DiffuseTextureAssetId = "grid-texture"
        };

        PsVitaCompiledShaderMaterialAsset decoded = new PsVitaCompiledShaderMaterialBinarySerializer().Deserialize(
            new PsVitaCompiledShaderMaterialBinarySerializer().Serialize(asset));

        Assert.True(decoded.RequiresDiffuseTexture);
        Assert.Equal("grid-texture", decoded.DiffuseTextureAssetId);
    }
}
