namespace helengine.psvita.builder;

/// <summary>
/// Registers the PS Vita shader backend into the shared Helengine shader backend registry.
/// </summary>
public sealed class PsVitaShaderBackendRegistration {
    /// <summary>
    /// Stores the PS Vita shader backend contributed by the PS Vita builder assembly.
    /// </summary>
    readonly IShaderBackend ShaderBackend;

    /// <summary>
    /// Initializes one PS Vita shader backend registration.
    /// </summary>
    public PsVitaShaderBackendRegistration()
        : this(new PsVitaShaderBackend()) {
    }

    /// <summary>
    /// Initializes one PS Vita shader backend registration with an explicit backend instance.
    /// </summary>
    /// <param name="shaderBackend">PS Vita shader backend contributed by the builder assembly.</param>
    internal PsVitaShaderBackendRegistration(IShaderBackend shaderBackend) {
        ShaderBackend = shaderBackend ?? throw new ArgumentNullException(nameof(shaderBackend));
    }

    /// <summary>
    /// Registers the PS Vita shader backend into the supplied shared registry.
    /// </summary>
    /// <param name="shaderBackendRegistry">Shared registry that should receive the PS Vita shader backend.</param>
    public void Register(ShaderBackendRegistry shaderBackendRegistry) {
        if (shaderBackendRegistry == null) {
            throw new ArgumentNullException(nameof(shaderBackendRegistry));
        }

        shaderBackendRegistry.Register(ShaderBackend);
    }
}
