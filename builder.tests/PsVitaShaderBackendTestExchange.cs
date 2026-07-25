namespace helengine.psvita.builder.tests;

/// <summary>
/// Records one submitted compiler job and returns a controllable device outbox fixture.
/// </summary>
public sealed class PsVitaShaderBackendTestExchange : IPsVitaShaderCompilerExchange {
    readonly byte[] ArtifactBytes;
    readonly bool ReturnStaleResult;

    /// <summary>
    /// Initializes the exchange fixture with a serialized artifact and result identity behavior.
    /// </summary>
    /// <param name="artifactBytes">Serialized PVSA artifact returned for successful reads.</param>
    /// <param name="returnStaleResult">Whether to return a mismatched job identity.</param>
    public PsVitaShaderBackendTestExchange(byte[] artifactBytes, bool returnStaleResult) {
        ArtifactBytes = artifactBytes ?? throw new ArgumentNullException(nameof(artifactBytes));
        ReturnStaleResult = returnStaleResult;
    }

    /// <summary>
    /// Gets the latest job submitted by the backend under test.
    /// </summary>
    public PsVitaShaderCompilerJob SubmittedJob { get; private set; }

    /// <summary>
    /// Gets source files submitted beside the latest recorded job manifest.
    /// </summary>
    public IReadOnlyList<PsVitaShaderCompilerSourceFile> SubmittedSourceFiles { get; private set; }

    /// <summary>
    /// Records the job submitted by the backend.
    /// </summary>
    /// <param name="job">Job submitted for device compilation.</param>
    /// <param name="sourceFiles">Source files written beside the job manifest.</param>
    public void Submit(PsVitaShaderCompilerJob job, IReadOnlyList<PsVitaShaderCompilerSourceFile> sourceFiles) {
        SubmittedJob = job ?? throw new ArgumentNullException(nameof(job));
        if (sourceFiles == null || sourceFiles.Count != 1) {
            throw new ArgumentException("The test exchange requires exactly one submitted source file.", nameof(sourceFiles));
        }

        SubmittedSourceFiles = sourceFiles.ToArray();
    }

    /// <summary>
    /// Returns either a matching or deliberately stale completed result.
    /// </summary>
    /// <param name="jobHash">Job hash requested by the backend.</param>
    /// <param name="result">Result fixture produced by the exchange.</param>
    /// <returns>True because the fixture always has a result after submission.</returns>
    public bool TryReadResult(string jobHash, out PsVitaShaderCompilerResult result) {
        PsVitaShaderArtifact artifact = new PsVitaShaderArtifactBinarySerializer().Deserialize(ArtifactBytes);
        string returnedJobHash = ReturnStaleResult ? "STALE" : jobHash;
        result = new PsVitaShaderCompilerResult(
            returnedJobHash,
            [new PsVitaShaderCompilerStageResult("vertex", true, string.Empty, "vertex.pvsa", artifact.ArtifactHash, artifact.ProgramBytes.Length)]);
        return true;
    }

    /// <summary>
    /// Returns the serialized artifact fixture for the matching stage output path.
    /// </summary>
    /// <param name="jobHash">Job hash requested by the backend.</param>
    /// <param name="artifactPath">Relative artifact path requested by the backend.</param>
    /// <param name="artifactBytes">Serialized PVSA artifact returned to the backend.</param>
    /// <returns>True when the requested artifact path is the fixture path.</returns>
    public bool TryReadArtifact(string jobHash, string artifactPath, out byte[] artifactBytes) {
        if (string.Equals(artifactPath, "vertex.pvsa", StringComparison.Ordinal)) {
            artifactBytes = ArtifactBytes.ToArray();
            return true;
        }

        artifactBytes = Array.Empty<byte>();
        return false;
    }
}
