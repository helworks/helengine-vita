namespace helengine.psvita.builder;

/// <summary>
/// Holds one source text file that accompanies a Vita shader compiler job manifest.
/// </summary>
public sealed class PsVitaShaderCompilerSourceFile {
    readonly string RelativePathValue;
    readonly string SourceTextValue;

    /// <summary>
    /// Initializes one source file for the fixed device compiler inbox.
    /// </summary>
    /// <param name="relativePath">Relative inbox path declared by the job manifest.</param>
    /// <param name="sourceText">Non-empty shader source text.</param>
    public PsVitaShaderCompilerSourceFile(string relativePath, string sourceText) {
        if (string.IsNullOrWhiteSpace(relativePath)) {
            throw new ArgumentException("Vita shader compiler source paths cannot be empty.", nameof(relativePath));
        } else if (Path.IsPathRooted(relativePath) || relativePath.Split('/', '\\').Any(segment => string.Equals(segment, "..", StringComparison.Ordinal))) {
            throw new ArgumentException("Vita shader compiler source paths must be relative and cannot escape the inbox.", nameof(relativePath));
        } else if (string.IsNullOrWhiteSpace(sourceText)) {
            throw new ArgumentException("Vita shader compiler source text cannot be empty.", nameof(sourceText));
        }

        RelativePathValue = relativePath.Replace('\\', '/');
        SourceTextValue = sourceText;
    }

    /// <summary>
    /// Gets the relative inbox path where the source must be written.
    /// </summary>
    public string RelativePath => RelativePathValue;

    /// <summary>
    /// Gets the complete source text submitted for device compilation.
    /// </summary>
    public string SourceText => SourceTextValue;
}
