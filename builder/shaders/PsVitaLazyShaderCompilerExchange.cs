namespace helengine.psvita.builder;

/// <summary>
/// Defers Vita shader compiler transfer-root resolution until a PS Vita shader compilation actually uses the exchange.
/// </summary>
public sealed class PsVitaLazyShaderCompilerExchange : IPsVitaShaderCompilerExchange {
    /// <summary>
    /// Submits one job through the explicitly configured host exchange root.
    /// </summary>
    /// <param name="job">Validated compiler job to submit.</param>
    /// <param name="sourceFiles">Source files referenced by the job.</param>
    public void Submit(PsVitaShaderCompilerJob job, IReadOnlyList<PsVitaShaderCompilerSourceFile> sourceFiles) {
        ResolveExchange().Submit(job, sourceFiles);
    }

    /// <summary>
    /// Attempts to read one completed result through the explicitly configured host exchange root.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="result">Validated device result when present.</param>
    /// <returns>True when a completed result exists.</returns>
    public bool TryReadResult(string jobHash, out PsVitaShaderCompilerResult result) {
        return ResolveExchange().TryReadResult(jobHash, out result);
    }

    /// <summary>
    /// Attempts to read one serialized artifact through the explicitly configured host exchange root.
    /// </summary>
    /// <param name="jobHash">Submitted job identity.</param>
    /// <param name="artifactPath">Relative artifact path reported by the result.</param>
    /// <param name="artifactBytes">Serialized artifact bytes when present.</param>
    /// <returns>True when the artifact exists.</returns>
    public bool TryReadArtifact(string jobHash, string artifactPath, out byte[] artifactBytes) {
        return ResolveExchange().TryReadArtifact(jobHash, artifactPath, out artifactBytes);
    }

    /// <summary>
    /// Resolves the mandatory host exchange root only at the moment a Vita compiler job is submitted or read.
    /// </summary>
    /// <returns>Exchange rooted at the configured host transfer directory.</returns>
    static PsVitaShaderCompilerExchange ResolveExchange() {
        string exchangeRoot = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT");
        if (string.IsNullOrWhiteSpace(exchangeRoot)) {
            throw new InvalidOperationException("PS Vita shader compilation requires HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT. The folder must contain inbox/ and outbox/ mirrors for ux0:data/helengine_shader_compiler/.");
        }

        return new PsVitaShaderCompilerExchange(exchangeRoot);
    }
}
