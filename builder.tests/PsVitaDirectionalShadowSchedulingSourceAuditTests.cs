using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita directional-shadow integration across material cooking and native mesh submission.
/// </summary>
public sealed class PsVitaDirectionalShadowSchedulingSourceAuditTests {
    /// <summary>
    /// Verifies the camera-independent directional shadow map is populated once from active scene drawables so a later empty UI camera cannot clear it.
    /// </summary>
    [Fact]
    public void Source_whenMultipleCamerasAreRegistered_buildsOneShadowPassFromSceneDrawables() {
        string renderManagerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string renderManagerSource = File.ReadAllText(renderManagerPath);
        int prepareStart = renderManagerSource.IndexOf("void PsVitaRenderManager3D::PrepareShadowMaps()", StringComparison.Ordinal);
        int drawStart = renderManagerSource.IndexOf("void PsVitaRenderManager3D::Draw()", prepareStart, StringComparison.Ordinal);

        Assert.True(prepareStart >= 0 && drawStart > prepareStart, "The Vita shadow preparation method must remain independently auditable.");
        string prepareSource = renderManagerSource.Substring(prepareStart, drawStart - prepareStart);
        Assert.Contains("get_Drawables3D()", prepareSource, StringComparison.Ordinal);
        Assert.DoesNotContain("get_Cameras()", prepareSource, StringComparison.Ordinal);
        Assert.DoesNotContain("get_RenderQueue3D()", prepareSource, StringComparison.Ordinal);
        Assert.Equal(1, CountOccurrences(prepareSource, "BeginShadowDepthPass()"));
        Assert.Equal(1, CountOccurrences(prepareSource, "EndShadowDepthPass()"));
        int beginPassIndex = prepareSource.IndexOf("BeginShadowDepthPass()", StringComparison.Ordinal);
        int drawableLoopIndex = prepareSource.IndexOf("for (int32_t drawableIndex", StringComparison.Ordinal);
        int visitDrawableIndex = prepareSource.IndexOf("Visit((*drawables)[drawableIndex]);", StringComparison.Ordinal);
        int endPassIndex = prepareSource.IndexOf("EndShadowDepthPass()", StringComparison.Ordinal);
        Assert.True(
            beginPassIndex >= 0 && beginPassIndex < drawableLoopIndex && drawableLoopIndex < visitDrawableIndex && visitDrawableIndex < endPassIndex,
            "The single Vita shadow pass must visit every active scene drawable before the depth target is closed.");

        int visitStart = renderManagerSource.IndexOf("void PsVitaRenderManager3D::Visit(::IDrawable3D* drawable)", StringComparison.Ordinal);
        int drawCameraStart = renderManagerSource.IndexOf("void PsVitaRenderManager3D::DrawCamera(::ICamera* camera)", visitStart, StringComparison.Ordinal);
        Assert.True(visitStart >= 0 && drawCameraStart > visitStart, "The Vita drawable visitor must remain independently auditable.");
        string visitSource = renderManagerSource.Substring(visitStart, drawCameraStart - visitStart);
        Assert.Contains("(!ShadowDepthPassActive && ActiveCamera == nullptr)", visitSource, StringComparison.Ordinal);
    }

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

    /// <summary>
    /// Counts non-overlapping occurrences of one required production token.
    /// </summary>
    /// <param name="source">Production source section to inspect.</param>
    /// <param name="value">Required token to count.</param>
    /// <returns>Number of non-overlapping token occurrences.</returns>
    static int CountOccurrences(string source, string value) {
        int count = 0;
        int index = 0;
        while ((index = source.IndexOf(value, index, StringComparison.Ordinal)) >= 0) {
            count++;
            index += value.Length;
        }

        return count;
    }
}
