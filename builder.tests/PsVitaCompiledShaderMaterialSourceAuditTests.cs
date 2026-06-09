using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita builder source so shared shader-backed materials are rewritten into one Vita-owned cooked material payload.
/// </summary>
public sealed class PsVitaCompiledShaderMaterialSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita builder owns dedicated compiled-shader material files and routes shader-backed requests through them.
    /// </summary>
    [Fact]
    public void Source_whenCookingShaderBackedMaterials_containsDedicatedCompiledShaderMaterialPayloadPath() {
        string assetPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaCompiledShaderMaterialAsset.cs");
        string serializerPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaCompiledShaderMaterialBinarySerializer.cs");
        string builderPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaPlatformAssetBuilder.cs");

        string builderSource = File.ReadAllText(builderPath);

        Assert.True(File.Exists(assetPath), "Expected one PS Vita compiled-shader material asset file.");
        Assert.True(File.Exists(serializerPath), "Expected one PS Vita compiled-shader material serializer file.");
        Assert.Contains("const string ShaderAssetIdFieldId = \"shader-asset-id\";", builderSource, StringComparison.Ordinal);
        Assert.Contains("const string VertexProgramFieldId = \"vertex-program\";", builderSource, StringComparison.Ordinal);
        Assert.Contains("const string PixelProgramFieldId = \"pixel-program\";", builderSource, StringComparison.Ordinal);
        Assert.Contains("const string VariantFieldId = \"variant\";", builderSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaCompiledShaderMaterialAsset cookedAsset = new()", builderSource, StringComparison.Ordinal);
        Assert.Contains("new PsVitaCompiledShaderMaterialBinarySerializer().Serialize(cookedAsset)", builderSource, StringComparison.Ordinal);
        Assert.Contains("[shaderAssetId]", builderSource, StringComparison.Ordinal);
    }
}
