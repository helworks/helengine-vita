using helengine.editor;

namespace helengine.psvita.builder.tests;

/// <summary>Imports real authored image and font bytes without loading a Windows editor assembly.</summary>
[Collection("VitaTextureStaging")]
public sealed class PsVitaPortableSourceProcessorTests {
    /// <summary>Preserves the PNG pixel and stable identity through the actual native texture cooker.</summary>
    [Fact]
    public void CookTexture_ImportsPngWithoutWindowsEditor() {
        string directory = Path.Combine(Path.GetTempPath(), "vita-portable-import");
        Directory.CreateDirectory(directory);
        string source = Path.Combine(directory, "source.png");
        File.WriteAllBytes(source, Convert.FromBase64String("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg=="));
        try {
            using TextureAsset cooked = new PsVitaPlatformCookSourceProcessor().CookTexture(source, "texture:portable", Settings());
            Assert.Equal((ushort)1, cooked.Width);
            Assert.Equal((ushort)1, cooked.Height);
            Assert.Equal("texture:portable", cooked.Id);
            Assert.Equal(RuntimeAssetIdGenerator.Generate(cooked.Id), cooked.RuntimeAssetId);
            Assert.Equal(new byte[] { 255, 0, 0, 255 }, VitaNativeTextureCodec.Decode(cooked));
        } finally {
            File.Delete(source);
            Directory.Delete(directory);
        }
    }

    /// <summary>Rasterizes the explicit source face and preserves the atlas identity through the native cooker.</summary>
    [Fact]
    public void CookFontAtlasTexture_ImportsFontWithoutWindowsEditor() {
        string source = Path.Combine(AppContext.BaseDirectory, "fixtures", "Synthetic.ttf");
        using TextureAsset cooked = new PsVitaPlatformCookSourceProcessor().CookFontAtlasTexture(source, "font:portable", Settings());
        Assert.Equal("font:portable#atlas", cooked.Id);
        Assert.Equal(RuntimeAssetIdGenerator.Generate(cooked.Id), cooked.RuntimeAssetId);
        Assert.True(cooked.Width > 0 && cooked.Height > 0);
        Assert.Contains(VitaNativeTextureCodec.Decode(cooked), channel => channel != 0);
    }

    /// <summary>Selects a lossless native format so imported pixels remain directly observable.</summary>
    static TextureAssetProcessorSettings Settings() => PsVitaNativeTextureCookTests.Settings("Vita.U8U8U8U8_ABGR", TextureAssetAlphaPrecision.A8);
}
