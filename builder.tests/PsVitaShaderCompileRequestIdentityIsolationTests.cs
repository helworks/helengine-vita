using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies Vita device-exchange helpers do not leak into the shared shader assembly generated for fixed-function platforms.
/// </summary>
public sealed class PsVitaShaderCompileRequestIdentityIsolationTests {
    /// <summary>
    /// Ensures the shared identity type only creates cross-platform cache keys and does not contain a Vita device-job hash helper.
    /// </summary>
    [Fact]
    public void SharedShaderIdentity_DoesNotContainVitaDeviceJobHashing() {
        string engineRepositoryPath = Path.GetFullPath(Path.Combine(PsVitaRepositoryPathResolver.ResolveRepositoryRoot(), "..", "helengine"));
        string identityPath = Path.Combine(engineRepositoryPath, "engine", "helengine.shader", "shaders", "compilation", "ShaderCompileRequestIdentity.cs");
        string identitySource = File.ReadAllText(identityPath);

        Assert.DoesNotContain("CreateDeviceJobHash", identitySource, StringComparison.Ordinal);
        Assert.DoesNotContain("Convert.ToHexString", identitySource, StringComparison.Ordinal);
    }
}
