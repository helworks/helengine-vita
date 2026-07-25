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
        Assert.Contains("PlatformShaderDependency", builderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the native material reader preserves an empty Standard diffuse asset identity so the renderer can bind its white fallback texture.
    /// </summary>
    [Fact]
    public void Source_whenReadingStandardMaterialWithoutDiffuseTexture_allowsTheRendererFallbackContract() {
        string readerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaCompiledShaderMaterialReader.cpp");
        string renderManagerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string readerSource = File.ReadAllText(readerPath);
        string renderManagerSource = File.ReadAllText(renderManagerPath);

        Assert.Contains("TryReadBoolean(stream, &decodedMaterial.RequiresDiffuseTexture)", readerSource, StringComparison.Ordinal);
        Assert.Contains("TryReadString(stream, decodedMaterial.DiffuseTextureAssetId)", readerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("Textured PS Vita compiled-shader materials require one diffuse texture asset id.", readerSource, StringComparison.Ordinal);
        Assert.Contains("compiledShaderMaterial.RequiresDiffuseTexture && !compiledShaderMaterial.DiffuseTextureAssetId.empty()", renderManagerSource, StringComparison.Ordinal);
    }
}
