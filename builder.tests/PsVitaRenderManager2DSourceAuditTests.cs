using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita 2D renderer source so texture-backed menu assets can materialize into platform runtime textures.
/// </summary>
public sealed class PsVitaRenderManager2DSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita 2D renderer builds and releases concrete Vita runtime textures instead of returning null placeholders.
    /// </summary>
    [Fact]
    public void Source_whenBuildingTextures_createsConcreteVitaRuntimeTextures() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string runtimeTextureHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeTexture.hpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);
        string runtimeTextureHeaderSource = File.ReadAllText(runtimeTextureHeaderPath);

        Assert.Contains("class PsVitaRuntimeTexture final : public RuntimeTexture", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("void ReleaseTexture(::RuntimeTexture* texture) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("void ReleaseFont(::FontAsset* font) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeTexture", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("return nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("BuildTextureFromRaw", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleaseTexture", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleaseFont", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 2D renderer can rebuild one cooked texture payload by deserializing the packaged texture asset and forwarding it through the raw Vita texture path.
    /// </summary>
    [Fact]
    public void Source_whenResolvingCookedPlatformOwnedTexture_reusesRawTextureBuilderPath() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"AssetSerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"Asset.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/native_cast.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"system/io/file.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::FileStream* stream = nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::Asset* asset = nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("stream = ::File::OpenRead(cookedAssetPath);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("asset = ::AssetSerializer::Deserialize(stream);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::TextureAsset* cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::RuntimeTexture* runtimeTexture = BuildTextureFromRaw(cookedTextureAsset);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 2D renderer uses the generated-core global int2 type instead of an unqualified namespace-local lookup.
    /// </summary>
    [Fact]
    public void Source_whenUsingGeneratedCoreScreenSizes_referencesGlobalGeneratedInt2TypeName() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"helengine_int2.hpp\"", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"helengine_int2.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("#include \"int2.hpp\"", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("#include \"int2.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const ::int2& size", headerSource, StringComparison.Ordinal);
        Assert.Contains("::int2 size = shape->get_Size();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::int2 innerSize", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::int2 spriteSize = sprite->get_Size();", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain(" const int2& size", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain(" int2 size = shape->get_Size();", sourceCode, StringComparison.Ordinal);
    }
}
