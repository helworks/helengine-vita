namespace helengine.psvita.builder;

/// <summary>Stores validated runtime bytes and both contained destinations until the entire Vita package is ready to publish.</summary>
internal sealed class PsVitaPreparedPayload {
    /// <summary>Identifies the cooked artifact for progress reports after publication.</summary>
    public string ItemId { get; init; }
    /// <summary>Retains the canonical cooked-relative path beneath the editor output root.</summary>
    public string OutputRelativePath { get; init; }
    /// <summary>Retains the runtime-relative path mounted beneath the native content root.</summary>
    public string StagedRelativePath { get; init; }
    /// <summary>Owns the fully prepared serialized bytes so source reads and cooking finish before staging is reset.</summary>
    public byte[] Bytes { get; init; }
}
