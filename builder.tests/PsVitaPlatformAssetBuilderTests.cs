using helengine.baseplatform.Definitions;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.baseplatform.Results;
using helengine.psvita.builder;
using helengine;
using Xunit;

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
        File.WriteAllText(sceneSourcePath, "scene payload");

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
            Assert.True(File.Exists(Path.Combine(outputRoot, "cooked", "scenes", "startup.hasset")));
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

}
