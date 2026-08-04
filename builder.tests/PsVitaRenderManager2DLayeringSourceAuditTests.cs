using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits PS Vita 2D command flushing so textured text and solid UI geometry share one render-order sequence.
/// </summary>
public sealed class PsVitaRenderManager2DLayeringSourceAuditTests {
    /// <summary>
    /// Verifies every render-order layer flushes its solid and textured primitives before the next layer begins.
    /// </summary>
    [Fact]
    public void Source_whenFlushing2DCommands_submitsSolidAndTexturedPrimitivesBySharedRenderOrder() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string source = File.ReadAllText(sourcePath);

        Assert.Contains("while (solidColorIndex < QueuedSolidColorTriangles.size() || quadIndex < QueuedQuads.size())", source, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer->SubmitSolidColorTriangles(solidColorLayer);", source, StringComparison.Ordinal);
        Assert.Contains("GxmRenderer->SubmitQuads(quadLayer);", source, StringComparison.Ordinal);
        Assert.DoesNotContain("GxmRenderer->SubmitSolidColorTriangles(QueuedSolidColorTriangles);", source, StringComparison.Ordinal);
        Assert.DoesNotContain("GxmRenderer->SubmitQuads(QueuedQuads);", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies overlay cameras retain priority over every local drawable layer from earlier cameras.
    /// </summary>
    [Fact]
    public void Source_whenQueuingOverlayCameraDrawables_composesCameraAndLocalRenderOrders() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string quadPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaQueuedQuad.hpp");
        string solidVertexPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaSolidColorVertex.hpp");

        Assert.Contains("std::uint16_t ActiveCameraRenderOrder", File.ReadAllText(headerPath), StringComparison.Ordinal);
        Assert.Contains("ActiveCameraRenderOrder = camera->get_CameraDrawOrder()", File.ReadAllText(sourcePath), StringComparison.Ordinal);
        Assert.Contains("ComposeRenderOrder(ActiveCameraRenderOrder, renderOrder)", File.ReadAllText(sourcePath), StringComparison.Ordinal);
        Assert.Contains("std::uint16_t RenderOrder", File.ReadAllText(quadPath), StringComparison.Ordinal);
        Assert.Contains("std::uint16_t RenderOrder", File.ReadAllText(solidVertexPath), StringComparison.Ordinal);
    }
}
