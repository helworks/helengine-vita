using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita directional-shadow integration across material cooking and native mesh submission.
/// </summary>
public sealed class PsVitaDirectionalShadowSchedulingSourceAuditTests {
    /// <summary>
    /// Verifies Standard Shader materials preserve cast and receive flags and the native renderer schedules a caster pass before shadowed receiver draws.
    /// </summary>
    [Fact]
    public void Source_whenDirectionalShadowsAreEnabled_preservesMaterialFlagsAndSchedulesBothPasses() {
        string cookedMaterialPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaCompiledShaderMaterialAsset.cs");
        string materialBuilderPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaPlatformAssetBuilder.cs");
        string renderManagerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string rendererPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp");
        string forwardProgramPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmForwardLambertProgram.cpp");
        string bootHostPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaBootHost.cpp");

        string cookedMaterialSource = File.ReadAllText(cookedMaterialPath);
        string materialBuilderSource = File.ReadAllText(materialBuilderPath);
        string renderManagerSource = File.ReadAllText(renderManagerPath);
        string rendererSource = File.ReadAllText(rendererPath);
        string forwardProgramSource = File.ReadAllText(forwardProgramPath);
        string bootHostSource = File.ReadAllText(bootHostPath);

        Assert.Contains("CastsShadows", cookedMaterialSource, StringComparison.Ordinal);
        Assert.Contains("ReceivesShadows", cookedMaterialSource, StringComparison.Ordinal);
        Assert.Contains("CastsShadows =", materialBuilderSource, StringComparison.Ordinal);
        Assert.Contains("ReceivesShadows =", materialBuilderSource, StringComparison.Ordinal);
        Assert.Contains("BeginShadowDepthPass", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("DrawShadowDepthMesh", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("EndShadowDepthPass", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("ForwardStandardShadowed", renderManagerSource, StringComparison.Ordinal);
        Assert.Contains("GetShadowTextureParameter", rendererSource, StringComparison.Ordinal);
        Assert.Contains("GetLightViewProjectionParameter", rendererSource, StringComparison.Ordinal);
        Assert.Contains("HelengineShadowTexture", forwardProgramSource, StringComparison.Ordinal);
        Assert.Contains("HelengineLightViewProjection", forwardProgramSource, StringComparison.Ordinal);
        Assert.Contains("void PsVitaRenderManager3D::PrepareShadowMaps()", renderManagerSource, StringComparison.Ordinal);
        int shadowDepthProgramIndex = rendererSource.IndexOf("sceGxmSetVertexProgram(context, ShadowDepthProgram.GetVertexProgram())", StringComparison.Ordinal);
        int shadowDepthUniformIndex = rendererSource.IndexOf("sceGxmReserveVertexDefaultUniformBuffer(context, &vertexUniformBuffer)", StringComparison.Ordinal);
        Assert.True(shadowDepthProgramIndex >= 0 && shadowDepthProgramIndex < shadowDepthUniformIndex, "The ShadowDepth program must be active before its default uniform buffer is reserved.");
        int standardDrawStart = rendererSource.IndexOf("bool PsVitaGxmRenderer::DrawForwardStandardMesh(", StringComparison.Ordinal);
        int standardDrawEnd = rendererSource.IndexOf("vita2d_texture* PsVitaGxmRenderer::GetOrCreateStandardWhiteTexture()", StringComparison.Ordinal);
        string standardDrawSource = rendererSource.Substring(standardDrawStart, standardDrawEnd - standardDrawStart);
        Assert.DoesNotContain("sceGxmReserveVertexDefaultUniformBuffer(context, &vertexUniformBuffer)", standardDrawSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGxmReserveFragmentDefaultUniformBuffer(context, &fragmentUniformBuffer)", standardDrawSource, StringComparison.Ordinal);
        Assert.DoesNotContain("|| !compiledMaterial->GetRequiresDiffuseTexture()", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("compiledMaterial->GetShaderAssetId() != \"ForwardStandardShader\"", renderManagerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawShadowMapDiagnosticOverlay", rendererSource, StringComparison.Ordinal);
        Assert.DoesNotContain("DrawShadowMapDiagnosticOverlay", bootHostSource, StringComparison.Ordinal);
        int shadowDepthDrawStart = rendererSource.IndexOf("bool PsVitaGxmRenderer::DrawShadowDepthMesh(", StringComparison.Ordinal);
        int shadowDepthDrawEnd = rendererSource.IndexOf("void PsVitaGxmRenderer::SubmitQuads(", StringComparison.Ordinal);
        string shadowDepthDrawSource = rendererSource.Substring(shadowDepthDrawStart, shadowDepthDrawEnd - shadowDepthDrawStart);
        Assert.DoesNotContain("vita2d_pool_memalign", shadowDepthDrawSource, StringComparison.Ordinal);
        Assert.Contains("ShadowSubmissionMemory", rendererSource, StringComparison.Ordinal);
        Assert.Contains("ReleaseCompletedShadowSubmissionMemory", rendererSource, StringComparison.Ordinal);
        Assert.True(
            bootHostSource.IndexOf("PrepareShadowMaps();", StringComparison.Ordinal) < bootHostSource.IndexOf("GxmRenderer->BeginFrame", StringComparison.Ordinal),
            "The shadow pass must finish before the main Vita frame begins.");
    }
}
