namespace helengine.psvita.builder;

/// <summary>
/// Represents one complete Vita shader compiler output together with the inputs that make the output compatible.
/// </summary>
public sealed class PsVitaShaderArtifact {
    readonly string StageProfileValue;
    readonly string CompilerVersionValue;
    readonly string SourceHashValue;
    readonly string EntryPointValue;
    readonly string OptionsSignatureValue;
    readonly byte[] ProgramBytesValue;

    /// <summary>
    /// Initializes one validated Vita shader artifact description.
    /// </summary>
    /// <param name="stageProfile">Compiler stage profile, such as VP or FP.</param>
    /// <param name="compilerVersion">Runtime compiler version string.</param>
    /// <param name="sourceHash">Hash of the source text used for compilation.</param>
    /// <param name="entryPoint">Compiled shader entry point.</param>
    /// <param name="optionsSignature">Canonical compiler option signature.</param>
    /// <param name="programBytes">Complete compiler program payload.</param>
    public PsVitaShaderArtifact(
        string stageProfile,
        string compilerVersion,
        string sourceHash,
        string entryPoint,
        string optionsSignature,
        byte[] programBytes) {
        StageProfileValue = RequireText(stageProfile, nameof(stageProfile));
        CompilerVersionValue = RequireText(compilerVersion, nameof(compilerVersion));
        SourceHashValue = RequireText(sourceHash, nameof(sourceHash));
        EntryPointValue = RequireText(entryPoint, nameof(entryPoint));
        OptionsSignatureValue = RequireText(optionsSignature, nameof(optionsSignature));
        if (programBytes == null || programBytes.Length == 0) {
            throw new ArgumentException("Vita shader artifacts require complete program bytes.", nameof(programBytes));
        }

        ProgramBytesValue = programBytes.ToArray();
    }

    /// <summary>
    /// Gets the Vita compiler stage profile represented by this artifact.
    /// </summary>
    public string StageProfile => StageProfileValue;

    /// <summary>
    /// Gets the compiler version that produced this artifact.
    /// </summary>
    public string CompilerVersion => CompilerVersionValue;

    /// <summary>
    /// Gets the source hash used to invalidate stale compiled output.
    /// </summary>
    public string SourceHash => SourceHashValue;

    /// <summary>
    /// Gets the compiled shader entry point.
    /// </summary>
    public string EntryPoint => EntryPointValue;

    /// <summary>
    /// Gets the canonical compiler option signature.
    /// </summary>
    public string OptionsSignature => OptionsSignatureValue;

    /// <summary>
    /// Gets a defensive copy of the complete Vita program payload.
    /// </summary>
    public byte[] ProgramBytes => ProgramBytesValue.ToArray();

    /// <summary>
    /// Gets the deterministic SHA-256 identity of the serialized artifact content.
    /// </summary>
    public string ArtifactHash => PsVitaShaderArtifactBinarySerializer.ComputeArtifactHash(this);

    /// <summary>
    /// Requires one non-empty text field for an artifact header.
    /// </summary>
    /// <param name="value">Text field value.</param>
    /// <param name="fieldName">Field name used in the exception.</param>
    /// <returns>The supplied text field.</returns>
    static string RequireText(string value, string fieldName) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Vita shader artifact fields cannot be empty.", fieldName);
        }

        return value;
    }
}
