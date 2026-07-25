namespace helengine.psvita.builder;

/// <summary>
/// Compiles self-contained vertex and fragment shaders by exchanging deterministic jobs with the Vita compiler VPK.
/// </summary>
public sealed class PsVitaShaderBackend : IShaderBackend {
    readonly IPsVitaShaderCompilerExchange Exchange;
    readonly ShaderBackendCapabilities CapabilitiesValue;
    readonly ShaderSourceHasher SourceHasher;
    readonly PsVitaForwardStandardSourceLowerer ForwardStandardSourceLowerer;

    /// <summary>
    /// Initializes the device-backed compiler backend with a host-to-Vita exchange implementation.
    /// </summary>
    /// <param name="exchange">Inbox and outbox exchange used to transfer compiler jobs and artifacts.</param>
    public PsVitaShaderBackend(IPsVitaShaderCompilerExchange exchange) {
        Exchange = exchange ?? throw new ArgumentNullException(nameof(exchange));
        CapabilitiesValue = new ShaderBackendCapabilities(
            new ShaderModel(4, 0),
            new ShaderModel(4, 0),
            [ShaderStage.Vertex, ShaderStage.Pixel],
            false);
        SourceHasher = new ShaderSourceHasher();
        ForwardStandardSourceLowerer = new PsVitaForwardStandardSourceLowerer();
    }

    /// <summary>
    /// Gets the PS Vita device compiler target.
    /// </summary>
    public ShaderCompileTarget Target => ShaderCompileTarget.PsVita;

    /// <summary>
    /// Gets the stage and shader-model limits supported by the first device compiler slice.
    /// </summary>
    public ShaderBackendCapabilities Capabilities => CapabilitiesValue;

    /// <summary>
    /// Submits one deterministic device job and returns a matching, validated PVSA artifact.
    /// </summary>
    /// <param name="request">Shader compilation request to submit to the Vita device.</param>
    /// <param name="includeResolver">Shared resolver required by the backend interface.</param>
    /// <returns>Compilation result containing the exact validated PVSA artifact bytes.</returns>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (includeResolver == null) {
            throw new ArgumentNullException(nameof(includeResolver));
        }

        ValidateRequest(request);
        string jobHash = CreateDeviceJobHash(request);
        string stageId = GetStageId(request.Stage);
        string profile = GetProfile(request.Stage);
        string optionsSignature = BuildOptionsSignature(request.Options);
        string deviceSource = ResolveDeviceSource(request);
        string sourcePath = string.Concat("source/", jobHash, "/", stageId, ".cg");
        PsVitaShaderCompilerJob job = PsVitaShaderCompilerJob.Create(
            jobHash,
            [new PsVitaShaderCompilerStageRequest(stageId, sourcePath, request.EntryPoint, profile, optionsSignature)]);
        Exchange.Submit(job, [new PsVitaShaderCompilerSourceFile(sourcePath, deviceSource)]);

        if (!Exchange.TryReadResult(jobHash, out PsVitaShaderCompilerResult result)) {
            throw new InvalidOperationException($"PS Vita shader compiler job '{jobHash}' was written to the host exchange, but no outbox result is available. Transfer inbox/ to ux0:data/helengine_shader_compiler/inbox/, launch Helengine Vita Shader Compiler, then transfer ux0:data/helengine_shader_compiler/outbox/{jobHash}/ back to the host exchange.");
        }
        if (!string.Equals(result.JobHash, jobHash, StringComparison.Ordinal)) {
            throw new InvalidOperationException($"PS Vita shader compiler returned stale job '{result.JobHash}' while '{jobHash}' was required.");
        }

        PsVitaShaderCompilerStageResult stageResult = GetRequiredStageResult(result, stageId);
        if (!stageResult.Success) {
            throw new InvalidOperationException($"PS Vita shader compiler failed stage '{stageId}' for job '{jobHash}': {stageResult.Diagnostic}");
        }
        if (!Exchange.TryReadArtifact(jobHash, stageResult.ArtifactPath, out byte[] artifactBytes)) {
            throw new InvalidOperationException($"PS Vita shader compiler result for job '{jobHash}' references missing artifact '{stageResult.ArtifactPath}'.");
        }

        PsVitaShaderArtifact artifact = new PsVitaShaderArtifactBinarySerializer().Deserialize(artifactBytes);
        ValidateArtifact(artifact, stageResult, request, profile, optionsSignature);
        return new ShaderCompileResult(
            request,
            BuildProgramDefinition(request),
            new ShaderCompiledBinary(request.Target, request.Stage, request.EntryPoint, request.Variant, artifactBytes),
            Array.Empty<ShaderCompileDiagnostic>(),
            true);
    }

    /// <summary>
    /// Validates that the request is supported by the current device compiler protocol.
    /// </summary>
    /// <param name="request">Request to validate.</param>
    void ValidateRequest(ShaderCompileRequest request) {
        if (request.Target != Target) {
            throw new InvalidOperationException("PsVitaShaderBackend only supports PsVita targets.");
        } else if (request.ShaderModel.Major != 4 || request.ShaderModel.Minor != 0) {
            throw new InvalidOperationException("PsVitaShaderBackend currently requires shader model 4.0.");
        } else if (request.Stage != ShaderStage.Vertex && request.Stage != ShaderStage.Pixel) {
            throw new InvalidOperationException("PsVitaShaderBackend currently supports only vertex and pixel stages.");
        } else if (!IsForwardStandardRequest(request) && request.Source.Source.Contains("#include", StringComparison.Ordinal)) {
            throw new InvalidOperationException("PsVitaShaderBackend first requires self-contained source; include expansion will be added with the Standard Shader lowering stage.");
        }
    }

    /// <summary>
    /// Resolves the self-contained Cg source that must be submitted to the Vita device compiler.
    /// </summary>
    /// <param name="request">Original shared shader compilation request.</param>
    /// <returns>Vita-compilable Cg source.</returns>
    string ResolveDeviceSource(ShaderCompileRequest request) {
        if (!IsForwardStandardRequest(request)) {
            return request.Source.Source;
        }

        return request.Stage == ShaderStage.Vertex
            ? ForwardStandardSourceLowerer.LowerVertex()
            : ForwardStandardSourceLowerer.LowerFragment();
    }

    /// <summary>
    /// Determines whether a compile request addresses the shared Forward Standard Shader asset.
    /// </summary>
    /// <param name="request">Compile request to inspect.</param>
    /// <returns>True when the Vita textured-Lambert lowering applies.</returns>
    static bool IsForwardStandardRequest(ShaderCompileRequest request) {
        return request.ProgramName.StartsWith("ForwardStandardShader", StringComparison.Ordinal);
    }

    /// <summary>
    /// Creates the filesystem-safe device job identity owned exclusively by the Vita compiler exchange.
    /// </summary>
    /// <param name="request">Shader compilation request to identify.</param>
    /// <returns>Uppercase SHA-256 identity safe for the fixed Vita outbox path.</returns>
    string CreateDeviceJobHash(ShaderCompileRequest request) {
        string cacheKey = ShaderCompileRequestIdentity.CreateCacheKey(request, SourceHasher).ToString();
        return Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(cacheKey)));
    }

    /// <summary>
    /// Resolves the manifest stage identifier for a supported engine shader stage.
    /// </summary>
    /// <param name="stage">Engine shader stage.</param>
    /// <returns>Stable manifest stage identifier.</returns>
    static string GetStageId(ShaderStage stage) {
        return stage == ShaderStage.Vertex ? "vertex" : "fragment";
    }

    /// <summary>
    /// Resolves the Vita compiler profile for a supported engine shader stage.
    /// </summary>
    /// <param name="stage">Engine shader stage.</param>
    /// <returns>Vita compiler profile.</returns>
    static string GetProfile(ShaderStage stage) {
        return stage == ShaderStage.Vertex ? "VP" : "FP";
    }

    /// <summary>
    /// Builds the deterministic device compiler option signature from shared compile options.
    /// </summary>
    /// <param name="options">Shared compile options to translate.</param>
    /// <returns>Vita compiler option signature.</returns>
    static string BuildOptionsSignature(ShaderCompileOptions options) {
        return string.Concat(
            options.Optimize ? "O3" : "O0",
            options.TreatWarningsAsErrors ? "-W4" : "-W0",
            options.GenerateDebugInfo ? "-G" : string.Empty);
    }

    /// <summary>
    /// Finds the single expected result stage while rejecting missing or unexpected device output.
    /// </summary>
    /// <param name="result">Validated device result manifest.</param>
    /// <param name="stageId">Submitted stage identifier.</param>
    /// <returns>Matching stage result.</returns>
    static PsVitaShaderCompilerStageResult GetRequiredStageResult(PsVitaShaderCompilerResult result, string stageId) {
        if (result.Stages.Count != 1 || !string.Equals(result.Stages[0].StageId, stageId, StringComparison.Ordinal)) {
            throw new InvalidOperationException($"PS Vita shader compiler result must contain exactly the submitted '{stageId}' stage.");
        }

        return result.Stages[0];
    }

    /// <summary>
    /// Validates that the serialized device artifact answers the submitted request and result manifest.
    /// </summary>
    /// <param name="artifact">Deserialized artifact returned by the device.</param>
    /// <param name="stageResult">Result metadata returned beside the artifact.</param>
    /// <param name="request">Original engine compile request.</param>
    /// <param name="profile">Expected Vita compiler profile.</param>
    /// <param name="optionsSignature">Expected device compiler option signature.</param>
    static void ValidateArtifact(
        PsVitaShaderArtifact artifact,
        PsVitaShaderCompilerStageResult stageResult,
        ShaderCompileRequest request,
        string profile,
        string optionsSignature) {
        if (!string.Equals(artifact.ArtifactHash, stageResult.ArtifactHash, StringComparison.Ordinal) || artifact.ProgramBytes.Length != stageResult.ProgramByteCount) {
            throw new InvalidOperationException("PS Vita shader compiler artifact does not match its result metadata.");
        } else if (!string.Equals(artifact.StageProfile, profile, StringComparison.Ordinal) || !string.Equals(artifact.EntryPoint, request.EntryPoint, StringComparison.Ordinal) || !string.Equals(artifact.OptionsSignature, optionsSignature, StringComparison.Ordinal)) {
            throw new InvalidOperationException("PS Vita shader compiler artifact does not match the submitted stage contract.");
        }
    }

    /// <summary>
    /// Builds reflection metadata from the source contract used by the shared cross-platform shader system.
    /// </summary>
    /// <param name="request">Compile request whose source declares the program bindings.</param>
    /// <returns>Program metadata associated with the device artifact.</returns>
    static ShaderProgramDefinition BuildProgramDefinition(ShaderCompileRequest request) {
        ShaderBinding[] bindings = HlslShaderBindingParser.ParseBindings(
            request.Source.Source,
            request.Options.BindingPolicy,
            request.Defines);
        string[] defines = new string[request.Defines.Count];
        for (int index = 0; index < request.Defines.Count; index++) {
            ShaderDefine define = request.Defines[index];
            defines[index] = string.IsNullOrWhiteSpace(define.Value)
                ? define.Name
                : string.Concat(define.Name, "=", define.Value);
        }

        return new ShaderProgramDefinition(
            request.ProgramName,
            request.Stage,
            request.EntryPoint,
            bindings,
            Array.Empty<ShaderVertexElement>(),
            Array.Empty<ShaderVertexElement>(),
            [new ShaderVariant(request.Variant, defines)]);
    }
}
