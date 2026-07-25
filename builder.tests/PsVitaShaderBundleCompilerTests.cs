using System.Security.Cryptography;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the host-side batch compiler that converts one Vita device job into the runtime shader bundle.
/// </summary>
public sealed class PsVitaShaderBundleCompilerTests {
    /// <summary>
    /// Verifies that dependencies sharing one authored shader source compile as one two-stage device job and emit one lookup entry for each material program pair.
    /// </summary>
    [Fact]
    public void Cook_sharedShaderSource_writesOneBundleWithEveryMaterialLookupEntry() {
        string cookRootPath = Path.Combine(Path.GetTempPath(), "helengine-psvita-shader-bundle-tests", Guid.NewGuid().ToString("N"));
        PsVitaShaderBundleCompilerTestExchange exchange = new();
        PlatformShaderArtifactCookRequest request = PlatformShaderArtifactCookRequest.CreateWithDependenciesAndSources(
            cookRootPath,
            "psvita",
            "release",
            "gxm",
            [
                new PlatformShaderDependency("game/surface", "VS", "PS", "Opaque"),
                new PlatformShaderDependency("game/surface", "VS", "PS", "Cutout")
            ],
            [new PlatformShaderArtifactCookSource("game/surface", Convert.ToHexString(SHA256.HashData("shader-source"u8)), "float4 VS() : POSITION { return 0; }\nfloat4 PS() : COLOR { return 1; }")]);

        PlatformShaderArtifactCookResult result = new PsVitaShaderBundleCompiler(exchange).Cook(request);

        string bundlePath = Path.Combine(cookRootPath, "shaders", "psvita", "shaders.psvb");
        Assert.Single(result.CookedArtifactDeclarations);
        Assert.Equal("cooked/shaders/psvita/shaders.psvb", result.CookedArtifactDeclarations[0].RelativePath);
        Assert.Equal(2, exchange.SubmittedJob.Stages.Count);
        Assert.Equal(2, exchange.SubmittedSourceFiles.Count);
        Assert.True(File.Exists(bundlePath));

        PsVitaShaderBundle bundle = new PsVitaShaderBundleBinarySerializer().Deserialize(File.ReadAllBytes(bundlePath));
        Assert.Equal(2, bundle.Entries.Count);
        Assert.All(bundle.Entries, entry => {
            Assert.Equal("game/surface", entry.ShaderAssetId);
            Assert.Equal("VS", entry.VertexProgramName);
            Assert.Equal("PS", entry.PixelProgramName);
            Assert.NotEmpty(entry.VertexArtifactBytes);
            Assert.NotEmpty(entry.FragmentArtifactBytes);
        });
    }

    /// <summary>
    /// Verifies a legacy Lambert shader identity is submitted as its own source rather than being silently remapped to Standard Shader Cg.
    /// </summary>
    [Fact]
    public void Cook_legacyForwardLambertShader_doesNotUseTheStandardShaderLowering() {
        string cookRootPath = Path.Combine(Path.GetTempPath(), "helengine-psvita-shader-bundle-tests", Guid.NewGuid().ToString("N"));
        PsVitaShaderBundleCompilerTestExchange exchange = new();
        PlatformShaderArtifactCookRequest request = PlatformShaderArtifactCookRequest.CreateWithDependenciesAndSources(
            cookRootPath,
            "psvita",
            "debug",
            "gxm",
            [new PlatformShaderDependency("ForwardLambertShader", "ForwardLambertShader.vs", "ForwardLambertShader.ps", "ForwardLambertOpaque")],
            [new PlatformShaderArtifactCookSource("ForwardLambertShader", Convert.ToHexString(SHA256.HashData("legacy-lambert"u8)), "cbuffer TransformBuffer { float4x4 worldViewProj; }")]);

        new PsVitaShaderBundleCompiler(exchange).Cook(request);

        Assert.Equal(2, exchange.SubmittedSourceFiles.Count);
        Assert.All(exchange.SubmittedSourceFiles, source => Assert.Contains("cbuffer TransformBuffer", source.SourceText, StringComparison.Ordinal));
        Assert.All(exchange.SubmittedSourceFiles, source => Assert.DoesNotContain("HelengineDiffuseTexture", source.SourceText, StringComparison.Ordinal));
    }

    /// <summary>
    /// Verifies that one Standard Shader source produces independent unshadowed, shadow-receiving, and depth-only artifact pairs.
    /// </summary>
    [Fact]
    public void Cook_forwardStandardShader_writesAllRuntimeShadowVariants() {
        string cookRootPath = Path.Combine(Path.GetTempPath(), "helengine-psvita-shader-bundle-tests", Guid.NewGuid().ToString("N"));
        PsVitaShaderBundleCompilerTestExchange exchange = new();
        PlatformShaderArtifactCookRequest request = PlatformShaderArtifactCookRequest.CreateWithDependenciesAndSources(
            cookRootPath,
            "psvita",
            "release",
            "gxm",
            [new PlatformShaderDependency("ForwardStandardShader", "ForwardStandardShader.vs", "ForwardStandardShader.ps", "ForwardStandardTextured")],
            [new PlatformShaderArtifactCookSource("ForwardStandardShader", Convert.ToHexString(SHA256.HashData("standard-source"u8)), "shared Standard Shader source")]);

        new PsVitaShaderBundleCompiler(exchange).Cook(request);

        string bundlePath = Path.Combine(cookRootPath, "shaders", "psvita", "shaders.psvb");
        PsVitaShaderBundle bundle = new PsVitaShaderBundleBinarySerializer().Deserialize(File.ReadAllBytes(bundlePath));

        Assert.Equal(6, exchange.SubmittedJob.Stages.Count);
        Assert.Equal(6, exchange.SubmittedSourceFiles.Count);
        Assert.Equal(3, bundle.Entries.Count);
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ForwardStandardTextured");
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ForwardStandardShadowed");
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ShadowDepth");
    }

    /// <summary>
    /// Ensures the Vita bundle preserves the material-selected unshadowed variant alongside the shared shadow variants.
    /// </summary>
    [Fact]
    public void Cook_forwardStandardShader_preservesTheMaterialVariantAlongsideShadowVariants() {
        string cookRootPath = Path.Combine(Path.GetTempPath(), "helengine-psvita-shader-bundle-tests", Guid.NewGuid().ToString("N"));
        PsVitaShaderBundleCompilerTestExchange exchange = new();
        PlatformShaderArtifactCookRequest request = PlatformShaderArtifactCookRequest.CreateWithDependenciesAndSources(
            cookRootPath,
            "psvita",
            "release",
            "gxm",
            [new PlatformShaderDependency("ForwardStandardShader", "ForwardStandardShader.vs", "ForwardStandardShader.ps", "ForwardStandardTextured")],
            [new PlatformShaderArtifactCookSource("ForwardStandardShader", Convert.ToHexString(SHA256.HashData("standard-source"u8)), "shared Standard Shader source")]);

        new PsVitaShaderBundleCompiler(exchange).Cook(request);

        string bundlePath = Path.Combine(cookRootPath, "shaders", "psvita", "shaders.psvb");
        PsVitaShaderBundle bundle = new PsVitaShaderBundleBinarySerializer().Deserialize(File.ReadAllBytes(bundlePath));

        Assert.Equal(StandardShaderVariants.All.Count, bundle.Entries.Count);
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ForwardStandardTextured");
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ForwardStandardShadowed");
        Assert.Contains(bundle.Entries, entry => entry.VariantName == "ShadowDepth");
    }
}
