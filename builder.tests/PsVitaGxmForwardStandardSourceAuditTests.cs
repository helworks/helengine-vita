using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies that the textured Forward Standard Vita path binds a real fragment texture before drawing.
/// </summary>
public sealed class PsVitaGxmForwardStandardSourceAuditTests {
    /// <summary>
    /// Ensures the renderer creates a linear fragment texture binding for the lowered Forward Standard Shader profile.
    /// </summary>
    [Fact]
    public void Source_WhenDrawingTexturedStandardMaterial_BindsTheDiffuseTextureBeforeTheIndexedDraw() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp"));

        Assert.Contains("DrawForwardStandardMesh", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmSetFragmentTexture", source, StringComparison.Ordinal);
        Assert.Contains("SCE_GXM_TEXTURE_FILTER_LINEAR", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmTextureInitLinear(", source, StringComparison.Ordinal);
        Assert.DoesNotContain("sceGxmTextureInitLinearStrided(", source, StringComparison.Ordinal);
        Assert.Contains("diffuseTexture == nullptr", source, StringComparison.Ordinal);
        Assert.Contains("GetOrCreateStandardWhiteTexture", source, StringComparison.Ordinal);
        Assert.Contains("StandardFallbackTextureDimension = 8u", source, StringComparison.Ordinal);
        Assert.Contains("vita2d_create_empty_texture_format(\n            StandardFallbackTextureDimension,\n            StandardFallbackTextureDimension", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures authored mesh UVs outside the normalized range repeat the diffuse texture instead of clamping its edge texels.
    /// </summary>
    [Fact]
    public void Source_WhenDrawingTexturedStandardMaterial_RepeatsDiffuseTextureUvs() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp"));

        Assert.Contains("sceGxmTextureSetUAddrMode(&diffuseGxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT)", source, StringComparison.Ordinal);
        Assert.Contains("sceGxmTextureSetVAddrMode(&diffuseGxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT)", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures textured material submission reads model UV coordinates before validating and binding them.
    /// </summary>
    [Fact]
    public void Source_WhenDrawingTexturedStandardMaterial_ReadsModelUvCoordinates() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp"));

        Assert.Contains(
            "const std::vector<::float3>& normals = runtimeModel->GetNormals();\n        const std::vector<::float2>& texCoords = runtimeModel->GetTexCoords();\n        if (positions.empty() || normals.size() != positions.size())",
            source,
            StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures untextured mesh submission uses the generated-core compatible float2 constructor for default UV coordinates.
    /// </summary>
    [Fact]
    public void Source_WhenDrawingUntexturedMesh_InitializesDefaultUvsWithoutAnUnavailableFloat2Accessor() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp"));

        Assert.Contains("::float2(0.0f, 0.0f)", source, StringComparison.Ordinal);
        Assert.DoesNotContain("::float2::get_Zero()", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures unavailable artifact-backed Standard Shader resources report their exact native failure instead of
    /// degrading into the renderer manager's generic path-selection error.
    /// </summary>
    [Fact]
    public void Source_WhenStandardShaderResourcesCannotBeSelected_ReportsTheConcreteFailure() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmRenderer.cpp"));

        Assert.Contains("PS Vita Forward Standard Shader bundle could not be loaded.", source, StringComparison.Ordinal);
        Assert.Contains("PS Vita Forward Standard Shader bundle does not contain the requested material program.", source, StringComparison.Ordinal);
        Assert.Contains("PS Vita Forward Standard Shader program creation failed from the requested compiled artifacts.", source, StringComparison.Ordinal);
        Assert.Contains("PS Vita Forward Standard Shader failed to initialize its fragment texture binding.", source, StringComparison.Ordinal);
        Assert.Contains("PS Vita Forward Standard Shader program did not provide one active GXM context.", source, StringComparison.Ordinal);
        Assert.Contains("textureInitializationResult", source, StringComparison.Ordinal);
        Assert.Contains("minimumFilterResult", source, StringComparison.Ordinal);
        Assert.Contains("magnificationFilterResult", source, StringComparison.Ordinal);
    }
}
