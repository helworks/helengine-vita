namespace helengine.psvita.builder;

/// <summary>
/// Represents one shader-program pair stored in a versioned PS Vita shader bundle.
/// </summary>
public sealed class PsVitaShaderBundleEntry {
    /// <summary>
    /// Stores the material-facing shader asset identifier.
    /// </summary>
    readonly string ShaderAssetIdValue;

    /// <summary>
    /// Stores the SHA-256 identity of the source used to compile the entry.
    /// </summary>
    readonly string SourceHashValue;

    /// <summary>
    /// Stores the material-facing vertex-program name.
    /// </summary>
    readonly string VertexProgramNameValue;

    /// <summary>
    /// Stores the material-facing pixel-program name.
    /// </summary>
    readonly string PixelProgramNameValue;

    /// <summary>
    /// Stores the material-facing shader variant name.
    /// </summary>
    readonly string VariantNameValue;

    /// <summary>
    /// Stores the complete serialized vertex PVSA artifact.
    /// </summary>
    readonly byte[] VertexArtifactBytesValue;

    /// <summary>
    /// Stores the complete serialized fragment PVSA artifact.
    /// </summary>
    readonly byte[] FragmentArtifactBytesValue;

    /// <summary>
    /// Initializes one complete shader-program bundle entry.
    /// </summary>
    /// <param name="shaderAssetId">Shader asset identifier referenced by materials.</param>
    /// <param name="sourceHash">SHA-256 hash of the authored source bytes.</param>
    /// <param name="vertexProgramName">Vertex program name referenced by materials.</param>
    /// <param name="pixelProgramName">Pixel program name referenced by materials.</param>
    /// <param name="variantName">Shader variant name referenced by materials.</param>
    /// <param name="vertexArtifactBytes">Complete validated vertex PVSA artifact.</param>
    /// <param name="fragmentArtifactBytes">Complete validated fragment PVSA artifact.</param>
    public PsVitaShaderBundleEntry(
        string shaderAssetId,
        string sourceHash,
        string vertexProgramName,
        string pixelProgramName,
        string variantName,
        byte[] vertexArtifactBytes,
        byte[] fragmentArtifactBytes) {
        ShaderAssetIdValue = RequireText(shaderAssetId, nameof(shaderAssetId));
        SourceHashValue = RequireText(sourceHash, nameof(sourceHash));
        VertexProgramNameValue = RequireText(vertexProgramName, nameof(vertexProgramName));
        PixelProgramNameValue = RequireText(pixelProgramName, nameof(pixelProgramName));
        VariantNameValue = RequireText(variantName, nameof(variantName));
        VertexArtifactBytesValue = CopyArtifactBytes(vertexArtifactBytes, nameof(vertexArtifactBytes));
        FragmentArtifactBytesValue = CopyArtifactBytes(fragmentArtifactBytes, nameof(fragmentArtifactBytes));
    }

    /// <summary>
    /// Gets the shader asset identifier referenced by materials.
    /// </summary>
    public string ShaderAssetId => ShaderAssetIdValue;

    /// <summary>
    /// Gets the SHA-256 identity of the authored shader source.
    /// </summary>
    public string SourceHash => SourceHashValue;

    /// <summary>
    /// Gets the vertex-program name referenced by materials.
    /// </summary>
    public string VertexProgramName => VertexProgramNameValue;

    /// <summary>
    /// Gets the pixel-program name referenced by materials.
    /// </summary>
    public string PixelProgramName => PixelProgramNameValue;

    /// <summary>
    /// Gets the shader variant name referenced by materials.
    /// </summary>
    public string VariantName => VariantNameValue;

    /// <summary>
    /// Gets a copy of the serialized vertex PVSA artifact.
    /// </summary>
    public byte[] VertexArtifactBytes => VertexArtifactBytesValue.ToArray();

    /// <summary>
    /// Gets a copy of the serialized fragment PVSA artifact.
    /// </summary>
    public byte[] FragmentArtifactBytes => FragmentArtifactBytesValue.ToArray();

    /// <summary>
    /// Requires one non-empty metadata value.
    /// </summary>
    /// <param name="value">Candidate metadata value.</param>
    /// <param name="parameterName">Parameter name used in the diagnostic.</param>
    /// <returns>The validated value.</returns>
    static string RequireText(string value, string parameterName) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("PS Vita shader bundle metadata cannot be blank.", parameterName);
        }

        return value;
    }

    /// <summary>
    /// Copies one required non-empty serialized stage artifact.
    /// </summary>
    /// <param name="artifactBytes">Artifact bytes to copy.</param>
    /// <param name="parameterName">Parameter name used in the diagnostic.</param>
    /// <returns>Independent copied artifact bytes.</returns>
    static byte[] CopyArtifactBytes(byte[] artifactBytes, string parameterName) {
        if (artifactBytes == null) {
            throw new ArgumentNullException(parameterName);
        } else if (artifactBytes.Length == 0) {
            throw new ArgumentException("PS Vita shader bundle artifacts cannot be empty.", parameterName);
        }

        return artifactBytes.ToArray();
    }
}
