using helengine;
using helengine.editor;
using Xunit;
using FilesAssetSerializer = helengine.files.AssetSerializer;

namespace helengine.psvita.builder.tests;

/// <summary>Checks every image-encodable GXM selector through the real Vita cooker, settings and serialized texture interfaces.</summary>
public sealed class PsVitaNativeTextureCookTests {
    /// <summary>Supplies each SDK selector with a real image encoder, including aliases and palette swizzles.</summary>
    public static IEnumerable<object[]> NativeFormats => VitaNativeTextureFormatCatalog.Formats.Where(format => format.SupportsCooking).Select(format => new object[] { format.Id });

    /// <summary>Supplies raw-only SDK selectors so capabilities never promise an unavailable image conversion.</summary>
    public static IEnumerable<object[]> RawFormats => VitaNativeTextureFormatCatalog.Formats.Where(format => !format.SupportsCooking).Select(format => new object[] { format.Id });

    /// <summary>Supplies native two-bit alpha selectors whose intermediate coverage must survive the RGBA preparation stage.</summary>
    public static IEnumerable<object[]> TwoBitAlphaFormats => VitaNativeTextureFormatCatalog.Formats.Where(format => format.SupportsCooking && format.DefaultAlphaPrecision == TextureAssetAlphaPrecision.A2).Select(format => new object[] { format.Id });

    /// <summary>Verifies capabilities expose actual alpha combinations without changing approved image or atlas defaults.</summary>
    [Fact]
    public void CapabilitiesPreserveDefaultsAndExcludeRawOnlyFormats() {
        var definition = PsVitaPlatformDefinitionFactory.Create();
        Assert.Equal(2, definition.AssetCookCapabilities.Length);
        Assert.Equal("{\"maxResolution\":512,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\"}", definition.AssetCookCapabilities.Single(item => item.SourceAssetKind == "texture").DefaultSerializedPlatformSettings);
        Assert.Equal("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\"}", definition.AssetCookCapabilities.Single(item => item.SourceAssetKind == "font-atlas-texture").DefaultSerializedPlatformSettings);
        foreach (var capability in definition.AssetCookCapabilities) {
            Assert.Equal(11 + VitaNativeTextureFormatCatalog.Formats.Count(format => format.SupportsCooking), capability.TextureFormatCapabilities.SupportedColorFormatIds.Length);
            foreach (var format in VitaNativeTextureFormatCatalog.Formats) {
                if (!format.SupportsCooking) {
                    Assert.DoesNotContain(format.Id, capability.TextureFormatCapabilities.SupportedColorFormatIds);
                    continue;
                }
                Assert.Contains(format.Id, capability.TextureFormatCapabilities.SupportedColorFormatIds);
                Assert.Equal(format.SupportedAlphaPrecisions, capability.TextureFormatCapabilities.SupportedCombinations.Where(item => item.ColorFormatId == format.Id).Select(item => item.AlphaPrecision));
            }
        }
    }

    /// <summary>Validates NPOT native payloads, preview and serialization while retaining the complete authored source identity and bytes.</summary>
    [Theory]
    [MemberData(nameof(NativeFormats))]
    public void EveryNativeSelectorCooksAndSerializesWithoutSourceMutation(string id) {
        VitaNativeTextureFormatCatalog.TryGetFormat(id, out var format);
        using TextureAsset source = Source(5, 3);
        byte[] original = (byte[])source.Colors.Clone();
        TextureAssetProcessorSettings settings = PsVitaTextureCookSettingsSerializer.Deserialize(PsVitaTextureCookSettingsSerializer.Serialize(Settings(id, format.DefaultAlphaPrecision)));
        using TextureAsset cooked = new PsVitaTextureCooker().CookTexture(source, settings);
        Assert.Equal(TextureAssetColorFormat.VitaNative, cooked.ColorFormat);
        Assert.Equal(source.Id, cooked.Id);
        Assert.Equal(source.RuntimeAssetId, cooked.RuntimeAssetId);
        Assert.Equal(source.AuthoringAssetId, cooked.AuthoringAssetId);
        Assert.Equal(source.FormerAuthoringAssetIds, cooked.FormerAuthoringAssetIds);
        Assert.NotSame(source.FormerAuthoringAssetIds, cooked.FormerAuthoringAssetIds);
        Assert.Equal(original, source.Colors);
        Assert.Equal(TextureAssetAlphaPrecision.A8, source.AlphaPrecision);
        using TextureAsset read = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(FilesAssetSerializer.SerializeToBytes(cooked)));
        Assert.Equal(cooked.Colors, read.Colors);
        Assert.Equal(cooked.PaletteColors, read.PaletteColors);
        Assert.Equal(format.HardwareFormat, VitaNativeTextureCodec.ReadLayout(read).Format.HardwareFormat);
        if (format.SupportsPreview) Assert.Equal(5 * 3 * 4, TextureAssetPixelCodec.DecodeToRgba32(read).Length);
    }

    /// <summary>Rejects raw-only selectors before trying to resize or convert any source pixels.</summary>
    [Theory]
    [MemberData(nameof(RawFormats))]
    public void RawOnlySelectorCannotBeSelectedForImageCooking(string id) {
        VitaNativeTextureFormatCatalog.TryGetFormat(id, out var format);
        var exception = Assert.Throws<InvalidOperationException>(() => PsVitaTextureCookSettingsSerializer.Validate(Settings(id, format.DefaultAlphaPrecision)));
        Assert.Contains("cannot be cooked", exception.Message, StringComparison.Ordinal);
    }

    /// <summary>Retains generic indexed settings, explicit quantization and source ownership for legacy authored assets.</summary>
    [Theory]
    [InlineData(TextureAssetColorFormat.Rgba32)]
    [InlineData(TextureAssetColorFormat.Indexed4)]
    [InlineData(TextureAssetColorFormat.Indexed8)]
    public void GenericCookRetainsLegacySettingsAndOwnsIndependentStorage(TextureAssetColorFormat format) {
        using TextureAsset source = Source(8, 4);
        byte[] original = (byte[])source.Colors.Clone();
        var settings = new TextureAssetProcessorSettings { MaxResolution = 4, ColorFormat = format, AlphaPrecision = TextureAssetAlphaPrecision.A8,
            IndexingMethodId = format == TextureAssetColorFormat.Rgba32 ? string.Empty : TextureAssetIndexingMethod.QuantizedIndexed.ToString() };
        using TextureAsset cooked = new PsVitaTextureCooker().CookTexture(source, settings);
        Assert.Equal(format, cooked.ColorFormat);
        Assert.Equal((ushort)4, cooked.Width);
        Assert.Equal((ushort)2, cooked.Height);
        Assert.NotSame(source, cooked);
        Assert.NotSame(source.Colors, cooked.Colors);
        Assert.Equal(original, source.Colors);
        Assert.Equal(4 * 2 * 4, TextureAssetPixelCodec.DecodeToRgba32(cooked).Length);
    }

    /// <summary>Preserves the 85 and 170 alpha levels by applying A2 only in the final native encoder.</summary>
    [Theory]
    [MemberData(nameof(TwoBitAlphaFormats))]
    public void TwoBitAlphaLevelsSurviveNativeCooking(string id) {
        using TextureAsset source = Source(2, 1);
        source.Colors = [255, 255, 255, 85, 255, 255, 255, 170];
        using TextureAsset cooked = new PsVitaTextureCooker().CookTexture(source, Settings(id, TextureAssetAlphaPrecision.A2));
        byte[] preview = VitaNativeTextureCodec.Decode(cooked);
        Assert.Equal((byte)85, preview[3]);
        Assert.Equal((byte)170, preview[7]);
        Assert.Equal(TextureAssetAlphaPrecision.A2, cooked.AlphaPrecision);
    }

    /// <summary>Applies the selected maximum before native allocation without resizing the caller's asset or atlas metadata.</summary>
    [Fact]
    public void NativeResizePrecedesStorageAllocation() {
        using TextureAsset source = Source(8, 4);
        var settings = Settings("Vita.U8U8U8U8_ABGR", TextureAssetAlphaPrecision.A8);
        settings.MaxResolution = 4;
        using TextureAsset cooked = new PsVitaTextureCooker().CookTexture(source, settings);
        Assert.Equal((ushort)4, cooked.Width);
        Assert.Equal((ushort)2, cooked.Height);
        Assert.Equal((ushort)8, source.Width);
        Assert.Equal((ushort)4, source.Height);
        Assert.Equal(4 * 2 * 4, VitaNativeTextureCodec.Decode(cooked).Length);
    }

    /// <summary>Rejects malformed, duplicate or unknown settings instead of silently substituting a default format or quantizer.</summary>
    [Theory]
    [InlineData("{}")]
    [InlineData("[]")]
    [InlineData("{\"maxResolution\":0,\"maxResolution\":1,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\"}")]
    [InlineData("{\"maxResolution\":-1,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"missing\",\"alphaPrecision\":\"A8\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"255\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A2\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A3\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Indexed8\",\"alphaPrecision\":\"A8\",\"indexingMethod\":\"missing\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\",\"indexingMethod\":\"QuantizedIndexed\"}")]
    [InlineData("{\"maxResolution\":0,\"colorFormat\":\"Rgba32\",\"alphaPrecision\":\"A8\",\"unknown\":1}")]
    public void InvalidSettingsAreRejected(string json) {
        Assert.ThrowsAny<Exception>(() => PsVitaTextureCookSettingsSerializer.Deserialize(json));
    }

    /// <summary>Creates private RGBA source pixels with nontrivial channels, alpha and authoring metadata for actual cook tests.</summary>
    internal static TextureAsset Source(int width, int height) {
        byte[] pixels = new byte[width * height * 4];
        for (int index = 0; index < width * height; index++) {
            pixels[index * 4] = (byte)(index * 29);
            pixels[index * 4 + 1] = (byte)(index * 53);
            pixels[index * 4 + 2] = (byte)(255 - index * 11);
            pixels[index * 4 + 3] = (byte)(index % 4 * 85);
        }
        return new TextureAsset { Id = "source", RuntimeAssetId = RuntimeAssetIdGenerator.Generate("source"), AuthoringAssetId = "authored-source",
            FormerAuthoringAssetIds = ["former-source"], IsEngineOwned = true, Width = (ushort)width, Height = (ushort)height,
            ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = pixels, PaletteColors = [] };
    }

    /// <summary>Builds native settings with the selected palette strategy only when that SDK format uses palette indices.</summary>
    internal static TextureAssetProcessorSettings Settings(string id, TextureAssetAlphaPrecision alpha) {
        VitaNativeTextureFormatCatalog.TryGetFormat(id, out var format);
        return new TextureAssetProcessorSettings { MaxResolution = 0, ColorFormatId = id, AlphaPrecision = alpha,
            IndexingMethodId = format != null && format.IsIndexed ? TextureAssetIndexingMethod.QuantizedIndexed.ToString() : string.Empty };
    }
}
