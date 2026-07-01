using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita texture cache source so scene transitions release detached GPU textures only after the host reaches its explicit frame-boundary safe point.
/// </summary>
public sealed class PsVitaTextureCacheSourceAuditTests {
    /// <summary>
    /// Verifies runtime texture release detaches uploaded Vita GPU textures into a deferred queue that is drained only when the renderer reaches the explicit flush boundary.
    /// </summary>
    [Fact]
    public void Source_whenReleasingRuntimeTextures_defersGpuTextureDeletionUntilExplicitFlushBoundary() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include <memory>", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include <vector>", headerSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<ReleasedGpuTextureEntry> ReleasedGpuTextures;", headerSource, StringComparison.Ordinal);
        Assert.Contains("void NotifyFramePresented();", headerSource, StringComparison.Ordinal);
        Assert.Contains("bool PresentedFramePendingFlush = false;", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include <vita2d.h>", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/psvita/rendering/PsVitaGpuTexture.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("std::unique_ptr<PsVitaGpuTexture> releasedGpuTexture = texture->ReleaseGpuTexture();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleasedGpuTextures.push_back(std::move(releasedEntry));", sourceCode, StringComparison.Ordinal);
        Assert.Contains("void PsVitaTextureCache::NotifyFramePresented()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (ReleasedGpuTextures.empty())", sourceCode, StringComparison.Ordinal);
        Assert.Contains("void PsVitaTextureCache::FlushReleasedTextures()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (!PresentedFramePendingFlush)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("vita2d_wait_rendering_done();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceGxmDisplayQueueFinish();", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaTextureCache::FinalizeReleasedTexturesAfterPresent()", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita texture cache retains runtime texture wrapper ownership until the same explicit flush boundary so scene teardown cannot observe freed wrappers before the host drains deferred releases.
    /// </summary>
    [Fact]
    public void Source_whenDeferringGpuTextureDeletion_alsoDefersRuntimeTextureWrapperDeletion() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("struct ReleasedGpuTextureEntry", headerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeTexture* RuntimeTexture = nullptr;", headerSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<ReleasedGpuTextureEntry> ReleasedGpuTextures;", headerSource, StringComparison.Ordinal);
        Assert.Contains("std::uint32_t FlushBoundariesRemaining = 3u;", headerSource, StringComparison.Ordinal);
        Assert.Contains("ReleasedGpuTextureEntry releasedEntry{};", sourceCode, StringComparison.Ordinal);
        Assert.Contains("releasedEntry.RuntimeTexture = texture;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (releasedEntry.FlushBoundariesRemaining > 0u)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleasedGpuTextures.push_back(std::move(releasedEntry));", sourceCode, StringComparison.Ordinal);
        Assert.Contains("AppendTextureTrace(\"TextureCache: FlushReleasedTextures gpu phase complete\")", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (releasedEntry.RuntimeTexture == nullptr)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("releasedEntry.RuntimeTexture->Dispose();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete releasedEntry.RuntimeTexture;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita texture cache copies the authored runtime asset id onto each runtime texture wrapper so repeated cooked atlas loads preserve deterministic identity.
    /// </summary>
    [Fact]
    public void Source_whenCreatingRuntimeTexture_copiesRuntimeAssetIdFromAuthoredTextureAsset() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("PsVitaRuntimeTexture* runtimeTexture = new PsVitaRuntimeTexture();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("runtimeTexture->set_Id(data->get_Id());", sourceCode, StringComparison.Ordinal);
        Assert.Contains("runtimeTexture->SetRuntimeAssetId(data->get_RuntimeAssetId());", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita texture cache reuses runtime textures by deterministic runtime asset id so repeated cooked atlas loads do not continuously recreate native GPU allocations.
    /// </summary>
    [Fact]
    public void Source_whenBuildingRuntimeTexture_reusesCachedTextureForNonZeroRuntimeAssetId() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("std::unordered_map<std::uint64_t, PsVitaRuntimeTexture*> CachedTextures;", headerSource, StringComparison.Ordinal);
        Assert.Contains("const std::uint64_t cacheKey = data->get_RuntimeAssetId();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (cacheKey != 0u)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("auto cachedTextureIterator = CachedTextures.find(cacheKey);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (cachedTextureIterator != CachedTextures.end())", sourceCode, StringComparison.Ordinal);
        Assert.Contains("return cachedTextureIterator->second;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("CachedTextures.emplace(cacheKey, runtimeTexture);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita texture cache retains cached runtime textures by deterministic runtime asset id so scene reloads can reuse the same native Vita allocation instead of freeing and rebuilding it.
    /// </summary>
    [Fact]
    public void Source_whenReleasingRuntimeTexture_keepsMatchingCachedEntryByRuntimeAssetIdResident() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("const std::uint64_t cacheKey = texture->GetRuntimeAssetId();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (cacheKey != 0u)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("auto cachedTextureIterator = CachedTextures.find(cacheKey);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (cachedTextureIterator != CachedTextures.end() && cachedTextureIterator->second == texture)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("TextureCache: ReleaseTexture retained cached runtimeTexture=", sourceCode, StringComparison.Ordinal);
        Assert.Contains("return;", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("CachedTextures.erase(cachedTextureIterator);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita texture cache drains every deferred release from the explicit flush boundary instead of trying to stagger native frees across future present calls.
    /// </summary>
    [Fact]
    public void Source_whenFlushingReleasedTextures_drainsTheDeferredQueueImmediatelyAtTheBoundary() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTextureCache.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("std::vector<ReleasedGpuTextureEntry> releasedTextures = std::move(ReleasedGpuTextures);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("std::vector<ReleasedGpuTextureEntry> readyReleasedTextures;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ReleasedGpuTextures.clear();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("for (std::size_t releasedTextureIndex = 0; releasedTextureIndex < releasedTextures.size(); releasedTextureIndex++)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (readyReleasedTextures.empty())", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("PresentFramesRemaining", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("finalizedTextureCount", sourceCode, StringComparison.Ordinal);
    }
}
