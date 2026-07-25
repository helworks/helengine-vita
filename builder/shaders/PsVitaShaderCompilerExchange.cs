namespace helengine.psvita.builder;

/// <summary>
/// Moves deterministic shader compiler jobs through a host folder that mirrors the Vita fixed inbox and outbox paths.
/// </summary>
public sealed class PsVitaShaderCompilerExchange : IPsVitaShaderCompilerExchange {
    readonly string RootPath;
    readonly PsVitaShaderCompilerJobSerializer JobSerializer;
    readonly PsVitaShaderCompilerResultSerializer ResultSerializer;

    /// <summary>
    /// Initializes the exchange at an explicit host folder used for Vita file transfer.
    /// </summary>
    /// <param name="rootPath">Host root containing the inbox and outbox directories.</param>
    public PsVitaShaderCompilerExchange(string rootPath) {
        if (string.IsNullOrWhiteSpace(rootPath)) {
            throw new ArgumentException("Vita shader compiler exchange root must be provided.", nameof(rootPath));
        }

        RootPath = Path.GetFullPath(rootPath);
        JobSerializer = new PsVitaShaderCompilerJobSerializer();
        ResultSerializer = new PsVitaShaderCompilerResultSerializer();
    }

    /// <summary>
    /// Writes sources before publishing the manifest that makes a job visible to the device compiler.
    /// </summary>
    /// <param name="job">Validated job manifest to submit.</param>
    /// <param name="sourceFiles">Source files referenced by the manifest.</param>
    public void Submit(PsVitaShaderCompilerJob job, IReadOnlyList<PsVitaShaderCompilerSourceFile> sourceFiles) {
        if (job == null) {
            throw new ArgumentNullException(nameof(job));
        } else if (sourceFiles == null) {
            throw new ArgumentNullException(nameof(sourceFiles));
        }

        ValidateSourceFiles(job, sourceFiles);
        string inboxRoot = Path.Combine(RootPath, "inbox");
        Directory.CreateDirectory(inboxRoot);
        for (int index = 0; index < sourceFiles.Count; index++) {
            PsVitaShaderCompilerSourceFile sourceFile = sourceFiles[index];
            string sourcePath = ResolvePathInsideRoot(inboxRoot, sourceFile.RelativePath);
            string sourceDirectory = Path.GetDirectoryName(sourcePath) ?? throw new InvalidOperationException("Vita shader compiler source directory could not be resolved.");
            Directory.CreateDirectory(sourceDirectory);
            File.WriteAllText(sourcePath, sourceFile.SourceText);
        }

        File.WriteAllText(Path.Combine(inboxRoot, "manifest.json"), JobSerializer.Serialize(job));
    }

    /// <summary>
    /// Attempts to read and validate a completed device result manifest.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="result">Validated device result when present.</param>
    /// <returns>True when a result manifest is available.</returns>
    public bool TryReadResult(string jobHash, out PsVitaShaderCompilerResult result) {
        string resultPath = Path.Combine(ResolveOutboxJobRoot(jobHash), "results.json");
        if (!File.Exists(resultPath)) {
            result = null;
            return false;
        }

        result = ResultSerializer.Deserialize(File.ReadAllText(resultPath));
        return true;
    }

    /// <summary>
    /// Attempts to read one outbox artifact while enforcing its declared relative path.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="artifactPath">Relative artifact path reported by the result manifest.</param>
    /// <param name="artifactBytes">Serialized artifact bytes when present.</param>
    /// <returns>True when the artifact is available.</returns>
    public bool TryReadArtifact(string jobHash, string artifactPath, out byte[] artifactBytes) {
        string artifactRoot = ResolveOutboxJobRoot(jobHash);
        string fullArtifactPath = ResolvePathInsideRoot(artifactRoot, artifactPath);
        if (!File.Exists(fullArtifactPath)) {
            artifactBytes = Array.Empty<byte>();
            return false;
        }

        artifactBytes = File.ReadAllBytes(fullArtifactPath);
        return true;
    }

    /// <summary>
    /// Validates that every submitted source maps one-to-one to the job's declared stage source path.
    /// </summary>
    /// <param name="job">Job whose stages declare expected source paths.</param>
    /// <param name="sourceFiles">Source files to write.</param>
    static void ValidateSourceFiles(PsVitaShaderCompilerJob job, IReadOnlyList<PsVitaShaderCompilerSourceFile> sourceFiles) {
        if (sourceFiles.Count != job.Stages.Count) {
            throw new ArgumentException("Vita shader compiler jobs require exactly one source file per stage.", nameof(sourceFiles));
        }

        HashSet<string> paths = new(StringComparer.Ordinal);
        for (int index = 0; index < job.Stages.Count; index++) {
            PsVitaShaderCompilerStageRequest stage = job.Stages[index];
            PsVitaShaderCompilerSourceFile sourceFile = sourceFiles[index] ?? throw new ArgumentException("Vita shader compiler jobs cannot contain null source files.", nameof(sourceFiles));
            if (!string.Equals(stage.SourcePath, sourceFile.RelativePath, StringComparison.Ordinal) || !paths.Add(sourceFile.RelativePath)) {
                throw new ArgumentException("Vita shader compiler source files must match unique stage source paths in order.", nameof(sourceFiles));
            }
        }
    }

    /// <summary>
    /// Resolves the host mirror directory for one device compiler outbox job.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <returns>Absolute host outbox directory for the job.</returns>
    string ResolveOutboxJobRoot(string jobHash) {
        if (string.IsNullOrWhiteSpace(jobHash) || jobHash.Any(character => !((character >= '0' && character <= '9') || (character >= 'A' && character <= 'F')))) {
            throw new ArgumentException("Vita shader compiler job hashes must use uppercase hexadecimal characters.", nameof(jobHash));
        }

        return Path.Combine(RootPath, "outbox", jobHash);
    }

    /// <summary>
    /// Resolves a path beneath a required root and rejects attempts to escape it.
    /// </summary>
    /// <param name="rootPath">Absolute root that contains the requested file.</param>
    /// <param name="relativePath">Relative file path beneath the root.</param>
    /// <returns>Absolute file path inside the required root.</returns>
    static string ResolvePathInsideRoot(string rootPath, string relativePath) {
        if (string.IsNullOrWhiteSpace(relativePath) || Path.IsPathRooted(relativePath)) {
            throw new ArgumentException("Vita shader compiler paths must be relative.", nameof(relativePath));
        }

        string fullRootPath = Path.GetFullPath(rootPath).TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string fullPath = Path.GetFullPath(Path.Combine(fullRootPath, relativePath));
        if (!fullPath.StartsWith(fullRootPath, StringComparison.OrdinalIgnoreCase)) {
            throw new ArgumentException("Vita shader compiler paths cannot escape their transfer root.", nameof(relativePath));
        }

        return fullPath;
    }
}
