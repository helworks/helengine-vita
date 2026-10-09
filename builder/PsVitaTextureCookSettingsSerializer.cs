using System.Text.Json;
using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>Retains the generic Vita settings interface while validating platform-owned GXM selections and palette policies.</summary>
public static class PsVitaTextureCookSettingsSerializer {
    /// <summary>Creates the existing image defaults; font capabilities independently retain their uncapped RGBA32 default.</summary>
    public static TextureAssetProcessorSettings CreateDefaults() => new() {
        MaxResolution = 512,
        ColorFormatId = "Rgba32",
        AlphaPrecision = TextureAssetAlphaPrecision.A8,
        IndexingMethodId = string.Empty
    };

    /// <summary>Writes the platform-owned format id without routing it through the generic format enum.</summary>
    public static string Serialize(TextureAssetProcessorSettings settings) {
        Validate(settings);
        Dictionary<string, object> values = new() {
            ["maxResolution"] = settings.MaxResolution,
            ["colorFormat"] = settings.ColorFormatId,
            ["alphaPrecision"] = settings.AlphaPrecision.ToString()
        };
        if (!string.IsNullOrEmpty(settings.IndexingMethodId)) values["indexingMethod"] = settings.IndexingMethodId;
        return JsonSerializer.Serialize(values);
    }

    /// <summary>Rejects missing, duplicate, unknown, or unsupported settings before cooking any pixels.</summary>
    public static TextureAssetProcessorSettings Deserialize(string serializedSettings) {
        if (string.IsNullOrWhiteSpace(serializedSettings)) throw new ArgumentException("PsVita texture settings are required.", nameof(serializedSettings));
        using JsonDocument document = JsonDocument.Parse(serializedSettings);
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object) throw new InvalidOperationException("PsVita texture settings must be an object.");
        HashSet<string> keys = new(StringComparer.Ordinal);
        foreach (JsonProperty property in root.EnumerateObject()) {
            if (!keys.Add(property.Name)) throw new InvalidOperationException($"Duplicate PsVita texture setting '{property.Name}'.");
            if (property.Name != "maxResolution" && property.Name != "colorFormat" && property.Name != "alphaPrecision" && property.Name != "indexingMethod") {
                throw new InvalidOperationException($"Unknown PsVita texture setting '{property.Name}'.");
            }
        }
        if (!root.TryGetProperty("maxResolution", out JsonElement maximum) || !root.TryGetProperty("colorFormat", out JsonElement format) || !root.TryGetProperty("alphaPrecision", out JsonElement alpha)) {
            throw new InvalidOperationException("PsVita texture settings require maxResolution, colorFormat, and alphaPrecision.");
        }
        string alphaName = alpha.GetString();
        if (!Enum.TryParse(alphaName, false, out TextureAssetAlphaPrecision precision) || precision.ToString() != alphaName) {
            throw new InvalidOperationException($"Unsupported PsVita alpha precision '{alphaName}'.");
        }
        TextureAssetProcessorSettings settings = new() {
            MaxResolution = maximum.GetInt32(),
            ColorFormatId = format.GetString(),
            AlphaPrecision = precision,
            IndexingMethodId = root.TryGetProperty("indexingMethod", out JsonElement indexing) ? indexing.GetString() : string.Empty
        };
        Validate(settings);
        return settings;
    }

    /// <summary>Checks catalog format/alpha compatibility and restricts palette indexing to the implemented quantizer.</summary>
    public static void Validate(TextureAssetProcessorSettings settings) {
        ArgumentNullException.ThrowIfNull(settings);
        if (settings.MaxResolution < 0) throw new InvalidOperationException("Vita maxResolution must be zero (uncapped) or positive.");
        bool isPaletted;
        if (TextureAssetProcessorSettings.TryResolveGenericColorFormat(settings.ColorFormatId, out TextureAssetColorFormat genericFormat)) {
            if (!TextureAssetPixelCodec.IsAlphaPrecisionSupported(genericFormat, settings.AlphaPrecision)) throw new InvalidOperationException($"PsVita generic format '{genericFormat}' does not support '{settings.AlphaPrecision}' alpha.");
            isPaletted = genericFormat == TextureAssetColorFormat.Indexed4 || genericFormat == TextureAssetColorFormat.Indexed8;
        } else if (VitaNativeTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out VitaNativeTextureFormatDefinition format)) {
            if (!format.SupportsCooking) throw new InvalidOperationException($"PsVita format '{format.Id}' cannot be cooked: {format.UnsupportedReason}");
            if (!format.SupportsAlpha(settings.AlphaPrecision)) throw new InvalidOperationException($"PsVita format '{format.Id}' does not support '{settings.AlphaPrecision}' alpha.");
            isPaletted = format.IsIndexed;
        } else {
            throw new InvalidOperationException($"Unsupported PsVita texture format '{settings.ColorFormatId}'.");
        }
        string indexing = settings.IndexingMethodId ?? string.Empty;
        if (isPaletted) {
            if (indexing.Length != 0 && indexing != TextureAssetIndexingMethod.QuantizedIndexed.ToString()) throw new InvalidOperationException($"Unsupported PsVita indexing method '{indexing}'.");
        } else if (indexing.Length != 0) {
            throw new InvalidOperationException("PsVita indexingMethod is only supported for a palette format.");
        }
    }
}
