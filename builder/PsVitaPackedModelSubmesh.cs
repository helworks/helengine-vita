namespace helengine.psvita.builder;

/// <summary>
/// Represents one PS Vita packed-model submesh whose triangle indices have already been resolved during staging.
/// </summary>
public sealed class PsVitaPackedModelSubmesh {
    /// <summary>
    /// Gets or sets the stable material slot name associated with the packed submesh.
    /// </summary>
    public string MaterialSlotName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the authored starting index associated with the packed submesh draw range.
    /// </summary>
    public int IndexStart { get; set; }

    /// <summary>
    /// Gets or sets the authored index count associated with the packed submesh draw range.
    /// </summary>
    public int IndexCount { get; set; }

    /// <summary>
    /// Gets or sets the fully resolved triangle index list that the Vita runtime can draw without rebuilding submesh ranges.
    /// </summary>
    public uint[] TriangleIndices { get; set; } = [];
}
