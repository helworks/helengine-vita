using helengine;
using helengine.editor;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the PS Vita builder assembly can contribute a PS Vita shader backend into the shared Helengine shader pipeline.
/// </summary>
public sealed class PsVitaShaderBackendTests {
    /// <summary>
    /// Ensures the dynamically loaded PS Vita builder contributes a PS Vita shader backend into the shared registry.
    /// </summary>
    [Fact]
    public void RegisterShaderBackends_whenBuilderContributesPsVitaSupport_registersPsVitaTarget() {
        PsVitaPlatformAssetBuilder builder = new PsVitaPlatformAssetBuilder();
        ShaderBackendRegistry shaderBackendRegistry = new ShaderBackendRegistry();
        IShaderBackendRegistryContributor contributor = Assert.IsAssignableFrom<IShaderBackendRegistryContributor>(builder);

        contributor.RegisterShaderBackends(shaderBackendRegistry);

        Assert.True(shaderBackendRegistry.ContainsTarget(ShaderCompileTarget.PsVita));
    }

    /// <summary>
    /// Ensures the contributed PS Vita backend can compile the shared built-in forward solid-color shader through the shared editor shader pipeline.
    /// </summary>
    [Fact]
    public void LoadShaderAsset_whenPsVitaBackendIsRegistered_compilesForwardSolidColorShader() {
        PsVitaPlatformAssetBuilder builder = new PsVitaPlatformAssetBuilder();
        ShaderBackendRegistry shaderBackendRegistry = new ShaderBackendRegistry();
        IShaderBackendRegistryContributor contributor = Assert.IsAssignableFrom<IShaderBackendRegistryContributor>(builder);

        contributor.RegisterShaderBackends(shaderBackendRegistry);
        EditorBuiltInShaderAssetLibrary.ConfigureShaderBackends(shaderBackendRegistry);

        ShaderAsset shaderAsset = EditorBuiltInShaderAssetLibrary.LoadShaderAsset(ShaderCompileTarget.PsVita, "ForwardSolidColorShader.hlsl");

        Assert.Equal("ForwardSolidColorShader", shaderAsset.Id);
        Assert.Equal("psvita", shaderAsset.TargetName);
        Assert.Contains(shaderAsset.Binaries, binary =>
            binary.ProgramName == "ForwardSolidColorShader.vs" &&
            binary.Variant == "Mesh" &&
            binary.Bytecode.Length > 0);
        Assert.Contains(shaderAsset.Binaries, binary =>
            binary.ProgramName == "ForwardSolidColorShader.ps" &&
            binary.Variant == "Mesh" &&
            binary.Bytecode.Length > 0);
    }
}
