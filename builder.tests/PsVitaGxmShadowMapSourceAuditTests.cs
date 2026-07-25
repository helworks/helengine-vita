using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the native shadow-map foundation so it remains inside Vita2D's supported render-target lifecycle.
/// </summary>
public sealed class PsVitaGxmShadowMapSourceAuditTests {
    /// <summary>
    /// Verifies the shadow target is allocated and activated through Vita2D without taking raw GXM scene ownership.
    /// </summary>
    [Fact]
    public void Source_whenCreatingShadowMap_usesVita2dOffscreenTargetAndPublicContext() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmShadowMap.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmShadowMap.cpp");
        string forwardProgramPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.cpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");

        string source = File.ReadAllText(sourcePath);
        string forwardProgramSource = File.ReadAllText(forwardProgramPath);
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.True(File.Exists(headerPath), "Expected one PS Vita Vita2D-owned shadow-map header.");
        Assert.Contains("vita2d_create_empty_texture_rendertarget", source, StringComparison.Ordinal);
        Assert.Contains("vita2d_start_drawing_advanced", source, StringComparison.Ordinal);
        Assert.Contains("vita2d_end_drawing", source, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGxmBeginScene", source, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGxmEndScene", source, StringComparison.Ordinal);
        Assert.Contains("vita2d_get_context", forwardProgramSource, StringComparison.Ordinal);
        Assert.DoesNotContain("_vita2d_context", forwardProgramSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaGxmShadowMap.cpp", cmakeSource, StringComparison.Ordinal);
    }
}
