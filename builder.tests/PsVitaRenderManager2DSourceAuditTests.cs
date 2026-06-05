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
}
