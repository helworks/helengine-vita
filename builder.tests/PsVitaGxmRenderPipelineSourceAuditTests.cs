using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita renderer sources so the first native 2D pass is built around a concrete GXM pipeline instead of empty sprite and text hooks.
/// </summary>
public sealed class PsVitaGxmRenderPipelineSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita runtime references the planned GXM renderer files, frame flush entrypoints, and build wiring required for the native 2D foundation.
    /// </summary>
    [Fact]
    public void Source_whenBuildingTheGxm2dFoundation_containsRendererFilesAndFrameWiring() {
        string gxmRendererHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.hpp");
        string gpuTextureHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGpuTexture.hpp");
        string queuedQuadHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaQueuedQuad.hpp");
        string texturedVertexHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaTexturedQuadVertex.hpp");
        string renderManagerHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string bootHostSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");

        string renderManagerHeaderSource = File.ReadAllText(renderManagerHeaderPath);
        string bootHostSource = File.ReadAllText(bootHostSourcePath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.True(File.Exists(gxmRendererHeaderPath), "Expected one PS Vita GXM renderer header.");
        Assert.True(File.Exists(gpuTextureHeaderPath), "Expected one PS Vita GPU texture header.");
        Assert.True(File.Exists(queuedQuadHeaderPath), "Expected one queued quad command header.");
        Assert.True(File.Exists(texturedVertexHeaderPath), "Expected one textured quad vertex header.");
        Assert.Contains("void Draw() override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmRenderer", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmRenderer.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGpuTexture.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRenderManager2D.cpp", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita runtime texture type exposes dimensions and native GPU ownership hooks needed for lazy GXM texture upload.
    /// </summary>
    [Fact]
    public void Source_whenBuildingRuntimeTextureResidency_exposesNativeTextureOwnershipHooks() {
        string runtimeTextureHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeTexture.hpp");
        string gxmRendererSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");

        string runtimeTextureHeaderSource = File.ReadAllText(runtimeTextureHeaderPath);
        string gxmRendererSource = File.ReadAllText(gxmRendererSourcePath);

        Assert.Contains("std::uint32_t GetTextureWidthPixels() const;", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::uint32_t GetTextureHeightPixels() const;", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGpuTexture* GetGpuTexture() const;", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("void SetGpuTexture(std::unique_ptr<PsVitaGpuTexture>&& gpuTexture);", runtimeTextureHeaderSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGpuTexture", gxmRendererSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 2D renderer stores queued sprite quads and flushes them through the native GXM renderer.
    /// </summary>
    [Fact]
    public void Source_whenQueueingSpriteQuads_usesTheQueuedQuadPathAndFlushesThroughGxm() {
        string renderManagerHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string renderManagerSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");

        string renderManagerHeaderSource = File.ReadAllText(renderManagerHeaderPath);
        string renderManagerSource = File.ReadAllText(renderManagerSourcePath);

        Assert.Contains("void SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer);", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("std::vector<rendering::PsVitaQueuedQuad> QueuedQuads;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.Contains("rendering::PsVitaQueuedQuad", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("SubmitQuads", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {\r\n    }", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {\n    }", renderManagerSource, StringComparison.Ordinal);
    }
}
