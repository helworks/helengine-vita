namespace helengine.psvita.builder;

/// <summary>
/// Holds the Vita Cg vertex and fragment source generated for one shared Standard Shader variant.
/// </summary>
public sealed class PsVitaForwardStandardSourcePair {
    /// <summary>
    /// Initializes one complete Vita Cg source pair.
    /// </summary>
    /// <param name="vertexSourceText">Vertex-stage Cg source.</param>
    /// <param name="fragmentSourceText">Fragment-stage Cg source.</param>
    public PsVitaForwardStandardSourcePair(string vertexSourceText, string fragmentSourceText) {
        VertexSourceText = string.IsNullOrWhiteSpace(vertexSourceText)
            ? throw new ArgumentException("Vita Standard Shader vertex source cannot be blank.", nameof(vertexSourceText))
            : vertexSourceText;
        FragmentSourceText = string.IsNullOrWhiteSpace(fragmentSourceText)
            ? throw new ArgumentException("Vita Standard Shader fragment source cannot be blank.", nameof(fragmentSourceText))
            : fragmentSourceText;
    }

    /// <summary>
    /// Gets the generated vertex-stage Cg source.
    /// </summary>
    public string VertexSourceText { get; }

    /// <summary>
    /// Gets the generated fragment-stage Cg source.
    /// </summary>
    public string FragmentSourceText { get; }
}
