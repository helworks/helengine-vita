namespace helengine.psvita.builder;

/// <summary>
/// Describes the device compiler outcome for one submitted shader stage.
/// </summary>
public sealed class PsVitaShaderCompilerStageResult {
    readonly string StageIdValue;
    readonly bool SuccessValue;
    readonly string DiagnosticValue;
    readonly string ArtifactPathValue;
    readonly string ArtifactHashValue;
    readonly int ProgramByteCountValue;

    /// <summary>
    /// Initializes one validated compiler stage result.
    /// </summary>
    /// <param name="stageId">Stable identifier copied from the submitted stage request.</param>
    /// <param name="success">Whether the device compiler successfully emitted an artifact.</param>
    /// <param name="diagnostic">Device compiler diagnostic, empty only for success.</param>
    /// <param name="artifactPath">Relative outbox artifact path, empty only for failure.</param>
    /// <param name="artifactHash">Canonical SHA-256 artifact identity, empty only for failure.</param>
    /// <param name="programByteCount">Compiled program payload size, zero only for failure.</param>
    public PsVitaShaderCompilerStageResult(string stageId, bool success, string diagnostic, string artifactPath, string artifactHash, int programByteCount) {
        StageIdValue = RequireText(stageId, nameof(stageId));
        SuccessValue = success;
        DiagnosticValue = diagnostic ?? throw new ArgumentNullException(nameof(diagnostic));
        ArtifactPathValue = artifactPath ?? throw new ArgumentNullException(nameof(artifactPath));
        ArtifactHashValue = artifactHash ?? throw new ArgumentNullException(nameof(artifactHash));
        ProgramByteCountValue = programByteCount;
        ValidateOutcome();
    }

    /// <summary>
    /// Gets the submitted stage identifier.
    /// </summary>
    public string StageId => StageIdValue;

    /// <summary>
    /// Gets whether the device emitted a complete shader artifact.
    /// </summary>
    public bool Success => SuccessValue;

    /// <summary>
    /// Gets compiler diagnostic text, if any.
    /// </summary>
    public string Diagnostic => DiagnosticValue;

    /// <summary>
    /// Gets the relative artifact path beneath the job's outbox directory.
    /// </summary>
    public string ArtifactPath => ArtifactPathValue;

    /// <summary>
    /// Gets the canonical SHA-256 identity of the serialized artifact.
    /// </summary>
    public string ArtifactHash => ArtifactHashValue;

    /// <summary>
    /// Gets the program payload size reported after artifact validation on the device.
    /// </summary>
    public int ProgramByteCount => ProgramByteCountValue;

    /// <summary>
    /// Requires one non-empty stage identifier.
    /// </summary>
    /// <param name="value">Candidate stage identifier.</param>
    /// <param name="parameterName">Parameter name used for diagnostics.</param>
    /// <returns>The validated stage identifier.</returns>
    static string RequireText(string value, string parameterName) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Vita shader compiler result stage identifiers cannot be empty.", parameterName);
        }

        return value;
    }

    /// <summary>
    /// Validates the required fields for either a successful or failed compiler stage.
    /// </summary>
    void ValidateOutcome() {
        if (SuccessValue) {
            if (string.IsNullOrWhiteSpace(ArtifactPathValue) || Path.IsPathRooted(ArtifactPathValue) || ArtifactPathValue.Split('/', '\\').Any(segment => string.Equals(segment, "..", StringComparison.Ordinal))) {
                throw new ArgumentException("Successful Vita shader compiler stages require a relative artifact path.", nameof(ArtifactPathValue));
            }
            if (!IsCanonicalHash(ArtifactHashValue)) {
                throw new ArgumentException("Successful Vita shader compiler stages require an uppercase SHA-256 artifact hash.", nameof(ArtifactHashValue));
            }
            if (ProgramByteCountValue <= 0) {
                throw new ArgumentException("Successful Vita shader compiler stages require a positive program byte count.", nameof(ProgramByteCountValue));
            }
        } else if (string.IsNullOrWhiteSpace(DiagnosticValue) || !string.IsNullOrEmpty(ArtifactPathValue) || !string.IsNullOrEmpty(ArtifactHashValue) || ProgramByteCountValue != 0) {
            throw new ArgumentException("Failed Vita shader compiler stages require a diagnostic and no artifact metadata.");
        }
    }

    /// <summary>
    /// Determines whether a string is an uppercase hexadecimal SHA-256 value.
    /// </summary>
    /// <param name="value">Candidate hash string.</param>
    /// <returns>True when the value has exactly 64 uppercase hexadecimal characters.</returns>
    static bool IsCanonicalHash(string value) {
        if (value.Length != 64) {
            return false;
        }

        for (int index = 0; index < value.Length; index++) {
            char character = value[index];
            bool isDigit = character >= '0' && character <= '9';
            bool isUppercaseHexLetter = character >= 'A' && character <= 'F';
            if (!isDigit && !isUppercaseHexLetter) {
                return false;
            }
        }

        return true;
    }
}
