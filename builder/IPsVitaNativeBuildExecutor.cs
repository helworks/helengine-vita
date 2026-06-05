namespace helengine.psvita.builder;

/// <summary>
/// Defines the native PS Vita build boundary used by the editor-facing builder.
/// </summary>
public interface IPsVitaNativeBuildExecutor {
    /// <summary>
    /// Builds the native PS Vita player and returns the produced VPK path.
    /// </summary>
    /// <param name="repositoryRoot">Absolute PS Vita repository root.</param>
    /// <param name="nativeBuildRoot">Absolute scratch directory for native build artifacts.</param>
    /// <param name="generatedCoreCppRootPath">Absolute generated core C++ root supplied by the editor.</param>
    /// <param name="stagedContentRootPath">Absolute staged cooked-content root supplied by the builder.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build.</param>
    /// <returns>Absolute path to the produced VPK.</returns>
    string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath, CancellationToken cancellationToken);
}
