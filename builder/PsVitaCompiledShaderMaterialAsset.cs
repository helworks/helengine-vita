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
    /// Gets or sets the base-color payload packed as little-endian ABGR8.
    /// </summary>
    public uint BaseColorAbgr { get; set; }
}
