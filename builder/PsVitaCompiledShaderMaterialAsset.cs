namespace helengine.psvita.builder;

/// <summary>
/// Represents the PS Vita cooked material payload that references one shared shader asset plus one resolved program pair.
/// </summary>
public sealed class PsVitaCompiledShaderMaterialAsset {
    /// <summary>
    /// Gets or sets the shared shader asset identifier referenced by this cooked material.
    /// </summary>
    public string ShaderAssetId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the vertex-program name that the PS Vita runtime should bind for this material.
    /// </summary>
    public string VertexProgramName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the pixel-program name that the PS Vita runtime should bind for this material.
    /// </summary>
    public string PixelProgramName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the shader variant name selected for this cooked material.
    /// </summary>
    public string VariantName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the parameter contract version expected by the runtime binder.
    /// </summary>
    public uint ParameterContractVersion { get; set; }

    /// <summary>
    /// Gets or sets the base-color payload packed as little-endian ABGR8.
    /// </summary>
    public uint BaseColorAbgr { get; set; }

    /// <summary>
    /// Gets or sets whether the selected shader profile requires a diffuse sampler binding at draw time.
    /// </summary>
    public bool RequiresDiffuseTexture { get; set; }

    /// <summary>
    /// Gets or sets the cooked diffuse texture asset identity used by the shader profile, or an empty value when the renderer must bind its native white fallback texture.
    /// </summary>
    public string DiffuseTextureAssetId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets whether this material contributes geometry to the directional shadow depth pass.
    /// </summary>
    public bool CastsShadows { get; set; } = true;

    /// <summary>
    /// Gets or sets whether this material receives directional shadow attenuation in the forward pass.
    /// </summary>
    public bool ReceivesShadows { get; set; } = true;
}
