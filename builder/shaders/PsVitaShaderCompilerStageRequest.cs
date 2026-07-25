namespace helengine.psvita.builder;

/// <summary>
/// Describes one shader stage source that the Vita compiler must compile for a device job.
/// </summary>
public sealed class PsVitaShaderCompilerStageRequest {
    readonly string StageIdValue;
    readonly string SourcePathValue;
    readonly string EntryPointValue;
    readonly string ProfileValue;
    readonly string OptionsSignatureValue;

    /// <summary>
    /// Initializes one validated device compiler stage request.
    /// </summary>
    /// <param name="stageId">Stable identifier that associates the request with its result.</param>
    /// <param name="sourcePath">Relative inbox source path.</param>
    /// <param name="entryPoint">Shader source entry point.</param>
    /// <param name="profile">Vita compiler profile, either VP or FP.</param>
    /// <param name="optionsSignature">Canonical compiler option signature.</param>
    public PsVitaShaderCompilerStageRequest(string stageId, string sourcePath, string entryPoint, string profile, string optionsSignature) {
        StageIdValue = RequireText(stageId, nameof(stageId));
        SourcePathValue = RequireRelativePath(sourcePath, nameof(sourcePath));
        EntryPointValue = RequireText(entryPoint, nameof(entryPoint));
        ProfileValue = RequireProfile(profile);
        OptionsSignatureValue = RequireText(optionsSignature, nameof(optionsSignature));
    }

    /// <summary>
    /// Gets the stable stage result identifier.
    /// </summary>
    public string StageId => StageIdValue;

    /// <summary>
    /// Gets the relative source file path beneath the compiler inbox.
    /// </summary>
    public string SourcePath => SourcePathValue;

    /// <summary>
    /// Gets the source entry point that the device compiler invokes.
    /// </summary>
    public string EntryPoint => EntryPointValue;

    /// <summary>
    /// Gets the Vita compiler profile for this stage.
    /// </summary>
    public string Profile => ProfileValue;

    /// <summary>
    /// Gets the stable compiler option signature for cache and artifact validation.
    /// </summary>
    public string OptionsSignature => OptionsSignatureValue;

    /// <summary>
    /// Requires one non-empty request text field.
    /// </summary>
    /// <param name="value">Candidate field value.</param>
    /// <param name="parameterName">Parameter name used for diagnostics.</param>
    /// <returns>The validated field value.</returns>
    static string RequireText(string value, string parameterName) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Vita shader compiler stage fields cannot be empty.", parameterName);
        }

        return value;
    }

    /// <summary>
    /// Requires a relative source path that remains inside the compiler inbox.
    /// </summary>
    /// <param name="value">Candidate source path.</param>
    /// <param name="parameterName">Parameter name used for diagnostics.</param>
    /// <returns>The validated relative source path.</returns>
    static string RequireRelativePath(string value, string parameterName) {
        string path = RequireText(value, parameterName).Replace('\\', '/');
        if (Path.IsPathRooted(path) || path.Split('/').Any(segment => string.Equals(segment, "..", StringComparison.Ordinal))) {
            throw new ArgumentException("Vita shader compiler source paths must be relative and cannot escape the inbox.", parameterName);
        }

        return path;
    }

    /// <summary>
    /// Requires one supported Vita compiler profile.
    /// </summary>
    /// <param name="value">Candidate profile identifier.</param>
    /// <returns>The validated profile identifier.</returns>
    static string RequireProfile(string value) {
        if (!string.Equals(value, "VP", StringComparison.Ordinal) && !string.Equals(value, "FP", StringComparison.Ordinal)) {
            throw new ArgumentException("Vita shader compiler profiles must be VP or FP.", nameof(value));
        }

        return value;
    }
}
