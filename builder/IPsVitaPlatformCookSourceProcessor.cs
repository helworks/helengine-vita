using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>
/// Imports builder-owned Vita source assets and converts them into final runtime payloads.
/// </summary>
public interface IPsVitaPlatformCookSourceProcessor {
    /// <summary>
    /// Imports one source texture asset and applies the resolved Vita texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved Vita texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);

    /// <summary>
    /// Imports one source font atlas asset and applies the resolved Vita atlas texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked atlas texture should preserve.</param>
    /// <param name="settings">Resolved Vita texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime atlas texture payload ready for serialization.</returns>
    TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);
}
