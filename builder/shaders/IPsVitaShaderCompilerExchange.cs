namespace helengine.psvita.builder;

/// <summary>
/// Defines the host-side inbox and outbox transfer seam for the Vita shader compiler VPK.
/// </summary>
public interface IPsVitaShaderCompilerExchange {
    /// <summary>
    /// Writes one compiler job and all of its source files into the host inbox mirror.
    /// </summary>
    /// <param name="job">Validated job manifest to submit.</param>
    /// <param name="sourceFiles">Source files referenced by the manifest.</param>
    void Submit(PsVitaShaderCompilerJob job, IReadOnlyList<PsVitaShaderCompilerSourceFile> sourceFiles);

    /// <summary>
    /// Attempts to read one completed result manifest from the host outbox mirror.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="result">Validated device result when present.</param>
    /// <returns>True when a result manifest is available.</returns>
    bool TryReadResult(string jobHash, out PsVitaShaderCompilerResult result);

    /// <summary>
    /// Attempts to read one serialized PVSA artifact from the host outbox mirror.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="artifactPath">Relative artifact path reported by the result manifest.</param>
    /// <param name="artifactBytes">Serialized artifact bytes when present.</param>
    /// <returns>True when the artifact is available.</returns>
    bool TryReadArtifact(string jobHash, string artifactPath, out byte[] artifactBytes);
}
