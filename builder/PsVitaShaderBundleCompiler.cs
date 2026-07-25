using System.Security.Cryptography;
using System.Text;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psvita.builder;

/// <summary>
/// Compiles every shader source required by one cooked Vita build through one device exchange job and writes the resulting runtime bundle.
/// </summary>
public sealed class PsVitaShaderBundleCompiler {
    /// <summary>
    /// Stores the stable profile submitted for Vita vertex programs.
    /// </summary>
    const string VertexProfile = "VP";

    /// <summary>
    /// Stores the stable profile submitted for Vita fragment programs.
    /// </summary>
    const string FragmentProfile = "FP";

    /// <summary>
    /// Stores the shared default vertex entry point used by the current cross-platform shader package contract.
    /// </summary>
    const string VertexEntryPoint = "VS";

    /// <summary>
    /// Stores the shared default fragment entry point used by the current cross-platform shader package contract.
    /// </summary>
    const string FragmentEntryPoint = "PS";

    /// <summary>
    /// Stores the deterministic compiler options accepted by the Vita compiler VPK.
    /// </summary>
    const string OptionsSignature = "O3-W4";

    /// <summary>
    /// Stores the runtime-relative destination of the one Vita shader bundle.
    /// </summary>
    const string BundleRelativePath = "cooked/shaders/psvita/shaders.psvb";

    /// <summary>
    /// Stores the material shader asset identity requiring the established Vita Standard Shader lowering.
    /// </summary>
    const string ForwardStandardShaderAssetId = "ForwardStandardShader";

    /// <summary>
    /// Stores the artifact variant assigned to shader assets that are submitted without target-specific lowering.
    /// </summary>
    const string PassthroughArtifactVariantName = "Passthrough";

    /// <summary>
    /// <summary>
    /// Stores the host-to-device compiler exchange used by this cook operation.
    /// </summary>
    readonly IPsVitaShaderCompilerExchange Exchange;

    /// <summary>
    /// Stores the established lowering for the engine Standard Shader source contract.
    /// </summary>
    readonly PsVitaForwardStandardSourceLowerer ForwardStandardSourceLowerer;

    /// <summary>
    /// Initializes the batch compiler with its explicit host-to-device exchange seam.
    /// </summary>
    /// <param name="exchange">Host inbox and outbox exchange used to compile one complete shader bundle.</param>
    public PsVitaShaderBundleCompiler(IPsVitaShaderCompilerExchange exchange) {
        Exchange = exchange ?? throw new ArgumentNullException(nameof(exchange));
        ForwardStandardSourceLowerer = new PsVitaForwardStandardSourceLowerer();
    }

    /// <summary>
    /// Compiles the requested source set, validates all returned PVSA artifacts, and writes one runtime bundle for the cooked Vita output.
    /// </summary>
    /// <param name="request">Complete material dependencies and resolved shader source set for one cook operation.</param>
    /// <returns>Explicit declaration for the emitted bundle, or no declarations when no material requires a Vita shader.</returns>
    public PlatformShaderArtifactCookResult Cook(PlatformShaderArtifactCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (request.ShaderDependencies.Count == 0) {
            return new PlatformShaderArtifactCookResult(Array.Empty<PlatformCookedArtifactDeclaration>());
        }

        ValidateShaderDependencies(request);
        IReadOnlyList<PlatformShaderArtifactCookSource> sources = GetOrderedSources(request);
        IReadOnlyList<PlatformShaderDependency> dependencies = GetOrderedDependencies(request);
        PsVitaShaderBundleSource[] bundleSources = CreateBundleSources(sources);
        string jobHash = CreateJobHash(bundleSources, dependencies);
        PsVitaShaderCompilerJob job = CreateJob(jobHash, bundleSources);
        Exchange.Submit(job, CreateSourceFiles(jobHash, bundleSources));

        if (!Exchange.TryReadResult(jobHash, out PsVitaShaderCompilerResult result)) {
            throw new InvalidOperationException($"PS Vita shader bundle job '{jobHash}' was written to the host exchange, but no outbox result is available. Transfer inbox/ to ux0:data/helengine_shader_compiler/inbox/, launch Helengine Vita Shader Compiler, then transfer ux0:data/helengine_shader_compiler/outbox/{jobHash}/ back to the host exchange.");
        }

        ValidateResultIdentity(result, job);
        Dictionary<string, byte[]> artifactBytesByStageId = ReadValidatedArtifacts(jobHash, job, result);
        PsVitaShaderBundle bundle = CreateBundle(bundleSources, dependencies, artifactBytesByStageId);
        string bundlePath = BuildBundlePath(request.CookRootPath);
        string bundleDirectoryPath = Path.GetDirectoryName(bundlePath) ?? throw new InvalidOperationException("The Vita shader bundle destination directory is required.");
        Directory.CreateDirectory(bundleDirectoryPath);
        File.WriteAllBytes(bundlePath, new PsVitaShaderBundleBinarySerializer().Serialize(bundle));

        return new PlatformShaderArtifactCookResult([
            new PlatformCookedArtifactDeclaration(BundleRelativePath, "psvita:shader-bundle", "shader", request.PlatformId)
        ]);
    }

    /// <summary>
    /// Resolves the physical cook-root path for the runtime-relative bundle declaration.
    /// </summary>
    /// <param name="cookRootPath">Absolute path that already represents the <c>cooked</c> root.</param>
    /// <returns>Physical bundle destination inside the cooked root.</returns>
    static string BuildBundlePath(string cookRootPath) {
        const string cookedPrefix = "cooked/";
        return Path.Combine(cookRootPath, BundleRelativePath.Substring(cookedPrefix.Length).Replace('/', Path.DirectorySeparatorChar));
    }

    /// <summary>
    /// Validates that this shader-capable platform received complete material lookup keys and source text for every source asset.
    /// </summary>
    /// <param name="request">Cook request to validate.</param>
    static void ValidateShaderDependencies(PlatformShaderArtifactCookRequest request) {
        if (request.ShaderSources.Count == 0) {
            throw new InvalidOperationException("PS Vita shader bundle cooking requires resolved source text for every referenced shader asset.");
        }

        for (int index = 0; index < request.ShaderDependencies.Count; index++) {
            if (!request.ShaderDependencies[index].HasProgramPair) {
                throw new InvalidOperationException($"PS Vita shader bundle cooking requires a vertex program, pixel program, and variant for shader asset '{request.ShaderDependencies[index].ShaderAssetId}'.");
            }
        }
    }

    /// <summary>
    /// Sorts resolved shader sources by their persistent asset identity.
    /// </summary>
    /// <param name="request">Cook request supplying resolved sources.</param>
    /// <returns>Deterministically ordered source records.</returns>
    static IReadOnlyList<PlatformShaderArtifactCookSource> GetOrderedSources(PlatformShaderArtifactCookRequest request) {
        return request.ShaderSources.OrderBy(source => source.ShaderAssetId, StringComparer.Ordinal).ToArray();
    }

    /// <summary>
    /// Sorts material-selected shader program pairs by their complete runtime lookup key.
    /// </summary>
    /// <param name="request">Cook request supplying material dependencies.</param>
    /// <returns>Deterministically ordered program-pair dependencies.</returns>
    static IReadOnlyList<PlatformShaderDependency> GetOrderedDependencies(PlatformShaderArtifactCookRequest request) {
        return request.ShaderDependencies
            .OrderBy(dependency => dependency.ShaderAssetId, StringComparer.Ordinal)
            .ThenBy(dependency => dependency.VertexProgramName, StringComparer.Ordinal)
            .ThenBy(dependency => dependency.PixelProgramName, StringComparer.Ordinal)
            .ThenBy(dependency => dependency.VariantName, StringComparer.Ordinal)
            .ToArray();
    }

    /// <summary>
    /// Creates stage-specific source records, applying only the explicit Standard Shader lowering where required.
    /// </summary>
    /// <param name="sources">Resolved authored sources in deterministic asset order.</param>
    /// <returns>Complete source records used to construct one device job.</returns>
    PsVitaShaderBundleSource[] CreateBundleSources(IReadOnlyList<PlatformShaderArtifactCookSource> sources) {
        List<PsVitaShaderBundleSource> bundleSources = new();
        for (int index = 0; index < sources.Count; index++) {
            PlatformShaderArtifactCookSource source = sources[index];
            if (IsForwardStandardShader(source)) {
                for (int variantIndex = 0; variantIndex < StandardShaderVariants.All.Count; variantIndex++) {
                    StandardShaderVariant variant = StandardShaderVariants.All[variantIndex];
                    PsVitaForwardStandardSourcePair sourcePair = ForwardStandardSourceLowerer.Lower(variant);
                    bundleSources.Add(new PsVitaShaderBundleSource(
                        source,
                        bundleSources.Count,
                        variant.Name,
                        sourcePair.VertexSourceText,
                        sourcePair.FragmentSourceText));
                }
            } else {
                bundleSources.Add(new PsVitaShaderBundleSource(
                    source,
                    bundleSources.Count,
                    PassthroughArtifactVariantName,
                    source.SourceText,
                    source.SourceText));
            }
        }

        return bundleSources.ToArray();
    }

    /// <summary>
    /// Determines whether one source asset uses the engine-owned Standard Shader lowering rather than direct source submission.
    /// </summary>
    /// <param name="source">Resolved shader source to inspect.</param>
    /// <returns>True when the established Standard Shader lowering applies.</returns>
    static bool IsForwardStandardShader(PlatformShaderArtifactCookSource source) {
        return string.Equals(source.ShaderAssetId, ForwardStandardShaderAssetId, StringComparison.Ordinal);
    }

    /// <summary>
    /// Creates the deterministic uppercase device job identity from all source-stage bytes and requested material lookup keys.
    /// </summary>
    /// <param name="sources">Complete stage-specific source records.</param>
    /// <param name="dependencies">Complete material-selected program pair dependencies.</param>
    /// <returns>Filesystem-safe SHA-256 job identity.</returns>
    static string CreateJobHash(IReadOnlyList<PsVitaShaderBundleSource> sources, IReadOnlyList<PlatformShaderDependency> dependencies) {
        StringBuilder identityBuilder = new("helengine-psvita-shader-bundle-v1\n");
        for (int index = 0; index < sources.Count; index++) {
            PsVitaShaderBundleSource source = sources[index];
            identityBuilder.Append(source.Source.ShaderAssetId).Append('\n');
            identityBuilder.Append(source.Source.SourceHash).Append('\n');
            identityBuilder.Append(source.ArtifactVariantName).Append('\n');
            identityBuilder.Append(Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source.VertexSourceText)))).Append('\n');
            identityBuilder.Append(Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source.FragmentSourceText)))).Append('\n');
        }
        for (int index = 0; index < dependencies.Count; index++) {
            PlatformShaderDependency dependency = dependencies[index];
            identityBuilder.Append(dependency.ShaderAssetId).Append('\n');
            identityBuilder.Append(dependency.VertexProgramName).Append('\n');
            identityBuilder.Append(dependency.PixelProgramName).Append('\n');
            identityBuilder.Append(dependency.VariantName).Append('\n');
        }

        return Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(identityBuilder.ToString())));
    }

    /// <summary>
    /// Creates every stage manifest entry required by the batch device compiler job.
    /// </summary>
    /// <param name="jobHash">Deterministic identity assigned to the batch job.</param>
    /// <param name="sources">Complete stage-specific source records.</param>
    /// <returns>Validated compiler job ready for exchange submission.</returns>
    static PsVitaShaderCompilerJob CreateJob(string jobHash, IReadOnlyList<PsVitaShaderBundleSource> sources) {
        List<PsVitaShaderCompilerStageRequest> stages = new(sources.Count * 2);
        for (int index = 0; index < sources.Count; index++) {
            PsVitaShaderBundleSource source = sources[index];
            stages.Add(new PsVitaShaderCompilerStageRequest(source.VertexStageId, BuildSourcePath(jobHash, source.VertexStageId), VertexEntryPoint, VertexProfile, OptionsSignature));
            stages.Add(new PsVitaShaderCompilerStageRequest(source.FragmentStageId, BuildSourcePath(jobHash, source.FragmentStageId), FragmentEntryPoint, FragmentProfile, OptionsSignature));
        }

        return PsVitaShaderCompilerJob.Create(jobHash, stages);
    }

    /// <summary>
    /// Creates source files matching every manifest stage because the current device compiler exchange validates one source file per stage.
    /// </summary>
    /// <param name="jobHash">Deterministic identity assigned to the batch job.</param>
    /// <param name="sources">Complete stage-specific source records.</param>
    /// <returns>Source files ready for exchange submission.</returns>
    static IReadOnlyList<PsVitaShaderCompilerSourceFile> CreateSourceFiles(string jobHash, IReadOnlyList<PsVitaShaderBundleSource> sources) {
        List<PsVitaShaderCompilerSourceFile> sourceFiles = new(sources.Count * 2);
        for (int index = 0; index < sources.Count; index++) {
            PsVitaShaderBundleSource source = sources[index];
            sourceFiles.Add(new PsVitaShaderCompilerSourceFile(BuildSourcePath(jobHash, source.VertexStageId), source.VertexSourceText));
            sourceFiles.Add(new PsVitaShaderCompilerSourceFile(BuildSourcePath(jobHash, source.FragmentStageId), source.FragmentSourceText));
        }

        return sourceFiles;
    }

    /// <summary>
    /// Builds one job-scoped inbox-relative path for a device compiler stage source file.
    /// </summary>
    /// <param name="jobHash">Deterministic batch job identity.</param>
    /// <param name="stageId">Compact stage identifier within the job.</param>
    /// <returns>Path relative to the host exchange inbox root.</returns>
    static string BuildSourcePath(string jobHash, string stageId) {
        return string.Concat("source/", jobHash, "/", stageId, ".cg");
    }

    /// <summary>
    /// Validates that the device result belongs to this exact submitted job and declares precisely its stages.
    /// </summary>
    /// <param name="result">Completed device result manifest.</param>
    /// <param name="job">Original submitted device job.</param>
    static void ValidateResultIdentity(PsVitaShaderCompilerResult result, PsVitaShaderCompilerJob job) {
        if (result == null) {
            throw new InvalidOperationException("PS Vita shader compiler returned an empty result manifest.");
        } else if (!string.Equals(result.JobHash, job.JobHash, StringComparison.Ordinal)) {
            throw new InvalidOperationException($"PS Vita shader compiler returned stale job '{result.JobHash}' while '{job.JobHash}' was required.");
        } else if (result.Stages.Count != job.Stages.Count) {
            throw new InvalidOperationException("PS Vita shader compiler result stage count does not match the submitted batch job.");
        }

        HashSet<string> submittedStageIds = new(job.Stages.Select(stage => stage.StageId), StringComparer.Ordinal);
        HashSet<string> resultStageIds = new(result.Stages.Select(stage => stage.StageId), StringComparer.Ordinal);
        if (submittedStageIds.Count != resultStageIds.Count || !submittedStageIds.SetEquals(resultStageIds)) {
            throw new InvalidOperationException("PS Vita shader compiler result stages do not match the submitted batch job.");
        }
    }

    /// <summary>
    /// Reads every completed device artifact and validates its result metadata against the submitted stage contract.
    /// </summary>
    /// <param name="jobHash">Deterministic submitted job identity.</param>
    /// <param name="job">Original submitted device job.</param>
    /// <param name="result">Completed device result manifest.</param>
    /// <returns>Validated serialized PVSA artifacts keyed by their submitted stage identifiers.</returns>
    Dictionary<string, byte[]> ReadValidatedArtifacts(string jobHash, PsVitaShaderCompilerJob job, PsVitaShaderCompilerResult result) {
        Dictionary<string, PsVitaShaderCompilerStageResult> resultStages = result.Stages.ToDictionary(stage => stage.StageId, StringComparer.Ordinal);
        Dictionary<string, byte[]> artifactBytesByStageId = new(StringComparer.Ordinal);
        PsVitaShaderArtifactBinarySerializer serializer = new();
        for (int index = 0; index < job.Stages.Count; index++) {
            PsVitaShaderCompilerStageRequest submittedStage = job.Stages[index];
            PsVitaShaderCompilerStageResult resultStage = resultStages[submittedStage.StageId];
            if (!resultStage.Success) {
                throw new InvalidOperationException($"PS Vita shader compiler failed stage '{submittedStage.StageId}' for job '{jobHash}': {resultStage.Diagnostic}");
            }

            if (!Exchange.TryReadArtifact(jobHash, resultStage.ArtifactPath, out byte[] artifactBytes)) {
                throw new InvalidOperationException($"PS Vita shader compiler result for job '{jobHash}' references missing artifact '{resultStage.ArtifactPath}'.");
            }

            PsVitaShaderArtifact artifact = serializer.Deserialize(artifactBytes);
            ValidateArtifact(artifact, resultStage, submittedStage);
            artifactBytesByStageId.Add(submittedStage.StageId, artifactBytes);
        }

        return artifactBytesByStageId;
    }

    /// <summary>
    /// Validates one returned PVSA artifact against both its result manifest metadata and submitted compiler stage values.
    /// </summary>
    /// <param name="artifact">Deserialized returned artifact.</param>
    /// <param name="resultStage">Device result metadata for the artifact.</param>
    /// <param name="submittedStage">Original stage manifest entry.</param>
    static void ValidateArtifact(PsVitaShaderArtifact artifact, PsVitaShaderCompilerStageResult resultStage, PsVitaShaderCompilerStageRequest submittedStage) {
        if (!string.Equals(artifact.ArtifactHash, resultStage.ArtifactHash, StringComparison.Ordinal) || artifact.ProgramBytes.Length != resultStage.ProgramByteCount) {
            throw new InvalidOperationException("PS Vita shader compiler artifact does not match its result metadata.");
        } else if (!string.Equals(artifact.StageProfile, submittedStage.Profile, StringComparison.Ordinal)
            || !string.Equals(artifact.EntryPoint, submittedStage.EntryPoint, StringComparison.Ordinal)
            || !string.Equals(artifact.OptionsSignature, submittedStage.OptionsSignature, StringComparison.Ordinal)) {
            throw new InvalidOperationException("PS Vita shader compiler artifact does not match the submitted stage contract.");
        }
    }

    /// <summary>
    /// Creates material lookup entries that reuse each source asset's validated vertex and fragment artifacts.
    /// </summary>
    /// <param name="sources">Complete stage-specific source records.</param>
    /// <param name="dependencies">Material-selected program pair dependencies.</param>
    /// <param name="artifactBytesByStageId">Validated serialized artifacts keyed by submitted stage id.</param>
    /// <returns>Runtime shader bundle with one entry for every material lookup key.</returns>
    static PsVitaShaderBundle CreateBundle(
        IReadOnlyList<PsVitaShaderBundleSource> sources,
        IReadOnlyList<PlatformShaderDependency> dependencies,
        IReadOnlyDictionary<string, byte[]> artifactBytesByStageId) {
        Dictionary<string, PsVitaShaderBundleSource> sourceByArtifactKey = sources.ToDictionary(BuildArtifactKey, StringComparer.Ordinal);
        List<PsVitaShaderBundleEntry> entries = new();
        for (int index = 0; index < dependencies.Count; index++) {
            PlatformShaderDependency dependency = dependencies[index];
            string artifactVariantName = string.Equals(dependency.ShaderAssetId, ForwardStandardShaderAssetId, StringComparison.Ordinal)
                ? BuiltInMaterialIds.StandardForwardVariantName
                : PassthroughArtifactVariantName;
            if (!sourceByArtifactKey.TryGetValue(BuildArtifactKey(dependency.ShaderAssetId, artifactVariantName), out PsVitaShaderBundleSource source)) {
                throw new InvalidOperationException($"PS Vita shader bundle source '{dependency.ShaderAssetId}' was not resolved.");
            }

            entries.Add(new PsVitaShaderBundleEntry(
                dependency.ShaderAssetId,
                source.Source.SourceHash,
                dependency.VertexProgramName,
                dependency.PixelProgramName,
                dependency.VariantName,
                artifactBytesByStageId[source.VertexStageId],
                artifactBytesByStageId[source.FragmentStageId]));
        }

        for (int index = 0; index < sources.Count; index++) {
            PsVitaShaderBundleSource source = sources[index];
            if (!string.Equals(source.Source.ShaderAssetId, ForwardStandardShaderAssetId, StringComparison.Ordinal)
                || string.Equals(source.ArtifactVariantName, BuiltInMaterialIds.StandardForwardVariantName, StringComparison.Ordinal)) {
                continue;
            }

            PlatformShaderDependency dependency = dependencies.FirstOrDefault(candidate => string.Equals(candidate.ShaderAssetId, source.Source.ShaderAssetId, StringComparison.Ordinal));
            if (dependency == null) {
                throw new InvalidOperationException($"PS Vita Standard Shader source '{source.Source.ShaderAssetId}' has no material dependency.");
            }

            entries.Add(new PsVitaShaderBundleEntry(
                dependency.ShaderAssetId,
                source.Source.SourceHash,
                dependency.VertexProgramName,
                dependency.PixelProgramName,
                source.ArtifactVariantName,
                artifactBytesByStageId[source.VertexStageId],
                artifactBytesByStageId[source.FragmentStageId]));
        }

        return new PsVitaShaderBundle(entries);
    }

    /// <summary>
    /// Builds the internal compiler-artifact lookup key for one source asset and Vita-only variant.
    /// </summary>
    /// <param name="source">Source record whose key should be built.</param>
    /// <returns>Unique compiler-artifact key.</returns>
    static string BuildArtifactKey(PsVitaShaderBundleSource source) {
        return BuildArtifactKey(source.Source.ShaderAssetId, source.ArtifactVariantName);
    }

    /// <summary>
    /// Builds the internal compiler-artifact lookup key from its source asset and Vita-only variant values.
    /// </summary>
    /// <param name="shaderAssetId">Persistent shader source asset identity.</param>
    /// <param name="artifactVariantName">Vita-only artifact variant identity.</param>
    /// <returns>Unique compiler-artifact key.</returns>
    static string BuildArtifactKey(string shaderAssetId, string artifactVariantName) {
        return string.Concat(shaderAssetId, "\n", artifactVariantName);
    }

}
