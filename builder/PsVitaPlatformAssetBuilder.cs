using helengine;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>
/// Implements the PS Vita platform asset builder contract consumed by the editor.
/// </summary>
public sealed class PsVitaPlatformAssetBuilder : IPlatformAssetBuilder, IShaderBackendRegistryContributor, IPlatformShaderArtifactBuilder {
    /// <summary>
    /// Stable material field identifier used for the authored base color.
    /// </summary>
    const string BaseColorFieldId = "base-color";

    /// <summary>
    /// Stable material field identifier used for the authored diffuse texture reference.
    /// </summary>
    const string TextureFieldId = "texture-id";

    /// <summary>
    /// Stable material field identifier used for one shared shader asset reference.
    /// </summary>
    const string ShaderAssetIdFieldId = "shader-asset-id";

    /// <summary>
    /// Stable material field identifier used for one resolved vertex-program name.
    /// </summary>
    const string VertexProgramFieldId = "vertex-program";

    /// <summary>
    /// Stable material field identifier used for one resolved pixel-program name.
    /// </summary>
    const string PixelProgramFieldId = "pixel-program";

    /// <summary>
    /// Stable material field identifier used for one resolved shader variant name.
    /// </summary>
    const string VariantFieldId = "variant";

    /// <summary>
    /// Stable material field identifier for the runtime parameter contract version.
    /// </summary>
    const string ParameterContractFieldId = "parameter-contract";

    /// <summary>
    /// Stable material field identifier used for the generic shadow-caster policy.
    /// </summary>
    const string CastsShadowsFieldId = "casts-shadows";

    /// <summary>
    /// Stable material field identifier used for the generic shadow-receiver policy.
    /// </summary>
    const string ReceivesShadowsFieldId = "receives-shadows";

    /// <summary>
    /// Parameter contract version implemented by the Forward Standard Shader artifact pair.
    /// </summary>
    const uint ForwardStandardParameterContractVersion = 1u;

    /// <summary>
    /// Constant-buffer name used for the authored base color payload.
    /// </summary>
    const string BaseColorBufferName = "BaseColorBuffer";

    /// <summary>
    /// Native build executor used to produce the PS Vita VPK.
    /// </summary>
    readonly IPsVitaNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Source processor used to execute builder-owned Vita texture cook work items.
    /// </summary>
    readonly IPsVitaPlatformCookSourceProcessor PlatformCookSourceProcessor;

    /// <summary>
    /// Writer that backfills generated runtime component deserializer support when the editor build graph does not emit it for the Vita workspace.
    /// </summary>
    readonly PsVitaGeneratedRuntimeComponentSupportWriter GeneratedRuntimeComponentSupportWriter;

    /// <summary>
    /// Host-to-device exchange used to compile all shader sources required by one cooked Vita build.
    /// </summary>
    readonly IPsVitaShaderCompilerExchange ShaderCompilerExchange;

    /// <summary>
    /// Initializes the PS Vita builder with the default Docker-backed native build executor.
    /// </summary>
    public PsVitaPlatformAssetBuilder()
        : this(new PsVitaNativeBuildExecutor(), new PsVitaPlatformCookSourceProcessor(), new PsVitaGeneratedRuntimeComponentSupportWriter(), new PsVitaLazyShaderCompilerExchange()) {
    }

    /// <summary>
    /// Initializes the PS Vita builder with an explicit native build executor.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by this builder instance.</param>
    public PsVitaPlatformAssetBuilder(IPsVitaNativeBuildExecutor nativeBuildExecutor) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        PlatformCookSourceProcessor = new PsVitaPlatformCookSourceProcessor();
        GeneratedRuntimeComponentSupportWriter = new PsVitaGeneratedRuntimeComponentSupportWriter();
        ShaderCompilerExchange = new PsVitaLazyShaderCompilerExchange();
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
    /// <param name="platformCookSourceProcessor">Builder-owned source processor used by this builder instance.</param>
    /// <param name="generatedRuntimeComponentSupportWriter">Generated runtime component fallback writer used by this builder instance.</param>
    internal PsVitaPlatformAssetBuilder(
        IPsVitaNativeBuildExecutor nativeBuildExecutor,
        IPsVitaPlatformCookSourceProcessor platformCookSourceProcessor,
        PsVitaGeneratedRuntimeComponentSupportWriter generatedRuntimeComponentSupportWriter) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        PlatformCookSourceProcessor = platformCookSourceProcessor ?? throw new ArgumentNullException(nameof(platformCookSourceProcessor));
        GeneratedRuntimeComponentSupportWriter = generatedRuntimeComponentSupportWriter ?? throw new ArgumentNullException(nameof(generatedRuntimeComponentSupportWriter));
        ShaderCompilerExchange = new PsVitaLazyShaderCompilerExchange();
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
    /// Initializes the PS Vita builder with every integration seam explicit for shader bundle testing.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor used by this builder instance.</param>
    /// <param name="platformCookSourceProcessor">Builder-owned source processor used by this builder instance.</param>
    /// <param name="generatedRuntimeComponentSupportWriter">Generated runtime component fallback writer used by this builder instance.</param>
    /// <param name="shaderCompilerExchange">Host-to-device compiler exchange used by shader bundle cooking.</param>
    internal PsVitaPlatformAssetBuilder(
        IPsVitaNativeBuildExecutor nativeBuildExecutor,
        IPsVitaPlatformCookSourceProcessor platformCookSourceProcessor,
        PsVitaGeneratedRuntimeComponentSupportWriter generatedRuntimeComponentSupportWriter,
        IPsVitaShaderCompilerExchange shaderCompilerExchange) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        PlatformCookSourceProcessor = platformCookSourceProcessor ?? throw new ArgumentNullException(nameof(platformCookSourceProcessor));
        GeneratedRuntimeComponentSupportWriter = generatedRuntimeComponentSupportWriter ?? throw new ArgumentNullException(nameof(generatedRuntimeComponentSupportWriter));
        ShaderCompilerExchange = shaderCompilerExchange ?? throw new ArgumentNullException(nameof(shaderCompilerExchange));
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
    /// Registers the device-backed PS Vita shader compiler backend for PS Vita editor builds.
    /// </summary>
    /// <param name="shaderBackendRegistry">Registry that receives the PS Vita backend.</param>
    public void RegisterShaderBackends(ShaderBackendRegistry shaderBackendRegistry) {
        if (shaderBackendRegistry == null) {
            throw new ArgumentNullException(nameof(shaderBackendRegistry));
        }

        shaderBackendRegistry.Register(new PsVitaShaderBackend(ShaderCompilerExchange));
    }

    /// <summary>
    /// Compiles all material-referenced shader sources into the one runtime bundle consumed by the Vita renderer.
    /// </summary>
    /// <param name="request">Complete shader source and material lookup-key cook request.</param>
    /// <returns>Explicit declaration for the emitted shader bundle.</returns>
    public PlatformShaderArtifactCookResult CookShaderArtifacts(PlatformShaderArtifactCookRequest request) {
        return new PsVitaShaderBundleCompiler(ShaderCompilerExchange).Cook(request);
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

        if (request.FieldValues.TryGetValue(ShaderAssetIdFieldId, out string shaderAssetId)
            && !string.IsNullOrWhiteSpace(shaderAssetId)) {
            string vertexProgramName = GetRequiredFieldValue(request.FieldValues, VertexProgramFieldId);
            string pixelProgramName = GetRequiredFieldValue(request.FieldValues, PixelProgramFieldId);
            string variantName = GetRequiredFieldValue(request.FieldValues, VariantFieldId);
            string shaderDiffuseTextureAssetId = request.FieldValues.TryGetValue(TextureFieldId, out string authoredDiffuseTextureAssetId)
                && !string.IsNullOrWhiteSpace(authoredDiffuseTextureAssetId)
                ? authoredDiffuseTextureAssetId
                : string.Empty;
            PsVitaCompiledShaderMaterialAsset cookedAsset = new() {
                ShaderAssetId = shaderAssetId,
                VertexProgramName = vertexProgramName,
                PixelProgramName = pixelProgramName,
                VariantName = variantName,
                ParameterContractVersion = request.FieldValues.TryGetValue(ParameterContractFieldId, out string authoredParameterContractVersion)
                    && !string.IsNullOrWhiteSpace(authoredParameterContractVersion)
                    ? ParseParameterContractVersion(authoredParameterContractVersion)
                    : ForwardStandardParameterContractVersion,
                BaseColorAbgr = ParseBaseColorAbgr(request.FieldValues.TryGetValue(BaseColorFieldId, out string authoredShaderBaseColor)
                    ? authoredShaderBaseColor
                    : "#ffffff"),
                RequiresDiffuseTexture = !string.IsNullOrWhiteSpace(shaderDiffuseTextureAssetId),
                DiffuseTextureAssetId = shaderDiffuseTextureAssetId,
                CastsShadows = ResolveShadowFlag(request.FieldValues, CastsShadowsFieldId),
                ReceivesShadows = ResolveShadowFlag(request.FieldValues, ReceivesShadowsFieldId)
            };

            return new PlatformMaterialCookResult(
                new PsVitaCompiledShaderMaterialBinarySerializer().Serialize(cookedAsset),
                [new PlatformShaderDependency(
                    cookedAsset.ShaderAssetId,
                    cookedAsset.VertexProgramName,
                    cookedAsset.PixelProgramName,
                    cookedAsset.VariantName)]);
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
    /// Resolves one generic material shadow policy from the editor-provided cook field map.
    /// </summary>
    /// <param name="fieldValues">Complete material cook field map.</param>
    /// <param name="fieldId">Stable shadow-policy field identifier.</param>
    /// <returns>The authored Boolean policy, or the engine material default when an older caller omitted the additive field.</returns>
    static bool ResolveShadowFlag(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        if (!fieldValues.TryGetValue(fieldId, out string text) || string.IsNullOrWhiteSpace(text)) {
            return true;
        } else if (bool.TryParse(text, out bool value)) {
            return value;
        }

        throw new InvalidOperationException($"PS Vita material field '{fieldId}' must be Boolean.");
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

        List<PlatformBuildDiagnostic> diagnostics = [];
        List<PlatformBuildItemOutcome> sceneOutcomes = [];
        List<PlatformBuildItemOutcome> looseAssetOutcomes = [];
        string stagedContentRootPath = Path.Combine(request.WorkingRoot, "staged-content");
        IReadOnlyList<PsVitaPreparedPayload> prepared = PreparePackagedPayloads(request, stagedContentRootPath, diagnostics, diagnosticReporter, cancellationToken);
        if (diagnostics.Count > 0) return Task.FromResult(new PlatformBuildReport(false, [.. diagnostics],
            request.Manifest.Scenes.Select(scene => new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Failed)).ToArray(),
            request.Manifest.LooseAssets.Select(asset => new PlatformBuildItemOutcome(asset.AssetId, PlatformBuildItemOutcomeKind.Failed)).ToArray()));
        cancellationToken.ThrowIfCancellationRequested();

        ResetDirectoryIfPresent(request.OutputRoot);
        ResetDirectoryIfPresent(request.WorkingRoot);
        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(stagedContentRootPath);
        foreach (PsVitaPreparedPayload payload in prepared) {
            WriteFile(payload.Bytes, PsVitaTextureCookPath.Resolve(request.OutputRoot, payload.OutputRelativePath));
            WriteFile(payload.Bytes, PsVitaTextureCookPath.Resolve(stagedContentRootPath, payload.StagedRelativePath));
        }

        int totalItems = request.Manifest.Scenes.Length + request.Manifest.LooseAssets.Length + 1;
        int completedItems = 0;
        foreach (PlatformBuildScene scene in request.Manifest.Scenes) {
            sceneOutcomes.Add(new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Succeeded));
            progressReporter.Report(new PlatformBuildProgressUpdate("Stage Payloads", scene.SceneId, ++completedItems, totalItems, $"Staged scene '{scene.SceneName}'."));
        }
        foreach (PlatformBuildAsset asset in request.Manifest.LooseAssets) {
            looseAssetOutcomes.Add(new PlatformBuildItemOutcome(asset.AssetId, PlatformBuildItemOutcomeKind.Succeeded));
            progressReporter.Report(new PlatformBuildProgressUpdate("Stage Payloads", asset.AssetId, ++completedItems, totalItems, $"Staged asset '{asset.AssetName}'."));
        }

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
                string nativeObjectCacheRoot = string.IsNullOrWhiteSpace(request.NativeObjectCacheRoot)
                    ? Path.Combine(nativeBuildRoot, "native-cache")
                    : request.NativeObjectCacheRoot;
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
                    cancellationToken,
                    request.SelectedBuildOptionValues != null
                        && request.SelectedBuildOptionValues.TryGetValue("game-name", out string authoredGameName)
                        ? authoredGameName
                        : string.Empty,
                    nativeObjectCacheRoot);
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

    /// <summary>Validates package paths and prepares every source and texture cook before replacing either existing output tree.</summary>
    IReadOnlyList<PsVitaPreparedPayload> PreparePackagedPayloads(PlatformBuildRequest request, string stagedRoot, List<PlatformBuildDiagnostic> diagnostics,
        IPlatformBuildDiagnosticReporter reporter, CancellationToken cancellationToken) {
        List<PsVitaPreparedPayload> prepared = [];
        HashSet<string> reserved = new(StringComparer.OrdinalIgnoreCase);
        Dictionary<string, string> sourcesByDestination = new(StringComparer.OrdinalIgnoreCase);
        try {
            PsVitaTextureCookPath.ValidateRoot(request.OutputRoot);
            PsVitaTextureCookPath.ValidateRoot(request.WorkingRoot);
            PsVitaTextureCookPath.ValidateRoot(stagedRoot);
            foreach (PlatformBuildScene scene in request.Manifest.Scenes) {
                PrepareSourcePayload(scene.SourceIdentity, scene.SceneId, request, stagedRoot, prepared, reserved, sourcesByDestination, cancellationToken);
                foreach (PlatformBuildPayloadReference reference in scene.PayloadReferences) {
                    PrepareSourcePayload(reference.SourceIdentity, scene.SceneId, request, stagedRoot, prepared, reserved, sourcesByDestination, cancellationToken);
                }
            }
            foreach (PlatformBuildAsset asset in request.Manifest.LooseAssets) {
                PrepareSourcePayload(asset.SourceIdentity, asset.AssetId, request, stagedRoot, prepared, reserved, sourcesByDestination, cancellationToken);
            }
            foreach (PlatformBuildArtifact artifact in request.Manifest.CookedArtifacts ?? []) {
                PsVitaTextureCookPath.Normalize(artifact.RelativePath);
                PrepareSourcePayload(artifact.RelativePath, artifact.LogicalArtifactId, request, stagedRoot, prepared, reserved, sourcesByDestination, cancellationToken);
            }
            PlatformCookWorkItem[] workItems = request.Manifest.PlatformCookWorkItems ?? [];
            foreach (PlatformCookWorkItem item in workItems) {
                ArgumentNullException.ThrowIfNull(item);
                if (item.TargetPlatformId != Definition.PlatformId || item.TargetArtifactKind != "runtime-texture"
                    || (item.SourceAssetKind != "texture" && item.SourceAssetKind != "font-atlas-texture")) {
                    throw new InvalidOperationException($"Unsupported Vita cook contract for '{item.WorkItemId}'.");
                }
                PsVitaTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings);
                string outputRelative = ResolveCookedRelativePath(PsVitaTextureCookPath.Normalize(item.OutputRelativePath));
                string stagedRelative = ResolveStagedContentRelativePath(outputRelative).Replace('\\', '/');
                PsVitaTextureCookPath.Reserve(reserved, stagedRelative);
                PsVitaTextureCookPath.Resolve(request.OutputRoot, outputRelative);
                PsVitaTextureCookPath.Resolve(stagedRoot, stagedRelative);
                ValidatePreparedSource(ResolveSourcePath(item.SourceAssetPath), request);
            }
            foreach (PlatformCookWorkItem item in workItems) {
                cancellationToken.ThrowIfCancellationRequested();
                string outputRelative = ResolveCookedRelativePath(item.OutputRelativePath);
                prepared.Add(new PsVitaPreparedPayload {
                    ItemId = item.OutputLogicalArtifactId, OutputRelativePath = outputRelative,
                    StagedRelativePath = ResolveStagedContentRelativePath(outputRelative).Replace('\\', '/'), Bytes = ExecutePlatformCookWorkItem(item, request.NumericPolicy)
                });
            }
        } catch (Exception exception) when (exception is not OperationCanceledException) {
            string sourceIdentity = exception is FileNotFoundException missing && !string.IsNullOrWhiteSpace(missing.FileName) ? missing.FileName : request.OutputRoot;
            AddDiagnostic(diagnostics, reporter, PlatformBuildDiagnosticSeverity.Error, exception is FileNotFoundException ? "VITABUILD002" : "VITABUILD004",
                $"Vita package preparation failed: {exception.Message}", string.Empty, string.Empty, sourceIdentity);
        }
        return prepared;
    }

    /// <summary>Reads a distinct source once, rejecting aliases that would occupy another file's runtime path or be erased by staging reset.</summary>
    static void PrepareSourcePayload(string sourceIdentity, string itemId, PlatformBuildRequest request, string stagedRoot,
        List<PsVitaPreparedPayload> prepared, HashSet<string> reserved, Dictionary<string, string> sourcesByDestination, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        if (string.IsNullOrWhiteSpace(sourceIdentity)) throw new InvalidOperationException($"Vita item '{itemId}' is missing its source identity.");
        string source = ResolveSourcePath(sourceIdentity);
        ValidatePreparedSource(source, request);
        string outputRelative = ResolveCookedRelativePath(sourceIdentity);
        string stagedRelative = ResolveStagedContentRelativePath(outputRelative).Replace('\\', '/');
        if (sourcesByDestination.TryGetValue(stagedRelative, out string existingSource)) {
            if (!existingSource.Equals(source, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException($"Vita source '{sourceIdentity}' conflicts with '{existingSource}'.");
            return;
        }
        PsVitaTextureCookPath.Reserve(reserved, stagedRelative);
        PsVitaTextureCookPath.Resolve(request.OutputRoot, outputRelative);
        PsVitaTextureCookPath.Resolve(stagedRoot, stagedRelative);
        sourcesByDestination.Add(stagedRelative, source);
        prepared.Add(new PsVitaPreparedPayload {
            ItemId = itemId, OutputRelativePath = outputRelative, StagedRelativePath = stagedRelative, Bytes = ResolveStagedPayloadBytes(source)
        });
    }

    /// <summary>Rejects missing or linked input files and inputs inside either output tree that the completed batch will replace.</summary>
    static void ValidatePreparedSource(string source, PlatformBuildRequest request) {
        foreach (string root in new[] { request.OutputRoot, request.WorkingRoot }) {
            string fullRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
            if (source.Equals(fullRoot, StringComparison.OrdinalIgnoreCase) || source.StartsWith(fullRoot + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) {
                throw new InvalidOperationException("Vita cook source lies inside an output tree scheduled for replacement.");
            }
        }
        if (!File.Exists(source)) throw new FileNotFoundException("Vita package source was not found.", source);
        string current = source;
        while (!string.IsNullOrEmpty(current)) {
            if ((File.Exists(current) || Directory.Exists(current)) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw new InvalidOperationException("Vita package source crosses a filesystem link.");
            current = Path.GetDirectoryName(current);
        }
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
    /// Executes one builder-owned Vita cook work item and returns the cooked runtime payload bytes.
    /// </summary>
    /// <param name="workItem">Builder-owned Vita cook work item to execute.</param>
    /// <param name="numericPolicy">Target numeric policy encoded in the final texture container.</param>
    /// <returns>Cooked runtime payload bytes.</returns>
    byte[] ExecutePlatformCookWorkItem(PlatformCookWorkItem workItem, NumericPrecisionPolicy numericPolicy) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (!string.Equals(workItem.TargetPlatformId, Definition.PlatformId, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Unsupported Vita work item target platform '{workItem.TargetPlatformId}'.");
        }

        if (workItem.TargetArtifactKind != "runtime-texture") throw new InvalidOperationException($"Unsupported Vita work item artifact '{workItem.TargetArtifactKind}'.");
        TextureAssetProcessorSettings settings = PsVitaTextureCookSettingsSerializer.Deserialize(workItem.SerializedPlatformSettings);
        string assetId = ResolveCookWorkItemAssetId(workItem);
        if (string.Equals(workItem.SourceAssetKind, "texture", StringComparison.OrdinalIgnoreCase)) {
            using TextureAsset cookedTextureAsset = PlatformCookSourceProcessor.CookTexture(ResolveSourcePath(workItem.SourceAssetPath), assetId, settings);
            return global::helengine.files.AssetSerializer.SerializeToBytes(cookedTextureAsset, numericPolicy);
        } else if (string.Equals(workItem.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase)) {
            using TextureAsset cookedAtlasTextureAsset = PlatformCookSourceProcessor.CookFontAtlasTexture(ResolveSourcePath(workItem.SourceAssetPath), assetId, settings);
            return global::helengine.files.AssetSerializer.SerializeToBytes(cookedAtlasTextureAsset, numericPolicy);
        }

        throw new InvalidOperationException($"Unsupported Vita platform cook work item source kind '{workItem.SourceAssetKind}'.");
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
        if (string.IsNullOrWhiteSpace(sourceIdentity)) throw new InvalidOperationException("Vita source identity is required.");
        string normalizedSourceIdentity = sourceIdentity.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
        if (Path.IsPathRooted(normalizedSourceIdentity)) {
            return Path.GetFullPath(normalizedSourceIdentity);
        }

        return PsVitaTextureCookPath.Resolve(Directory.GetCurrentDirectory(), sourceIdentity);
    }

    /// <summary>
    /// Resolves the cooked relative path used by the editor output and staged content package.
    /// </summary>
    /// <param name="sourceIdentity">Source identity from the build manifest.</param>
    /// <returns>Cooked relative output path.</returns>
    static string ResolveCookedRelativePath(string sourceIdentity) {
        string normalizedSourceIdentity = Path.IsPathRooted(sourceIdentity) ? Path.GetFileName(sourceIdentity) : sourceIdentity;
        normalizedSourceIdentity = PsVitaTextureCookPath.Normalize(normalizedSourceIdentity);

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
    /// Resolves the stable asset identifier a Vita platform cook work item should preserve in the cooked payload.
    /// </summary>
    /// <param name="workItem">Cook work item whose metadata should be inspected.</param>
    /// <returns>Stable asset identifier for the cooked payload.</returns>
    static string ResolveCookWorkItemAssetId(PlatformCookWorkItem workItem) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        }

        PlatformCookWorkItemMetadata[] metadata = workItem.Metadata ?? [];
        for (int index = 0; index < metadata.Length; index++) {
            PlatformCookWorkItemMetadata entry = metadata[index];
            if (entry == null) {
                continue;
            }

            if (string.Equals(entry.Key, "source-asset-id", StringComparison.OrdinalIgnoreCase) && !string.IsNullOrWhiteSpace(entry.Value)) {
                return entry.Value;
            }
        }

        return workItem.OutputRelativePath;
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
        ParseBaseColorChannels(serializedColor, out byte red, out byte green, out byte blue, out byte alpha);
        return new float4(
            red / 255f,
            green / 255f,
            blue / 255f,
            alpha / 255f);
    }

    /// <summary>
    /// Packs one serialized base-color string into little-endian ABGR8.
    /// </summary>
    /// <param name="serializedColor">Serialized color string in <c>#RRGGBB</c> or <c>#RRGGBBAA</c> form.</param>
    /// <returns>Packed ABGR8 color value.</returns>
    static uint ParseBaseColorAbgr(string serializedColor) {
        ParseBaseColorChannels(serializedColor, out byte red, out byte green, out byte blue, out byte alpha);
        return (uint)(alpha << 24)
            | (uint)(blue << 16)
            | (uint)(green << 8)
            | red;
    }

    /// <summary>
    /// Parses one serialized base-color string into byte channels.
    /// </summary>
    /// <param name="serializedColor">Serialized color string in <c>#RRGGBB</c> or <c>#RRGGBBAA</c> form.</param>
    /// <param name="red">Resolved red channel.</param>
    /// <param name="green">Resolved green channel.</param>
    /// <param name="blue">Resolved blue channel.</param>
    /// <param name="alpha">Resolved alpha channel.</param>
    static void ParseBaseColorChannels(string serializedColor, out byte red, out byte green, out byte blue, out byte alpha) {
        if (string.IsNullOrWhiteSpace(serializedColor)) {
            red = 255;
            green = 255;
            blue = 255;
            alpha = 255;
            return;
        }

        string normalized = serializedColor.Trim();
        if (normalized.StartsWith("#", StringComparison.Ordinal)) {
            normalized = normalized.Substring(1);
        }

        if (normalized.Length != 6 && normalized.Length != 8) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.");
        }

        try {
            red = Convert.ToByte(normalized.Substring(0, 2), 16);
            green = Convert.ToByte(normalized.Substring(2, 2), 16);
            blue = Convert.ToByte(normalized.Substring(4, 2), 16);
            alpha = normalized.Length == 8
                ? Convert.ToByte(normalized.Substring(6, 2), 16)
                : (byte)255;
        } catch (FormatException ex) {
            throw new InvalidOperationException("Base color must use #RRGGBB or #RRGGBBAA.", ex);
        }
    }

    /// <summary>
    /// Resolves one required material field value or throws when the value is missing.
    /// </summary>
    /// <param name="fieldValues">Material field values supplied by the editor build graph.</param>
    /// <param name="fieldId">Field identifier that must be present.</param>
    /// <returns>Resolved nonblank field value.</returns>
    static string GetRequiredFieldValue(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id is required.", nameof(fieldId));
        }

        if (!fieldValues.TryGetValue(fieldId, out string fieldValue) || string.IsNullOrWhiteSpace(fieldValue)) {
            throw new InvalidOperationException($"PS Vita shader-backed materials require the '{fieldId}' field.");
        }

        return fieldValue;
    }

    /// <summary>
    /// Parses one positive material parameter-contract version.
    /// </summary>
    /// <param name="serializedVersion">Serialized decimal version.</param>
    /// <returns>Positive contract version.</returns>
    static uint ParseParameterContractVersion(string serializedVersion) {
        if (!uint.TryParse(serializedVersion, out uint version) || version == 0u) {
            throw new InvalidOperationException("PS Vita shader-backed materials require a positive parameter contract version.");
        }

        return version;
    }

    /// <summary>
    /// Packs one floating-point color into a 16-byte little-endian constant-buffer payload.
    /// </summary>
    /// <param name="value">Normalized color value to encode.</param>
    /// <returns>Packed constant-buffer bytes.</returns>
    static byte[] CreateFloat4ConstantBufferData(float4 value) {
        using MemoryStream stream = new();
        using EngineBinaryWriter writer = EngineBinaryWriter.Create(stream, EngineBinaryEndianness.LittleEndian);
        writer.WriteSingle((float)value.X);
        writer.WriteSingle((float)value.Y);
        writer.WriteSingle((float)value.Z);
        writer.WriteSingle((float)value.W);
        return stream.ToArray();
    }
}
