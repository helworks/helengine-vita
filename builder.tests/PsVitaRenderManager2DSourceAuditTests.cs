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

    /// <summary>
    /// Ensures 2D sprites sample only their authored texels when Vita stores them in larger padded native allocations.
    /// </summary>
    [Fact]
    public void Source_whenDrawingSprites_excludesPaddedTextureTexelsFromLogicalUvs() {
        string queuedQuadPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaQueuedQuad.hpp");
        string renderManagerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string rendererPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");
        string queuedQuadSource = File.ReadAllText(queuedQuadPath);
        string renderManagerSource = File.ReadAllText(renderManagerPath);
        string rendererSource = File.ReadAllText(rendererPath);
        int drawSpriteStart = renderManagerSource.IndexOf("void PsVitaRenderManager2D::DrawSprite", StringComparison.Ordinal);
        int drawTextStart = renderManagerSource.IndexOf("void PsVitaRenderManager2D::DrawText", StringComparison.Ordinal);

        Assert.True(drawSpriteStart >= 0, "Expected the Vita DrawSprite implementation.");
        Assert.True(drawTextStart > drawSpriteStart, "Expected DrawText to follow DrawSprite in the Vita 2D renderer.");
        string drawSpriteSource = renderManagerSource[drawSpriteStart..drawTextStart];
        string drawTextSource = renderManagerSource[drawTextStart..];

        Assert.Contains("bool UsesLogicalTextureExtents = false;", queuedQuadSource, StringComparison.Ordinal);
        Assert.Contains("queuedQuad.UsesLogicalTextureExtents = true;", drawSpriteSource, StringComparison.Ordinal);
        Assert.Contains("queuedQuad.UsesLogicalTextureExtents = true;", drawTextSource, StringComparison.Ordinal);
        Assert.Contains("queuedQuad.UsesLogicalTextureExtents", rendererSource, StringComparison.Ordinal);
        Assert.Contains("textureUScale = static_cast<float>(queuedQuad.Texture->GetTextureWidthPixels())", rendererSource, StringComparison.Ordinal);
        Assert.Contains("/ static_cast<float>(gpuTexture->GetWidth());", rendererSource, StringComparison.Ordinal);
        Assert.Contains("textureVScale = static_cast<float>(queuedQuad.Texture->GetTextureHeightPixels())", rendererSource, StringComparison.Ordinal);
        Assert.Contains("/ static_cast<float>(gpuTexture->GetHeight());", rendererSource, StringComparison.Ordinal);
        Assert.Contains("triangleVertices[0].u = queuedQuad.Vertices[0].TextureU * textureUScale;", rendererSource, StringComparison.Ordinal);
        Assert.Contains("triangleVertices[0].v = queuedQuad.Vertices[0].TextureV * textureVScale;", rendererSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Vita 2D visitor rejects drawables whose entity or ancestor is disabled before invoking their draw callback.
    /// </summary>
    [Fact]
    public void Source_whenVisitingDrawables_requiresHierarchyEnabledEntity() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("if (parent == nullptr || !parent->get_IsHierarchyEnabled()) {", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("if (parent == nullptr || !parent->get_Enabled()) {", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita text renderer emits each authored shadow and outline pass before the foreground glyph.
    /// </summary>
    [Fact]
    public void Source_whenDrawingText_emitsShadowOutlineAndForegroundGlyphPasses() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("get_ShadowOffset", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_ShadowColor", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_OutlineScale", sourceCode, StringComparison.Ordinal);
        Assert.Contains("get_OutlineColor", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float2(-outlineScale, 0.0f)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float2(outlineScale, 0.0f)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float2(0.0f, -outlineScale)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float2(0.0f, outlineScale)", sourceCode, StringComparison.Ordinal);
    }
}
