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
        Assert.Contains("void ReleaseTexture(::RuntimeTexture* texture);", headerSource, StringComparison.Ordinal);
        Assert.Contains("void ReleaseFont(::FontAsset* font);", headerSource, StringComparison.Ordinal);
        Assert.Contains("void NotifyFramePresented();", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeTexture", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("return nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("BuildTextureFromRaw", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleaseTexture", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleaseFont", sourceCode, StringComparison.Ordinal);
        Assert.Contains("void PsVitaRenderManager2D::NotifyFramePresented()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("TextureCache.NotifyFramePresented();", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("FinalizeReleasedTexturesAfterPresent", sourceCode, StringComparison.Ordinal);
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

        Assert.Contains("::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath);", headerSource, StringComparison.Ordinal);
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
    /// Verifies the PS Vita font release path hands runtime texture wrapper destruction to the texture cache instead of deleting the wrapper immediately after queuing deferred GPU destruction.
    /// </summary>
    [Fact]
    public void Source_whenReleasingFonts_doesNotDeleteRuntimeTextureWrapperImmediately() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("ReleaseTexture(texture);", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("delete texture;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 2D renderer uses the current generated-core UI int2 header and type name shape.
    /// </summary>
    [Fact]
    public void Source_whenUsingGeneratedCoreScreenSizes_referencesCurrentGeneratedInt2TypeName() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"int2.hpp\"", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"int2.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("#include \"Int2.hpp\"", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("#include \"Int2.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const ::int2& size", headerSource, StringComparison.Ordinal);
        Assert.Contains("::int2 size = shape->get_Size();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::int2 innerSize", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("const ::Int2& size", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("::Int2 size = shape->get_Size();", sourceCode, StringComparison.Ordinal);
    }
}
