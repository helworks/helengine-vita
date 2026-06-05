using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita renderer sources so the first native 2D pass is built around a concrete GXM pipeline instead of empty sprite and text hooks.
/// </summary>
public sealed class PsVitaGxmRenderPipelineSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita runtime references the planned GXM renderer files, flush entrypoints, and build wiring required for sprite and text rendering.
    /// </summary>
    [Fact]
    public void Source_whenBuildingTheGxm2dPipeline_containsRendererFilesAndFlushHooks() {
        string repositoryRootPath = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT");
        if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        }

        string repositoryMarkerPath = Path.Combine("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        DirectoryInfo candidateDirectory = new DirectoryInfo(repositoryRootPath);
        while (candidateDirectory != null && !File.Exists(Path.Combine(candidateDirectory.FullName, repositoryMarkerPath))) {
            candidateDirectory = candidateDirectory.Parent;
        }

        if (candidateDirectory == null) {
            candidateDirectory = new DirectoryInfo(Directory.GetCurrentDirectory());
            while (candidateDirectory != null && !File.Exists(Path.Combine(candidateDirectory.FullName, repositoryMarkerPath))) {
                candidateDirectory = candidateDirectory.Parent;
            }
        }

        if (candidateDirectory == null) {
            throw new DirectoryNotFoundException("Could not locate the helengine-psvita repository root from the test process.");
        }

        repositoryRootPath = candidateDirectory.FullName;
        string gxmRendererHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.hpp");
        string gpuTextureHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaGpuTexture.hpp");
        string queuedQuadHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaQueuedQuad.hpp");
        string texturedVertexHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaTexturedQuadVertex.hpp");
        string renderManagerHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        string renderManagerSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.cpp");
        string bootHostSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "psvita", "PsVitaBootHost.cpp");
        string cmakePath = Path.Combine(repositoryRootPath, "CMakeLists.txt");

        string renderManagerHeaderSource = File.ReadAllText(renderManagerHeaderPath);
        string renderManagerSource = File.ReadAllText(renderManagerSourcePath);
        string bootHostSource = File.ReadAllText(bootHostSourcePath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.True(File.Exists(gxmRendererHeaderPath), "Expected one PS Vita GXM renderer header.");
        Assert.True(File.Exists(gpuTextureHeaderPath), "Expected one PS Vita GPU texture header.");
        Assert.True(File.Exists(queuedQuadHeaderPath), "Expected one queued quad command header.");
        Assert.True(File.Exists(texturedVertexHeaderPath), "Expected one textured quad vertex header.");
        Assert.Contains("void Draw() override;", renderManagerHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {\r\n    }", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {\n    }", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawText(::ITextDrawable2D* text) {\r\n    }", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("void PsVitaRenderManager2D::DrawText(::ITextDrawable2D* text) {\n    }", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmRenderer", bootHostSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmRenderer.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGpuTexture.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRenderManager2D.cpp", cmakeSource, StringComparison.Ordinal);
    }
}
