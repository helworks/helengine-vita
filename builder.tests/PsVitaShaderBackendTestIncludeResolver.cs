namespace helengine.psvita.builder.tests;

/// <summary>
/// Fails tests that unexpectedly attempt host-side include resolution for a self-contained Vita source fixture.
/// </summary>
public sealed class PsVitaShaderBackendTestIncludeResolver : IShaderIncludeResolver {
    /// <summary>
    /// Rejects include resolution because the fixture intentionally contains no include directives.
    /// </summary>
    /// <param name="requestingFile">Source file that requested the include.</param>
    /// <param name="includePath">Requested include path.</param>
    /// <returns>Never returns because include resolution is unexpected for this fixture.</returns>
    public ShaderIncludeResult Resolve(string requestingFile, string includePath) {
        throw new InvalidOperationException("The Vita shader backend fixture did not expect include resolution.");
    }
}
