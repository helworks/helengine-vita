using System.Collections.ObjectModel;

namespace helengine.psvita.builder;

/// <summary>
/// Holds all device-compiled shader program pairs required by one PS Vita cooked build.
/// </summary>
public sealed class PsVitaShaderBundle {
    /// <summary>
    /// Stores immutable bundle entries in deterministic serialization order.
    /// </summary>
    readonly IReadOnlyList<PsVitaShaderBundleEntry> EntriesValue;

    /// <summary>
    /// Initializes one validated bundle of shader-program pairs.
    /// </summary>
    /// <param name="entries">Program-pair entries that must have unique material lookup keys.</param>
    public PsVitaShaderBundle(IReadOnlyList<PsVitaShaderBundleEntry> entries) {
        if (entries == null) {
            throw new ArgumentNullException(nameof(entries));
        }

        PsVitaShaderBundleEntry[] copiedEntries = new PsVitaShaderBundleEntry[entries.Count];
        HashSet<string> lookupKeys = new(StringComparer.Ordinal);
        for (int index = 0; index < entries.Count; index++) {
            PsVitaShaderBundleEntry entry = entries[index] ?? throw new ArgumentException("PS Vita shader bundles cannot contain null entries.", nameof(entries));
            if (!lookupKeys.Add(BuildLookupKey(entry))) {
                throw new ArgumentException("PS Vita shader bundles cannot contain duplicate material lookup keys.", nameof(entries));
            }

            copiedEntries[index] = entry;
        }

        EntriesValue = new ReadOnlyCollection<PsVitaShaderBundleEntry>(copiedEntries);
    }

    /// <summary>
    /// Gets immutable shader-program entries in deterministic serialization order.
    /// </summary>
    public IReadOnlyList<PsVitaShaderBundleEntry> Entries => EntriesValue;

    /// <summary>
    /// Builds the complete material lookup key for one bundle entry.
    /// </summary>
    /// <param name="entry">Entry whose material-facing key should be generated.</param>
    /// <returns>Unique internal key for the entry.</returns>
    static string BuildLookupKey(PsVitaShaderBundleEntry entry) {
        return string.Concat(entry.ShaderAssetId, "\n", entry.VertexProgramName, "\n", entry.PixelProgramName, "\n", entry.VariantName);
    }
}
