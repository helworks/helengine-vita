using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>Resizes a private RGBA copy before encoding the requested GXM texture payload.</summary>
public sealed class PsVitaTextureCooker {
    /// <summary>Shares the editor's aspect-preserving resize implementation with platform cooks.</summary>
    readonly TextureAssetProcessor Processor = new();

    /// <summary>Produces a new native texture while retaining authored identity and leaving source bytes intact.</summary>
    public TextureAsset CookTexture(TextureAsset source, TextureAssetProcessorSettings settings) {
        ArgumentNullException.ThrowIfNull(source);
        PsVitaTextureCookSettingsSerializer.Validate(settings);
        TextureAsset rgba = new() {
            Width = source.Width, Height = source.Height,
            ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8,
            Colors = TextureAssetPixelCodec.DecodeToRgba32(source), PaletteColors = Array.Empty<byte>()
        };
        CopyIdentity(source, rgba);
        TextureAsset resized = null;
        TextureAsset indexed = null;
        TextureAsset result = null;
        try {
            resized = Processor.Apply(rgba, new TextureAssetProcessorSettings {
                MaxResolution = settings.MaxResolution, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8
            });
            if (resized.Width > 4096 || resized.Height > 4096) throw new InvalidOperationException("Vita cooked texture dimensions cannot exceed 4096 pixels after resizing.");

            if (TextureAssetProcessorSettings.TryResolveGenericColorFormat(settings.ColorFormatId, out TextureAssetColorFormat genericFormat)) {
                result = Processor.Apply(resized, new TextureAssetProcessorSettings {
                    MaxResolution = 0, ColorFormat = genericFormat, AlphaPrecision = settings.AlphaPrecision, IndexingMethodId = settings.IndexingMethodId
                });
                CopyIdentity(source, result);
                return result;
            }
            VitaNativeTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out VitaNativeTextureFormatDefinition format);
            TextureAsset encodingSource = resized;
            if (format.IsIndexed) {
                indexed = Processor.Apply(resized, new TextureAssetProcessorSettings {
                    MaxResolution = 0, ColorFormat = format.IndexBitDepth == 4 ? TextureAssetColorFormat.Indexed4 : TextureAssetColorFormat.Indexed8,
                    AlphaPrecision = TextureAssetAlphaPrecision.A8, IndexingMethodId = settings.IndexingMethodId
                });
                CopyIdentity(source, indexed);
                encodingSource = indexed;
            }
            result = VitaNativeTextureCodec.Encode(encodingSource, settings.ColorFormatId, settings.AlphaPrecision);
            CopyIdentity(source, result);
            return result;
        } finally {
            indexed?.Dispose();
            if (resized != null && !ReferenceEquals(resized, rgba) && !ReferenceEquals(resized, result)) resized.Dispose();
            if (!ReferenceEquals(rgba, result)) rgba.Dispose();
        }
    }

    /// <summary>Copies identity metadata while keeping the mutable alias-array container independent.</summary>
    internal static void CopyIdentity(TextureAsset source, TextureAsset destination) {
        destination.Id = source.Id;
        destination.RuntimeAssetId = source.RuntimeAssetId;
        destination.AuthoringAssetId = source.AuthoringAssetId;
        destination.FormerAuthoringAssetIds = source.FormerAuthoringAssetIds == null ? null : (string[])source.FormerAuthoringAssetIds.Clone();
        destination.IsEngineOwned = source.IsEngineOwned;
    }
}
