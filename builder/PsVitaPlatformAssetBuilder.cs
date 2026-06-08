using helengine;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.psvita.builder;

/// <summary>
/// Implements the PS Vita platform asset builder contract consumed by the editor.
/// </summary>
public sealed class PsVitaPlatformAssetBuilder : IPlatformAssetBuilder, IShaderBackendRegistryContributor {
    /// <summary>
    /// Stable material field identifier used for the authored base color.
    /// </summary>
    const string BaseColorFieldId = "base-color";

    /// <summary>
    /// Stable material field identifier used for the authored diffuse texture reference.
    /// </summary>
    const string TextureFieldId = "texture-id";

    /// <summary>
    /// Constant-buffer name used for the authored base color payload.
    /// </summary>
    const string BaseColorBufferName = "BaseColorBuffer";

    /// <summary>
    /// Native build executor used to produce the PS Vita VPK.
    /// </summary>
    readonly IPsVitaNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Writer that backfills generated runtime component deserializer support when the editor build graph does not emit it for the Vita workspace.
    /// </summary>
    readonly PsVitaGeneratedRuntimeComponentSupportWriter GeneratedRuntimeComponentSupportWriter;

    /// <summary>
    /// Shared shader backend contributor owned by the dynamically loaded PS Vita builder assembly.
    /// </summary>
    readonly PsVitaShaderBackendRegistration ShaderBackendRegistration;

    /// <summary>
    /// Initializes the PS Vita builder with the default Docker-backed native build executor.
    /// </summary>
    public PsVitaPlatformAssetBuilder()
        : this(new PsVitaNativeBuildExecutor(), new PsVitaGeneratedRuntimeComponentSupportWriter()) {
    }

    /// <summary>
    /// Initializes the PS Vita builder with an explicit native build executor.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by this builder instance.</param>
    public PsVitaPlatformAssetBuilder(IPsVitaNativeBuildExecutor nativeBuildExecutor) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        GeneratedRuntimeComponentSupportWriter = new PsVitaGeneratedRuntimeComponentSupportWriter();
        ShaderBackendRegistration = new PsVitaShaderBackendRegistration();
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.psvita.builder",
            "1.0.0",
            "psvita",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["psvita"],
            ["debug"]);
        Definition = PsVitaPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Initializes the PS Vita builder with explicit native build and generated-runtime fallback services.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by this builder instance.</param>
    /// <param name="generatedRuntimeComponentSupportWriter">Generated runtime component fallback writer used by this builder instance.</param>
    internal PsVitaPlatformAssetBuilder(
        IPsVitaNativeBuildExecutor nativeBuildExecutor,
        PsVitaGeneratedRuntimeComponentSupportWriter generatedRuntimeComponentSupportWriter) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        GeneratedRuntimeComponentSupportWriter = generatedRuntimeComponentSupportWriter ?? throw new ArgumentNullException(nameof(generatedRuntimeComponentSupportWriter));
        ShaderBackendRegistration = new PsVitaShaderBackendRegistration();
        Descriptor = new PlatformBuilderDescriptor(
            "helengine.psvita.builder",
            "1.0.0",
            "psvita",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["psvita"],
            ["debug"]);
        Definition = PsVitaPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor for the PS Vita builder assembly.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed PS Vita platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Registers the PS Vita shader backend exposed by this dynamically loaded builder assembly.
    /// </summary>
    /// <param name="shaderBackendRegistry">Shared registry that should receive the PS Vita shader backend.</param>
    public void RegisterShaderBackends(ShaderBackendRegistry shaderBackendRegistry) {
        if (shaderBackendRegistry == null) {
            throw new ArgumentNullException(nameof(shaderBackendRegistry));
        }

        ShaderBackendRegistration.Register(shaderBackendRegistry);
    }

    /// <summary>
    /// Returns the builder-owned cooked material payload for one PS Vita material schema request.
    /// </summary>
    /// <param name="request">Material translation request to validate.</param>
    /// <returns>Cooked material payload.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        string baseColor = request.FieldValues.TryGetValue(BaseColorFieldId, out string authoredBaseColor)
            ? authoredBaseColor
            : "#ffffff";
        string diffuseTextureAssetId = request.FieldValues.TryGetValue(TextureFieldId, out string authoredTextureAssetId) && !string.IsNullOrWhiteSpace(authoredTextureAssetId)
            ? authoredTextureAssetId
            : string.Empty;

        ShaderMaterialAsset materialAsset = new() {
            Id = request.MaterialAssetId,
            ShaderAssetId = string.Empty,
            VertexProgram = string.Empty,
            PixelProgram = string.Empty,
            Variant = "PSVitaForward",
            DiffuseTextureAssetId = diffuseTextureAssetId,
            CastsShadows = false,
            ReceivesShadows = false,
            RenderState = new MaterialRenderState(),
            ConstantBuffers = [
                new MaterialConstantBufferAsset {
                    Name = BaseColorBufferName,
                    Data = CreateFloat4ConstantBufferData(ParseBaseColor(baseColor))
                }
            ]
        };

        return new PlatformMaterialCookResult(global::helengine.files.AssetSerializer.SerializeToBytes(materialAsset), []);
    }

    /// <summary>
    /// Executes one editor-driven PS Vita build and copies the produced VPK into the output root.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <param name="progressReporter">Progress reporter.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter.</param>
    /// <param name="cancellationToken">Cancellation token.</param>
    /// <returns>Final build report.</returns>
    public Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        }

        ResetDirectoryIfPresent(request.OutputRoot);
        ResetDirectoryIfPresent(request.WorkingRoot);
        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(request.WorkingRoot);

        List<PlatformBuildDiagnostic> diagnostics = [];
        List<PlatformBuildItemOutcome> sceneOutcomes = [];
        List<PlatformBuildItemOutcome> looseAssetOutcomes = [];
        string stagedContentRootPath = Path.Combine(request.WorkingRoot, "staged-content");
        Directory.CreateDirectory(stagedContentRootPath);

        int totalItems = request.Manifest.Scenes.Length + request.Manifest.LooseAssets.Length + 1;
        int completedItems = 0;

        StageScenes(request, stagedContentRootPath, diagnostics, diagnosticReporter, progressReporter, sceneOutcomes, ref completedItems, totalItems, cancellationToken);
        StageLooseAssets(request, stagedContentRootPath, diagnostics, diagnosticReporter, progressReporter, looseAssetOutcomes, ref completedItems, totalItems, cancellationToken);

        bool nativeBuildSucceeded = false;
        if (diagnostics.Count == 0) {
            try {
                progressReporter.Report(new PlatformBuildProgressUpdate(
                    "Native Build",
                    "helengine_psvita",
                    completedItems,
                    totalItems,
                    "Running native PS Vita build."));

                string nativeBuildRoot = Path.Combine(request.WorkingRoot, "native");
                string generatedCoreRoot = ResolveGeneratedCoreRoot(request);
                Directory.CreateDirectory(generatedCoreRoot);
                GeneratedRuntimeComponentSupportWriter.EnsureGeneratedRuntimeSupport(
                    generatedCoreRoot,
                    ResolveCookedSceneOutputPaths(request));
                string nativeVpkPath = NativeBuildExecutor.Build(
                    ResolveRepositoryRoot(),
                    nativeBuildRoot,
                    generatedCoreRoot,
                    stagedContentRootPath,
                    cancellationToken);
                string outputVpkPath = Path.Combine(request.OutputRoot, Path.GetFileName(nativeVpkPath));
                File.Copy(nativeVpkPath, outputVpkPath, true);
                nativeBuildSucceeded = true;
                completedItems++;
                progressReporter.Report(new PlatformBuildProgressUpdate(
                    "Native Build",
                    "helengine_psvita",
                    completedItems,
                    totalItems,
                    $"Built PS Vita VPK at '{outputVpkPath}'."));
            } catch (Exception ex) {
                AddDiagnostic(
                    diagnostics,
                    diagnosticReporter,
                    PlatformBuildDiagnosticSeverity.Error,
                    "VITABUILD003",
                    $"Native PS Vita build failed: {ex.Message}",
                    string.Empty,
                    string.Empty,
                    request.OutputRoot);
            }
        }

        bool succeeded = diagnostics.Count == 0
            && sceneOutcomes.TrueForAll(outcome => outcome.OutcomeKind == PlatformBuildItemOutcomeKind.Succeeded)
            && looseAssetOutcomes.TrueForAll(outcome => outcome.OutcomeKind == PlatformBuildItemOutcomeKind.Succeeded)
            && nativeBuildSucceeded;

        return Task.FromResult(new PlatformBuildReport(
            succeeded,
            [.. diagnostics],
            [.. sceneOutcomes],
            [.. looseAssetOutcomes]));
    }

    /// <summary>
    /// Stages all resolved scenes into the PS Vita output and staged content roots.
    /// </summary>
    static void StageScenes(
        PlatformBuildRequest request,
        string stagedContentRootPath,
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        IPlatformBuildProgressReporter progressReporter,
        List<PlatformBuildItemOutcome> sceneOutcomes,
        ref int completedItems,
        int totalItems,
        CancellationToken cancellationToken) {
        for (int sceneIndex = 0; sceneIndex < request.Manifest.Scenes.Length; sceneIndex++) {
            cancellationToken.ThrowIfCancellationRequested();
            PlatformBuildScene scene = request.Manifest.Scenes[sceneIndex];
            bool copied = StageScenePayloads(scene, request.OutputRoot, stagedContentRootPath, diagnostics, diagnosticReporter);
            sceneOutcomes.Add(new PlatformBuildItemOutcome(
                scene.SceneId,
                copied ? PlatformBuildItemOutcomeKind.Succeeded : PlatformBuildItemOutcomeKind.Failed));
            completedItems++;
            progressReporter.Report(new PlatformBuildProgressUpdate(
                "Stage Payloads",
                scene.SceneId,
                completedItems,
                totalItems,
                copied ? $"Staged scene '{scene.SceneName}'." : $"Failed to stage scene '{scene.SceneName}'."));
        }
    }

    /// <summary>
    /// Stages all resolved loose assets into the PS Vita output and staged content roots.
    /// </summary>
    static void StageLooseAssets(
        PlatformBuildRequest request,
        string stagedContentRootPath,
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        IPlatformBuildProgressReporter progressReporter,
        List<PlatformBuildItemOutcome> looseAssetOutcomes,
        ref int completedItems,
        int totalItems,
        CancellationToken cancellationToken) {
        for (int assetIndex = 0; assetIndex < request.Manifest.LooseAssets.Length; assetIndex++) {
            cancellationToken.ThrowIfCancellationRequested();
            PlatformBuildAsset asset = request.Manifest.LooseAssets[assetIndex];
            StagePayload(asset.AssetId, asset.SourceIdentity, request.OutputRoot, stagedContentRootPath, diagnostics, diagnosticReporter, out bool copied);
            looseAssetOutcomes.Add(new PlatformBuildItemOutcome(
                asset.AssetId,
                copied ? PlatformBuildItemOutcomeKind.Succeeded : PlatformBuildItemOutcomeKind.Failed));
            completedItems++;
            progressReporter.Report(new PlatformBuildProgressUpdate(
                "Stage Payloads",
                asset.AssetId,
                completedItems,
                totalItems,
                copied ? $"Staged asset '{asset.AssetName}'." : $"Failed to stage asset '{asset.AssetName}'."));
        }
    }

    /// <summary>
    /// Stages the scene source payload plus any editor-provided cooked payload references required by the runtime scene.
    /// </summary>
    static bool StageScenePayloads(
        PlatformBuildScene scene,
        string outputRoot,
        string stagedContentRootPath,
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter) {
        if (scene == null) {
            throw new ArgumentNullException(nameof(scene));
        }

        HashSet<string> stagedSourceIdentities = new(StringComparer.OrdinalIgnoreCase);
        List<string> sourceIdentities = [];
        if (!string.IsNullOrWhiteSpace(scene.SourceIdentity)) {
            sourceIdentities.Add(scene.SourceIdentity);
        }

        for (int payloadIndex = 0; payloadIndex < scene.PayloadReferences.Length; payloadIndex++) {
            PlatformBuildPayloadReference payloadReference = scene.PayloadReferences[payloadIndex];
            if (payloadReference != null && !string.IsNullOrWhiteSpace(payloadReference.SourceIdentity)) {
                sourceIdentities.Add(payloadReference.SourceIdentity);
            }
        }

        bool allCopied = true;
        for (int sourceIndex = 0; sourceIndex < sourceIdentities.Count; sourceIndex++) {
            string sourceIdentity = sourceIdentities[sourceIndex];
            if (!stagedSourceIdentities.Add(sourceIdentity)) {
                continue;
            }

            StagePayload(scene.SceneId, sourceIdentity, outputRoot, stagedContentRootPath, diagnostics, diagnosticReporter, out bool copied);
            allCopied &= copied;
        }

        return allCopied;
    }

    /// <summary>
    /// Stages one payload into both the editor output root and the native-build content root.
    /// </summary>
    static void StagePayload(
        string itemId,
        string sourceIdentity,
        string outputRoot,
        string stagedContentRootPath,
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        out bool copied) {
        copied = false;
        if (string.IsNullOrWhiteSpace(sourceIdentity)) {
            AddDiagnostic(
                diagnostics,
                diagnosticReporter,
                PlatformBuildDiagnosticSeverity.Error,
                "VITABUILD001",
                $"Item '{itemId}' is missing a source identity.",
                string.Empty,
                itemId,
                itemId);
            return;
        }

        string sourcePath = ResolveSourcePath(sourceIdentity);
        if (!File.Exists(sourcePath)) {
            AddDiagnostic(
                diagnostics,
                diagnosticReporter,
                PlatformBuildDiagnosticSeverity.Error,
                "VITABUILD002",
                $"Payload source '{sourceIdentity}' was not found.",
                string.Empty,
                itemId,
                sourceIdentity);
            return;
        }

        string relativePath = ResolveCookedRelativePath(sourceIdentity);
        byte[] stagedPayloadBytes = ResolveStagedPayloadBytes(sourcePath);
        WriteFile(stagedPayloadBytes, Path.Combine(outputRoot, relativePath));
        WriteFile(stagedPayloadBytes, Path.Combine(stagedContentRootPath, ResolveStagedContentRelativePath(relativePath)));
        copied = true;
    }

    /// <summary>
    /// Adds one diagnostic to the build report and streams it to the reporter.
    /// </summary>
    static void AddDiagnostic(
        List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        PlatformBuildDiagnosticSeverity severity,
        string code,
        string message,
        string sceneId,
        string assetId,
        string sourceIdentity) {
        PlatformBuildDiagnostic diagnostic = new(severity, code, message, sceneId, assetId, sourceIdentity);
        diagnostics.Add(diagnostic);
        diagnosticReporter.Report(diagnostic);
    }

    /// <summary>
    /// Resolves the payload bytes that should be staged for one source asset.
    /// </summary>
    /// <param name="sourcePath">Source file path to inspect.</param>
    /// <returns>Bytes that should be written into the Vita staged-content roots.</returns>
    static byte[] ResolveStagedPayloadBytes(string sourcePath) {
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        byte[] sourceBytes = File.ReadAllBytes(sourcePath);
        if (TryPackModelPayload(sourceBytes, out byte[] packedModelBytes)) {
            return packedModelBytes;
        }

        return sourceBytes;
    }

    /// <summary>
    /// Attempts to convert one generic cooked model payload into the Vita packed-model format.
    /// </summary>
    /// <param name="sourceBytes">Source payload bytes to inspect.</param>
    /// <param name="packedModelBytes">Packed Vita model bytes when the payload is a model asset.</param>
    /// <returns><c>true</c> when the source payload was a generic cooked model that should be transformed for Vita staging.</returns>
    static bool TryPackModelPayload(byte[] sourceBytes, out byte[] packedModelBytes) {
        if (sourceBytes == null) {
            throw new ArgumentNullException(nameof(sourceBytes));
        }

        packedModelBytes = Array.Empty<byte>();
        if (PsVitaPackedModelAssetBinarySerializer.HasMagicPrefix(sourceBytes)) {
            packedModelBytes = sourceBytes;
            return true;
        }

        object asset;
        try {
            asset = global::helengine.files.AssetSerializer.DeserializeFromBytes(sourceBytes);
        } catch {
            return false;
        }

        ModelAsset modelAsset = asset as ModelAsset;
        if (modelAsset == null) {
            return false;
        }

        packedModelBytes = PsVitaPackedModelAssetBinarySerializer.SerializeModelAssetToBytes(modelAsset);
        return true;
    }

    /// <summary>
    /// Writes one file and creates the destination directory if necessary.
    /// </summary>
    /// <param name="bytes">Bytes to write.</param>
    /// <param name="destinationPath">Destination file path.</param>
    static void WriteFile(byte[] bytes, string destinationPath) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        string destinationDirectory = Path.GetDirectoryName(destinationPath);
        if (!string.IsNullOrWhiteSpace(destinationDirectory)) {
            Directory.CreateDirectory(destinationDirectory);
        }

        File.WriteAllBytes(destinationPath, bytes);
    }

    /// <summary>
    /// Resolves one request source identity relative to the staged project root.
    /// </summary>
    /// <param name="sourceIdentity">Source identity from the build manifest.</param>
    /// <returns>Absolute source path.</returns>
    static string ResolveSourcePath(string sourceIdentity) {
        string normalizedSourceIdentity = sourceIdentity.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
        if (Path.IsPathRooted(normalizedSourceIdentity)) {
            return Path.GetFullPath(normalizedSourceIdentity);
        }

        return Path.GetFullPath(Path.Combine(Directory.GetCurrentDirectory(), normalizedSourceIdentity));
    }

    /// <summary>
    /// Resolves the cooked relative path used by the editor output and staged content package.
    /// </summary>
    /// <param name="sourceIdentity">Source identity from the build manifest.</param>
    /// <returns>Cooked relative output path.</returns>
    static string ResolveCookedRelativePath(string sourceIdentity) {
        string normalizedSourceIdentity = sourceIdentity.Replace('\\', '/').TrimStart('/');
        if (Path.IsPathRooted(normalizedSourceIdentity)) {
            normalizedSourceIdentity = Path.GetFileName(normalizedSourceIdentity);
        }

        const string CookedPrefix = "cooked/";
        if (normalizedSourceIdentity.StartsWith(CookedPrefix, StringComparison.OrdinalIgnoreCase)) {
            return normalizedSourceIdentity.Replace('/', Path.DirectorySeparatorChar);
        }

        const string GeneratedPrefix = "generated/";
        if (normalizedSourceIdentity.StartsWith(GeneratedPrefix, StringComparison.OrdinalIgnoreCase)) {
            normalizedSourceIdentity = normalizedSourceIdentity.Substring(GeneratedPrefix.Length);
        }

        return Path.Combine("cooked", normalizedSourceIdentity.Replace('/', Path.DirectorySeparatorChar));
    }

    /// <summary>
    /// Resolves the staged native-content relative path that will be mounted directly into the Vita player repository's cooked root.
    /// </summary>
    /// <param name="cookedRelativePath">Cooked relative path used for the editor output root.</param>
    /// <returns>Relative path rooted at the native staged-content mount.</returns>
    static string ResolveStagedContentRelativePath(string cookedRelativePath) {
        const string CookedPrefix = "cooked";
        if (string.IsNullOrWhiteSpace(cookedRelativePath)) {
            throw new ArgumentException("Cooked relative path must be provided.", nameof(cookedRelativePath));
        }

        string normalizedCookedRelativePath = cookedRelativePath.Replace('\\', '/').TrimStart('/');
        if (normalizedCookedRelativePath.StartsWith(CookedPrefix + "/", StringComparison.OrdinalIgnoreCase)) {
            normalizedCookedRelativePath = normalizedCookedRelativePath.Substring(CookedPrefix.Length + 1);
        }

        return normalizedCookedRelativePath.Replace('/', Path.DirectorySeparatorChar);
    }

    /// <summary>
    /// Resolves the PS Vita repository root from the builder assembly location or environment override.
    /// </summary>
    /// <returns>Absolute repository root path.</returns>
    static string ResolveRepositoryRoot() {
        string environmentRoot = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT");
        if (!string.IsNullOrWhiteSpace(environmentRoot)) {
            string environmentRootPath = Path.GetFullPath(environmentRoot);
            if (IsPsVitaRepositoryRoot(environmentRootPath)) {
                return environmentRootPath;
            }

            throw new DirectoryNotFoundException($"The HELENGINE_PSVITA_REPOSITORY_ROOT path '{environmentRootPath}' does not point to a valid helengine-psvita checkout.");
        }

        string assemblyLocation = typeof(PsVitaPlatformAssetBuilder).Assembly.Location;
        if (string.IsNullOrWhiteSpace(assemblyLocation)) {
            throw new InvalidOperationException("Unable to resolve the PS Vita builder assembly location.");
        }

        string baseDirectory = Path.GetDirectoryName(assemblyLocation)
            ?? throw new InvalidOperationException($"Unable to resolve the PS Vita builder base directory from '{assemblyLocation}'.");
        DirectoryInfo candidateDirectory = new(baseDirectory);
        while (candidateDirectory != null) {
            if (IsPsVitaRepositoryRoot(candidateDirectory.FullName)) {
                return candidateDirectory.FullName;
            }

            candidateDirectory = candidateDirectory.Parent;
        }

        throw new DirectoryNotFoundException($"Unable to locate the helengine-psvita repository root from builder assembly directory '{baseDirectory}'.");
    }

    /// <summary>
    /// Verifies whether one directory contains the PS Vita repository markers required by the native builder.
    /// </summary>
    /// <param name="directoryPath">Directory path to validate.</param>
    /// <returns><c>true</c> when the directory matches the expected PS Vita repository layout.</returns>
    static bool IsPsVitaRepositoryRoot(string directoryPath) {
        if (string.IsNullOrWhiteSpace(directoryPath)) {
            return false;
        }

        return File.Exists(Path.Combine(directoryPath, "Dockerfile"))
            && File.Exists(Path.Combine(directoryPath, "CMakeLists.txt"))
            && File.Exists(Path.Combine(directoryPath, "src", "platform", "psvita", "PsVitaBootHost.hpp"));
    }

    /// <summary>
    /// Resolves the generated-core root used by the native build.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <returns>Absolute generated-core root.</returns>
    static string ResolveGeneratedCoreRoot(PlatformBuildRequest request) {
        string generatedCoreRoot = string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath)
            ? Path.Combine(request.WorkingRoot, "generated-core")
            : request.GeneratedCoreCppRootPath;
        return Path.GetFullPath(generatedCoreRoot);
    }

    /// <summary>
    /// Resolves the cooked scene output paths that should drive Vita-local fallback runtime support generation.
    /// </summary>
    /// <param name="request">Resolved build request whose staged output roots should be inspected.</param>
    /// <returns>Absolute cooked scene output paths that exist beneath the PS Vita output root.</returns>
    static IReadOnlyList<string> ResolveCookedSceneOutputPaths(PlatformBuildRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        List<string> cookedSceneOutputPaths = [];
        for (int sceneIndex = 0; sceneIndex < request.Manifest.Scenes.Length; sceneIndex++) {
            PlatformBuildScene scene = request.Manifest.Scenes[sceneIndex];
            if (scene == null || string.IsNullOrWhiteSpace(scene.SourceIdentity)) {
                continue;
            }

            string cookedSceneOutputPath = Path.Combine(request.OutputRoot, ResolveCookedRelativePath(scene.SourceIdentity));
            if (File.Exists(cookedSceneOutputPath)) {
                cookedSceneOutputPaths.Add(cookedSceneOutputPath);
            }
        }

        return cookedSceneOutputPaths;
    }

    /// <summary>
    /// Deletes one directory tree when it already exists.
    /// </summary>
    /// <param name="path">Directory path to clear.</param>
    static void ResetDirectoryIfPresent(string path) {
        if (!string.IsNullOrWhiteSpace(path) && Directory.Exists(path)) {
            Directory.Delete(path, true);
        }
    }

    /// <summary>
    /// Parses one serialized base-color string into normalized floating-point color channels.
    /// </summary>
    /// <param name="serializedColor">Serialized color string in <c>#RRGGBB</c> or <c>#RRGGBBAA</c> form.</param>
    /// <returns>Normalized color value.</returns>
    static float4 ParseBaseColor(string serializedColor) {
        if (string.IsNullOrWhiteSpace(serializedColor)) {
            return new float4(1f, 1f, 1f, 1f);
        }

        string normalized = serializedColor.Trim();
        if (normalized.StartsWith("#", StringComparison.Ordinal)) {
            normalized = normalized.Substring(1);
        }

        if (normalized.Length != 6 && normalized.Length != 8) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.");
        }

        try {
            byte red = Convert.ToByte(normalized.Substring(0, 2), 16);
            byte green = Convert.ToByte(normalized.Substring(2, 2), 16);
            byte blue = Convert.ToByte(normalized.Substring(4, 2), 16);
            byte alpha = normalized.Length == 8
                ? Convert.ToByte(normalized.Substring(6, 2), 16)
                : (byte)255;

            return new float4(
                red / 255f,
                green / 255f,
                blue / 255f,
                alpha / 255f);
        } catch (FormatException ex) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.", ex);
        }
    }

    /// <summary>
    /// Packs one floating-point color into a 16-byte little-endian constant-buffer payload.
    /// </summary>
    /// <param name="value">Normalized color value to encode.</param>
    /// <returns>Packed constant-buffer bytes.</returns>
    static byte[] CreateFloat4ConstantBufferData(float4 value) {
        using MemoryStream stream = new();
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteSingle(value.X);
        writer.WriteSingle(value.Y);
        writer.WriteSingle(value.Z);
        writer.WriteSingle(value.W);
        return stream.ToArray();
    }
}
