using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies registration of the Vita shader backend does not impose a device-transfer configuration on unrelated builds.
/// </summary>
public sealed class PsVitaShaderBackendRegistrationTests {
    /// <summary>
    /// Ensures registering the Vita contributor remains configuration-free until a Vita shader is actually compiled.
    /// </summary>
    [Fact]
    public void RegisterShaderBackends_WhenExchangeRootIsUnset_RegistersTheBackendWithoutThrowing() {
        string previousValue = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT");
        try {
            Environment.SetEnvironmentVariable("HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT", null);
            ShaderBackendRegistry registry = new();

            new PsVitaPlatformAssetBuilder().RegisterShaderBackends(registry);

            Assert.True(registry.ContainsTarget(ShaderCompileTarget.PsVita));
        } finally {
            Environment.SetEnvironmentVariable("HELENGINE_PSVITA_SHADER_COMPILER_EXCHANGE_ROOT", previousValue);
        }
    }
}
