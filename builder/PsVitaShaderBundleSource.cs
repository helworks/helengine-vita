using helengine.baseplatform.Requests;

namespace helengine.psvita.builder;

/// <summary>
/// Represents the two device compiler stages derived from one authored shader source asset.
/// </summary>
public sealed class PsVitaShaderBundleSource {
    /// <summary>
    /// Stores the original authored source identity and text.
    /// </summary>
    public PlatformShaderArtifactCookSource Source { get; }

    /// <summary>
    /// Stores the stable source index used to build compact filesystem-safe stage identifiers.
    /// </summary>
    public int SourceIndex { get; }

    /// <summary>
    /// Stores the Vita-only artifact variant represented by this pair of compiler stages.
    /// </summary>
    public string ArtifactVariantName { get; }

    /// <summary>
    /// Stores the Vita-ready vertex stage source text.
    /// </summary>
    public string VertexSourceText { get; }

    /// <summary>
    /// Stores the Vita-ready fragment stage source text.
    /// </summary>
    public string FragmentSourceText { get; }

    /// <summary>
    /// Initializes the source record used by both compiler stages.
    /// </summary>
    /// <param name="source">Original resolved authored source.</param>
    /// <param name="sourceIndex">Stable source index in the submitted batch.</param>
    /// <param name="artifactVariantName">Vita-only artifact variant represented by the two stages.</param>
    /// <param name="vertexSourceText">Vita-ready vertex source text.</param>
    /// <param name="fragmentSourceText">Vita-ready fragment source text.</param>
    public PsVitaShaderBundleSource(PlatformShaderArtifactCookSource source, int sourceIndex, string artifactVariantName, string vertexSourceText, string fragmentSourceText) {
        if (sourceIndex < 0) {
            throw new ArgumentOutOfRangeException(nameof(sourceIndex));
        }

        Source = source ?? throw new ArgumentNullException(nameof(source));
        SourceIndex = sourceIndex;
        ArtifactVariantName = string.IsNullOrWhiteSpace(artifactVariantName)
            ? throw new ArgumentException("PS Vita shader bundle artifact variants cannot be blank.", nameof(artifactVariantName))
            : artifactVariantName;
        VertexSourceText = vertexSourceText ?? throw new ArgumentNullException(nameof(vertexSourceText));
        FragmentSourceText = fragmentSourceText ?? throw new ArgumentNullException(nameof(fragmentSourceText));
    }

    /// <summary>
    /// Gets the compact vertex stage identity assigned to this source.
    /// </summary>
    public string VertexStageId => string.Concat("S", SourceIndex.ToString("D4", System.Globalization.CultureInfo.InvariantCulture), "V");

    /// <summary>
    /// Gets the compact fragment stage identity assigned to this source.
    /// </summary>
    public string FragmentStageId => string.Concat("S", SourceIndex.ToString("D4", System.Globalization.CultureInfo.InvariantCulture), "F");

}
