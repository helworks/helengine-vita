namespace helengine.psvita.builder;

/// <summary>
/// Represents one PS Vita packed model payload whose draw-ready triangle lists were resolved during staging.
/// </summary>
public sealed class PsVitaPackedModelAsset {
    /// <summary>
    /// Gets or sets the copied model-space positions referenced by the packed submesh triangle lists.
    /// </summary>
    public float3[] Positions { get; set; } = [];

    /// <summary>
    /// Gets or sets the authored minimum bounds preserved for runtime framing and culling logic.
    /// </summary>
    public float3 BoundsMin { get; set; }

    /// <summary>
    /// Gets or sets the authored maximum bounds preserved for runtime framing and culling logic.
    /// </summary>
    public float3 BoundsMax { get; set; }

    /// <summary>
    /// Gets or sets the packed index element width in bytes.
    /// </summary>
    public int IndexElementSizeBytes { get; set; }

    /// <summary>
    /// Gets or sets the packed submeshes whose triangle lists can be copied directly into Vita runtime model objects.
    /// </summary>
    public PsVitaPackedModelSubmesh[] Submeshes { get; set; } = [];
}
