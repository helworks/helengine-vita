using helengine.baseplatform.Definitions;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.baseplatform.Results;
using helengine.psvita.builder;
using Xunit;
using System.Text;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the PS Vita builder metadata and editor build execution behavior.
/// </summary>
public sealed class PsVitaPlatformAssetBuilderTests {
    /// <summary>
    /// Verifies the builder exposes the PS Vita profiles required for editor discovery.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_psvita_metadata() {
        PsVitaPlatformAssetBuilder builder = new();

        Assert.Equal("helengine.psvita.builder", builder.Descriptor.BuilderId);
        Assert.Equal("psvita", builder.Descriptor.TargetPlatformId);
        Assert.Equal("psvita", builder.Definition.PlatformId);
        Assert.Equal("PS Vita", builder.Definition.DisplayName);
        Assert.Contains(builder.Definition.BuildProfiles, profile => profile.ProfileId == "debug");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "psvita-forward");
        Assert.Contains(builder.Definition.StorageProfiles, profile =>
            profile.ProfileId == "vpk-package" &&
            profile.RuntimeSpecializationId == "psvita-vpk-package");
        Assert.Contains(builder.Definition.MediaProfiles, profile =>
            profile.ProfileId == "psvita-memory-card" &&
            profile.LayoutKind == PlatformMediaLayoutKind.InstallTree);
        Assert.Contains(builder.Definition.ComponentSupportRules, supportRule =>
            supportRule.ComponentTypeId == "helengine.meshcomponent" &&
            supportRule.SupportKind == PlatformComponentSupportKind.Transform);
        Assert.Contains(builder.Definition.AssetCookCapabilities, capability =>
            capability.SourceAssetKind == "texture" &&
            capability.TargetArtifactKind == "runtime-texture" &&
            capability.OwnershipKind == PlatformAssetCookOwnershipKind.BuilderOwned &&
            capability.SettingsContractId == "psvita-texture");
        Assert.Contains(builder.Definition.AssetCookCapabilities, capability =>
            capability.SourceAssetKind == "font-atlas-texture" &&
            capability.TargetArtifactKind == "runtime-texture" &&
            capability.OwnershipKind == PlatformAssetCookOwnershipKind.BuilderOwned &&
            capability.SettingsContractId == "psvita-font-atlas-texture" &&
            capability.OutputFileExtension == ".hetex");

        string platformDefinitionFactoryPath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaPlatformDefinitionFactory.cs");
        string platformDefinitionFactorySource = File.ReadAllText(platformDefinitionFactoryPath);
        Assert.Contains("\"generated-math-convention\"", platformDefinitionFactorySource, StringComparison.Ordinal);
        Assert.Contains("\"pointer-size-bytes\"", platformDefinitionFactorySource, StringComparison.Ordinal);
        Assert.Contains("\"type-remaps\"", platformDefinitionFactorySource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the builder executes the native PS Vita build and copies the produced VPK into the output root.
    /// </summary>
    [Fact]
    public async Task BuildAsync_runs_native_build_and_copies_vpk_to_output_root() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psvita-builder-tests-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "project");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRoot, "scenes", "startup.hasset");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath));
        Directory.CreateDirectory(generatedCoreRoot);
        File.WriteAllBytes(sceneSourcePath, CreateCookedSceneBytes());

        string previousDirectory = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(sourceRoot);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "psvita",
                "1.0.0",
                "startup",
                [
                    new PlatformBuildScene(
                        "startup",
                        "Startup",
                        "scenes/startup.hasset",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "scenes/startup.hasset")])
                ],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan(string.Empty, []));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
                [new PlatformCookProfile(
                    "debug",
                    "Debug",
                    new PlatformCookProfileCapabilities(
                        "psvita",
                        "raw",
                        "rgba",
                        "psvita-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "tmp"),
                "debug",
                "psvita-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psvita-memory-card",
                "vpk-package");

            RecordingPsVitaNativeBuildExecutor nativeBuildExecutor = new();
            PsVitaPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.True(nativeBuildExecutor.WasCalled);
            Assert.Equal(
                PsVitaRepositoryPathResolver.ResolveRepositoryRoot(),
                nativeBuildExecutor.RepositoryRootPath);
            Assert.True(File.Exists(Path.Combine(outputRoot, "cooked", "scenes", "startup.hasset")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.StagedContentRootPath, "scenes", "startup.hasset")));
            Assert.False(File.Exists(Path.Combine(nativeBuildExecutor.StagedContentRootPath, "cooked", "scenes", "startup.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "helengine_psvita.vpk")));
            Assert.True(progressReporter.Updates.Count >= 2);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Verifies the builder stages cooked payload references supplied by the editor build graph so imported runtime dependencies reach the final VPK.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenSceneCarriesPayloadReferences_stagesImportedCookedDependencies() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psvita-builder-tests-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "project");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "startup.hasset");
        string importedTexturePath = Path.Combine(sourceRoot, "cooked", "imported", "menu.tex");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath));
        Directory.CreateDirectory(Path.GetDirectoryName(importedTexturePath));
        Directory.CreateDirectory(generatedCoreRoot);
        File.WriteAllBytes(sceneSourcePath, CreateCookedSceneBytes());
        File.WriteAllBytes(importedTexturePath, [1, 2, 3, 4]);

        string previousDirectory = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(sourceRoot);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "psvita",
                "1.0.0",
                "startup",
                [
                    new PlatformBuildScene(
                        "startup",
                        "Startup",
                        "cooked/scenes/startup.hasset",
                        [
                            new PlatformBuildPayloadReference("cooked/scenes/startup.hasset", "cooked/scenes/startup.hasset"),
                            new PlatformBuildPayloadReference("cooked/imported/menu.tex", "cooked/imported/menu.tex")
                        ],
                        [new KeyValuePair<string, string>("cooked-relative-path", "scenes/startup.hasset")])
                ],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan(string.Empty, []));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
                [new PlatformCookProfile(
                    "debug",
                    "Debug",
                    new PlatformCookProfileCapabilities(
                        "psvita",
                        "raw",
                        "rgba",
                        "psvita-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "tmp"),
                "debug",
                "psvita-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psvita-memory-card",
                "vpk-package");

            RecordingPsVitaNativeBuildExecutor nativeBuildExecutor = new();
            PsVitaPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.True(File.Exists(Path.Combine(outputRoot, "cooked", "scenes", "startup.hasset")));
            Assert.True(File.Exists(Path.Combine(outputRoot, "cooked", "imported", "menu.tex")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.StagedContentRootPath, "scenes", "startup.hasset")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.StagedContentRootPath, "imported", "menu.tex")));
            Assert.True(progressReporter.Updates.Count >= 2);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Verifies the builder emits fallback generated runtime deserializer support for menu-scene component types when the workspace core does not already contain it.
    /// </summary>
    [Fact]
    public async Task BuildAsync_when_generated_runtime_deserializer_support_is_missing_emits_menu_runtime_support_sources() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psvita-builder-tests-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "project");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRoot, "scenes", "startup.hasset");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath));
        Directory.CreateDirectory(generatedCoreRoot);
        File.WriteAllBytes(
            sceneSourcePath,
            CreateCookedSceneBytes(
                "helengine.SpriteComponent",
                "helengine.TextComponent",
                "city.menu.MenuComponent, gameplay"));

        string previousDirectory = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(sourceRoot);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "psvita",
                "1.0.0",
                "startup",
                [
                    new PlatformBuildScene(
                        "startup",
                        "Startup",
                        "scenes/startup.hasset",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "scenes/startup.hasset")])
                ],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan(string.Empty, []));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
                [new PlatformCookProfile(
                    "debug",
                    "Debug",
                    new PlatformCookProfileCapabilities(
                        "psvita",
                        "raw",
                        "rgba",
                        "psvita-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "tmp"),
                "debug",
                "psvita-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psvita-memory-card",
                "vpk-package");

            RecordingPsVitaNativeBuildExecutor nativeBuildExecutor = new();
            PsVitaPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.True(nativeBuildExecutor.WasCalled);
            Assert.Equal(generatedCoreRoot, nativeBuildExecutor.GeneratedCoreRootPath);
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeComponentDeserializerRegistration.hpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeComponentDeserializerRegistration.cpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeSpriteComponentDeserializer.hpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeSpriteComponentDeserializer.cpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeTextComponentDeserializer.hpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "GeneratedRuntimeTextComponentDeserializer.cpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "PsVitaUnsupportedRuntimeComponent.hpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "PsVitaUnsupportedRuntimeComponent.cpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "PsVitaUnsupportedRuntimeComponentDeserializer.hpp")));
            Assert.True(File.Exists(Path.Combine(generatedCoreRoot, "PsVitaUnsupportedRuntimeComponentDeserializer.cpp")));
            Assert.Contains(
                "city.menu.MenuComponent, gameplay",
                File.ReadAllText(Path.Combine(generatedCoreRoot, "GeneratedRuntimeComponentDeserializerRegistration.cpp"), Encoding.UTF8),
                StringComparison.Ordinal);
            Assert.Contains(
                "#include \"GeneratedRuntimeSpriteComponentDeserializer.cpp\"",
                File.ReadAllText(Path.Combine(generatedCoreRoot, "helengine_core_unity.cpp"), Encoding.UTF8),
                StringComparison.Ordinal);
            Assert.Contains(
                "#include \"PsVitaUnsupportedRuntimeComponentDeserializer.cpp\"",
                File.ReadAllText(Path.Combine(generatedCoreRoot, "helengine_core_unity.cpp"), Encoding.UTF8),
                StringComparison.Ordinal);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Verifies the PS Vita standard material cook path preserves authored color and texture references.
    /// </summary>
    [Fact]
    public void CookMaterial_preserves_standard_material_fields() {
        PsVitaPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "Materials/Test.helmat",
            "Materials/Test.helmat",
            "psvita",
            "debug",
            "psvita-forward",
            "standard-shader",
            new Dictionary<string, string> {
                ["base-color"] = "#336699",
                ["texture-id"] = "Textures/Checker"
            }));

        ShaderMaterialAsset materialAsset = Assert.IsType<ShaderMaterialAsset>(global::helengine.files.AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        Assert.Equal("Textures/Checker", materialAsset.DiffuseTextureAssetId);
        MaterialConstantBufferAsset baseColorBuffer = Assert.Single(materialAsset.ConstantBuffers);
        Assert.Equal("BaseColorBuffer", baseColorBuffer.Name);
        Assert.Equal(16, baseColorBuffer.Data.Length);
        Assert.Empty(result.ReferencedShaderAssetIds);
    }

    /// <summary>
    /// Verifies the PS Vita builder rewrites shader-backed materials into one Vita-native cooked payload that keeps the referenced shader id.
    /// </summary>
    [Fact]
    public void CookMaterial_whenShaderFieldsArePresent_returnsReferencedShaderAssetId() {
        PsVitaPlatformAssetBuilder builder = new();

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "materials/rendering/cube_test_solid.hasset",
            "materials/rendering/cube_test_solid.hasset",
            "psvita",
            "debug",
            "default",
            "standard-shader",
            new Dictionary<string, string> {
                ["shader-asset-id"] = "ForwardSolidColorShader",
                ["vertex-program"] = "ForwardSolidColorShader.vs",
                ["pixel-program"] = "ForwardSolidColorShader.ps",
                ["variant"] = "Mesh",
                ["base-color"] = "#ffffffff"
            }));

        PsVitaCompiledShaderMaterialAsset materialAsset = new PsVitaCompiledShaderMaterialBinarySerializer().Deserialize(result.CookedMaterialBytes);
        Assert.Equal("ForwardSolidColorShader", materialAsset.ShaderAssetId);
        Assert.Equal("ForwardSolidColorShader.vs", materialAsset.VertexProgramName);
        Assert.Equal("ForwardSolidColorShader.ps", materialAsset.PixelProgramName);
        Assert.Equal("Mesh", materialAsset.VariantName);
        Assert.Equal(0xFFFFFFFFu, materialAsset.BaseColorAbgr);
        Assert.Equal(["ForwardSolidColorShader"], result.ReferencedShaderAssetIds);
    }

    /// <summary>
    /// Verifies the Vita staging path rewrites generic cooked model payloads into the packed PS Vita model format before they reach the packaged content roots.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenStagingModelPayloads_rewritesCookedModelIntoPackedVitaPayload() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psvita-builder-tests-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "project");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "startup.hasset");
        string modelSourcePath = Path.Combine(sourceRoot, "cooked", "models", "cube.hasset");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath));
        Directory.CreateDirectory(Path.GetDirectoryName(modelSourcePath));
        Directory.CreateDirectory(generatedCoreRoot);
        File.WriteAllBytes(sceneSourcePath, CreateCookedSceneBytes());
        File.WriteAllBytes(modelSourcePath, CreateCookedModelBytes());

        string previousDirectory = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(sourceRoot);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "psvita",
                "1.0.0",
                "startup",
                [
                    new PlatformBuildScene(
                        "startup",
                        "Startup",
                        "cooked/scenes/startup.hasset",
                        [
                            new PlatformBuildPayloadReference("cooked/scenes/startup.hasset", "cooked/scenes/startup.hasset"),
                            new PlatformBuildPayloadReference("cooked/models/cube.hasset", "cooked/models/cube.hasset")
                        ],
                        [new KeyValuePair<string, string>("cooked-relative-path", "scenes/startup.hasset")])
                ],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan(string.Empty, []));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
                [new PlatformCookProfile(
                    "debug",
                    "Debug",
                    new PlatformCookProfileCapabilities(
                        "psvita",
                        "raw",
                        "rgba",
                        "psvita-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "tmp"),
                "debug",
                "psvita-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psvita-memory-card",
                "vpk-package");

            RecordingPsVitaNativeBuildExecutor nativeBuildExecutor = new();
            PsVitaPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);

            string outputModelPath = Path.Combine(outputRoot, "cooked", "models", "cube.hasset");
            string stagedModelPath = Path.Combine(nativeBuildExecutor.StagedContentRootPath, "models", "cube.hasset");
            Assert.True(File.Exists(outputModelPath));
            Assert.True(File.Exists(stagedModelPath));

            byte[] outputModelBytes = File.ReadAllBytes(outputModelPath);
            byte[] stagedModelBytes = File.ReadAllBytes(stagedModelPath);
            Assert.True(PsVitaPackedModelAssetBinarySerializer.HasMagicPrefix(outputModelBytes));
            Assert.True(PsVitaPackedModelAssetBinarySerializer.HasMagicPrefix(stagedModelBytes));

            PsVitaPackedModelAsset packedModel = PsVitaPackedModelAssetBinarySerializer.DeserializeFromBytes(outputModelBytes);
            Assert.Equal(2, packedModel.IndexElementSizeBytes);
            PsVitaPackedModelSubmesh packedSubmesh = Assert.Single(packedModel.Submeshes);
            Assert.Equal(0, packedSubmesh.IndexStart);
            Assert.Equal(3, packedSubmesh.IndexCount);
            Assert.Equal([0u, 1u, 2u], packedSubmesh.TriangleIndices);
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Verifies the Vita builder executes one builder-owned font-atlas cook work item and writes the cooked atlas beside the packaged font.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenManifestCarriesFontAtlasCookWorkItem_emits_external_cooked_font_atlas() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-psvita-builder-tests-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string sourceRoot = Path.Combine(workingRoot, "project");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sceneSourcePath = Path.Combine(sourceRoot, "cooked", "scenes", "startup.hasset");
        string sourceFontPath = Path.Combine(sourceRoot, "fonts", "default.hefont");

        Directory.CreateDirectory(Path.GetDirectoryName(sceneSourcePath));
        Directory.CreateDirectory(Path.GetDirectoryName(sourceFontPath));
        Directory.CreateDirectory(generatedCoreRoot);
        File.WriteAllBytes(sceneSourcePath, CreateCookedSceneBytes());
        File.WriteAllBytes(sourceFontPath, CreateCookedFontBytes());

        string previousDirectory = Directory.GetCurrentDirectory();
        try {
            Directory.SetCurrentDirectory(sourceRoot);

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "psvita",
                "1.0.0",
                "startup",
                [
                    new PlatformBuildScene(
                        "startup",
                        "Startup",
                        "cooked/scenes/startup.hasset",
                        [],
                        [new KeyValuePair<string, string>("cooked-relative-path", "scenes/startup.hasset")])
                ],
                [],
                [],
                [],
                [],
                new PlatformContainerWritePlan(string.Empty, []),
                [
                    new PlatformCookWorkItem(
                        "psvita:font-atlas-texture:cooked/fonts/default.hetex",
                        sourceFontPath,
                        "font-atlas-texture",
                        "psvita",
                        "runtime-texture",
                        "cooked/fonts/default.hetex",
                        "texture:cooked/fonts/default.hetex",
                        "sha256:source",
                        "sha256:settings",
                        "{\"maxResolution\":128,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\"}",
                        [
                            new PlatformCookWorkItemMetadata("source-asset-id", "fonts/default.hefont")
                        ])
                ]);

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
                [new PlatformCookProfile(
                    "debug",
                    "Debug",
                    new PlatformCookProfileCapabilities(
                        "psvita",
                        "raw",
                        "rgba",
                        "psvita-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                Path.Combine(workingRoot, "tmp"),
                "debug",
                "psvita-forward",
                "default",
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                new Dictionary<string, string>(),
                generatedCoreRoot,
                "psvita-memory-card",
                "vpk-package");

            RecordingPsVitaNativeBuildExecutor nativeBuildExecutor = new();
            PsVitaPlatformAssetBuilder builder = new(nativeBuildExecutor);
            RecordingProgressReporter progressReporter = new();
            RecordingDiagnosticReporter diagnosticReporter = new();

            PlatformBuildReport report = await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.Empty(diagnosticReporter.Diagnostics);
            Assert.True(File.Exists(Path.Combine(outputRoot, "cooked", "fonts", "default.hetex")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.StagedContentRootPath, "fonts", "default.hetex")));
        } finally {
            Directory.SetCurrentDirectory(previousDirectory);
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Creates one minimal cooked scene payload that contains the supplied serialized component type identifiers.
    /// </summary>
    /// <param name="componentTypeIds">Serialized component type identifiers to include beneath one root entity.</param>
    /// <returns>Cooked scene bytes suitable for builder-side component-type discovery.</returns>
    static byte[] CreateCookedSceneBytes(params string[] componentTypeIds) {
        if (componentTypeIds == null) {
            throw new ArgumentNullException(nameof(componentTypeIds));
        }

        SceneComponentAssetRecord[] componentRecords = new SceneComponentAssetRecord[componentTypeIds.Length];
        for (int componentIndex = 0; componentIndex < componentTypeIds.Length; componentIndex++) {
            componentRecords[componentIndex] = new SceneComponentAssetRecord {
                ComponentTypeId = componentTypeIds[componentIndex],
                Payload = Array.Empty<byte>()
            };
        }

        SceneAsset sceneAsset = new SceneAsset {
            RootEntities = [
                new SceneEntityAsset {
                    Components = componentRecords
                }
            ]
        };
        return global::helengine.files.AssetSerializer.SerializeToBytes(sceneAsset);
    }

    /// <summary>
    /// Creates one minimal cooked model payload that forces the Vita builder to resolve default submeshes and collapse 32-bit indices into the packed format.
    /// </summary>
    /// <returns>Cooked model bytes suitable for Vita staging tests.</returns>
    static byte[] CreateCookedModelBytes() {
        ModelAsset modelAsset = new() {
            Positions = [
                new float3(0f, 0f, 0f),
                new float3(1f, 0f, 0f),
                new float3(0f, 1f, 0f)
            ],
            BoundsMin = new float3(0f, 0f, 0f),
            BoundsMax = new float3(1f, 1f, 0f),
            Indices32 = [0u, 1u, 2u],
            Submeshes = []
        };

        return global::helengine.files.AssetSerializer.SerializeToBytes(modelAsset);
    }

    /// <summary>
    /// Creates one packaged font payload with an embedded source atlas suitable for builder-owned font-atlas cook tests.
    /// </summary>
    /// <returns>Packaged font bytes whose source atlas can be externalized by the Vita builder.</returns>
    static byte[] CreateCookedFontBytes() {
        TextureAsset textureAsset = new() {
            Id = "fonts/default.hefont#atlas",
            RuntimeAssetId = RuntimeAssetIdGenerator.Generate("fonts/default.hefont#atlas"),
            Width = 1,
            Height = 1,
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8,
            PaletteColors = Array.Empty<byte>(),
            Colors = [255, 255, 255, 255]
        };
        FontAsset fontAsset = new(
            new FontInfo("Default", 16, 4f),
            null,
            new Dictionary<char, FontChar>(),
            16f,
            1,
            1) {
                SourceTextureAsset = textureAsset,
                CookedAtlasTextureRelativePath = string.Empty
        };

        using MemoryStream stream = new MemoryStream();
        global::helengine.files.FontAssetBinarySerializer.Serialize(stream, fontAsset);
        return stream.ToArray();
    }

}
