using helengine;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.editor;
using Xunit;
using FilesAssetSerializer = helengine.files.AssetSerializer;

namespace helengine.psvita.builder.tests;

/// <summary>Exercises actual serialized sources, native texture/atlas work items and preservation of existing output trees on preflight failures.</summary>
[Collection("VitaTextureStaging")]
public sealed class PsVitaNativeTextureWorkItemTests : IDisposable {
    /// <summary>Keeps all fixture data inside the fixed visible test artifacts directory.</summary>
    readonly string Root = Path.Combine(AppContext.BaseDirectory, "vita-texture-work-items");
    /// <summary>Preserves the caller's repository override for cleanup.</summary>
    readonly string PreviousRepositoryRoot = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT");
    /// <summary>Preserves the caller's authored project directory for cleanup.</summary>
    readonly string PreviousDirectory = Directory.GetCurrentDirectory();
    /// <summary>Gets the real serialized source image used by both work-item kinds.</summary>
    string Source => Path.Combine(Root, "source", "source.hetex");
    /// <summary>Gets the existing editor output tree that unsuccessful batches must preserve.</summary>
    string Output => Path.Combine(Root, "output");
    /// <summary>Gets the native staged content tree that unsuccessful batches must preserve.</summary>
    string Stage => Path.Combine(Root, "working", "staged-content");

    /// <summary>Creates serialized source pixels and previous output markers without importing through a desktop backend.</summary>
    public PsVitaNativeTextureWorkItemTests() {
        Environment.SetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT", ResolveRepositoryRoot());
        Directory.CreateDirectory(Path.GetDirectoryName(Source));
        Directory.CreateDirectory(Output);
        Directory.CreateDirectory(Stage);
        Directory.CreateDirectory(Path.Combine(Root, "generated-core"));
        using TextureAsset source = PsVitaNativeTextureCookTests.Source(5, 3);
        File.WriteAllBytes(Source, FilesAssetSerializer.SerializeToBytes(source));
        File.WriteAllText(Path.Combine(Output, "previous.txt"), "previous");
        File.WriteAllText(Path.Combine(Stage, "previous.txt"), "previous");
    }

    /// <summary>Runs every advertised native selection through both real work-item kinds and inspects the staged serialized payloads.</summary>
    [Fact]
    public async Task EveryNativeSelectorCooksImageAndAtlasWorkItems() {
        byte[] original = File.ReadAllBytes(Source);
        List<PlatformCookWorkItem> items = [];
        int index = 0;
        foreach (var format in VitaNativeTextureFormatCatalog.Formats.Where(format => format.SupportsCooking)) {
            items.Add(Item($"cooked/images/{index}.hetex", format.Id));
            items.Add(Item($"cooked/fonts/{index}.hetex", format.Id, "font-atlas-texture"));
            index++;
        }
        PsVitaStopAfterStagingNativeBuildExecutor boundary = new();
        var report = await new PsVitaPlatformAssetBuilder(boundary).BuildAsync(Request(items.ToArray(), []), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.True(boundary.WasCalled);
        Assert.Contains(report.Diagnostics, diagnostic => diagnostic.Message.Contains(PsVitaStopAfterStagingNativeBuildExecutor.StopMessage, StringComparison.Ordinal));
        foreach (var item in items) {
            string stagedRelative = item.OutputRelativePath.Substring("cooked/".Length);
            byte[] editorBytes = File.ReadAllBytes(Path.Combine(Output, item.OutputRelativePath));
            Assert.Equal(editorBytes, File.ReadAllBytes(Path.Combine(Stage, stagedRelative)));
            using TextureAsset cooked = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(editorBytes));
            Assert.Equal("authored-source", cooked.AuthoringAssetId);
            Assert.Equal(item.SerializedPlatformSettings, PsVitaTextureCookSettingsSerializer.Serialize(PsVitaTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings)));
            VitaNativeTextureFormatCatalog.TryGetFormat(PsVitaTextureCookSettingsSerializer.Deserialize(item.SerializedPlatformSettings).ColorFormatId, out var selected);
            Assert.Equal(selected.HardwareFormat, VitaNativeTextureCodec.ReadLayout(cooked).Format.HardwareFormat);
        }
        Assert.Equal(original, File.ReadAllBytes(Source));
    }

    /// <summary>Loads an actual embedded font atlas through the existing interface and preserves the source file and identity metadata.</summary>
    [Fact]
    public void SerializedFontAtlasUsesNativeCookerWithoutMutatingFont() {
        string fontPath = WriteFontSource("font.hefont", "cooked/fonts/body.hetex");
        byte[] original = File.ReadAllBytes(fontPath);
        using TextureAsset atlas = new PsVitaPlatformCookSourceProcessor().CookFontAtlasTexture(fontPath, "fonts/body.hefont",
            PsVitaNativeTextureCookTests.Settings("Vita.U8U8U8U8_ABGR", TextureAssetAlphaPrecision.A8));
        Assert.Equal("fonts/body.hefont#atlas", atlas.Id);
        Assert.Equal(RuntimeAssetIdGenerator.Generate(atlas.Id), atlas.RuntimeAssetId);
        Assert.Equal("Vita.U8U8U8U8_ABGR", VitaNativeTextureCodec.ReadLayout(atlas).Format.Id);
        Assert.Equal(original, File.ReadAllBytes(fontPath));
    }

    /// <summary>Rejects runtime path traversal, device aliases and alternate streams before replacing either old output tree.</summary>
    [Theory]
    [InlineData("../escape.hetex")]
    [InlineData("C:/escape.hetex")]
    [InlineData("image.hetex:stream")]
    [InlineData("NUL.hetex")]
    [InlineData("bad?.hetex")]
    public async Task UnsafeOutputPreservesPreviousTrees(string path) {
        await AssertPreparationFails([Item(path)], []);
    }

    /// <summary>Detects case and cooked-prefix aliases, plus file-versus-directory conflicts, before any publication.</summary>
    [Theory]
    [InlineData("cooked/image.hetex", "image.hetex")]
    [InlineData("cooked/image.hetex", "cooked/IMAGE.hetex")]
    [InlineData("cooked/image", "cooked/image/child.hetex")]
    public async Task ConflictingOutputsPreservePreviousTrees(string first, string second) {
        await AssertPreparationFails([Item(first), Item(second)], []);
    }

    /// <summary>Finishes every cook before reset, so a corrupt later source cannot destroy previous outputs after an earlier successful cook.</summary>
    [Fact]
    public async Task LaterDecodeFailurePublishesNothing() {
        string broken = Path.Combine(Path.GetDirectoryName(Source), "broken.hetex");
        File.WriteAllBytes(broken, [1, 2, 3]);
        await AssertPreparationFails([Item("cooked/first.hetex"), Item("cooked/second.hetex", source: broken)], []);
        Assert.False(File.Exists(Path.Combine(Output, "cooked/first.hetex")));
    }

    /// <summary>Reserves cooked manifest artifacts against work-item destinations rather than letting a later texture silently replace them.</summary>
    [Fact]
    public async Task ManifestCollisionPreservesPreviousTrees() {
        Directory.SetCurrentDirectory(Path.GetDirectoryName(Source));
        await AssertPreparationFails([Item("source.hetex")], [new PlatformBuildArtifact("source.hetex", "source", "hash", "texture", "debug")]);
    }

    /// <summary>Rejects input files beneath a tree scheduled for reset, even when the input could be decoded before publication.</summary>
    [Fact]
    public async Task InputInReplacedTreeIsRejected() {
        string inside = Path.Combine(Output, "input.hetex");
        File.Copy(Source, inside);
        await AssertPreparationFails([Item("cooked/result.hetex", source: inside)], []);
        Assert.True(File.Exists(inside));
    }

    /// <summary>Publishes a selected native atlas alongside an unchanged font's explicit atlas reference without recooking it to generic RGBA.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public async Task ExplicitNativeAtlasRemainsSelected(bool fromManifest) {
        string fontPath = WriteFontSource("cooked/fonts/body.hefont", "cooked/fonts/body.hetex");
        byte[] originalFont = File.ReadAllBytes(fontPath);
        Directory.SetCurrentDirectory(Path.GetDirectoryName(Source));
        List<PlatformBuildArtifact> artifacts = [new("cooked/fonts/body.hefont", "font", "hash", "font", "debug")];
        var nativeFormat = VitaNativeTextureFormatCatalog.Formats.First(format => format.SupportsCooking && format.IsIndexed);
        PlatformCookWorkItem[] items = [];
        byte[] explicitBytes = null;
        if (fromManifest) {
            using TextureAsset raw = PsVitaNativeTextureCookTests.Source(5, 3);
            using TextureAsset atlas = new PsVitaTextureCooker().CookTexture(raw, PsVitaNativeTextureCookTests.Settings(nativeFormat.Id, nativeFormat.DefaultAlphaPrecision));
            explicitBytes = FilesAssetSerializer.SerializeToBytes(atlas);
            File.WriteAllBytes(Path.Combine(Directory.GetCurrentDirectory(), "cooked/fonts/body.hetex"), explicitBytes);
            artifacts.Add(new("cooked/fonts/body.hetex", "atlas", "hash", "texture", "debug"));
        } else {
            items = [Item("cooked/fonts/body.hetex", nativeFormat.Id, "font-atlas-texture", fontPath)];
        }
        PsVitaStopAfterStagingNativeBuildExecutor boundary = new();
        await new PsVitaPlatformAssetBuilder(boundary).BuildAsync(Request(items, artifacts.ToArray()), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.True(boundary.WasCalled);
        byte[] stagedBytes = File.ReadAllBytes(Path.Combine(Stage, "fonts/body.hetex"));
        using TextureAsset staged = Assert.IsType<TextureAsset>(FilesAssetSerializer.DeserializeFromBytes(stagedBytes));
        Assert.Equal(nativeFormat.Id, VitaNativeTextureCodec.ReadLayout(staged).Format.Id);
        if (explicitBytes != null) Assert.Equal(explicitBytes, stagedBytes);
        Assert.Equal(originalFont, File.ReadAllBytes(Path.Combine(Stage, "fonts/body.hefont")));
        Assert.Equal(originalFont, File.ReadAllBytes(fontPath));
    }

    /// <summary>Executes a failing real build and verifies it never reaches the native boundary or changes previous files.</summary>
    async Task AssertPreparationFails(PlatformCookWorkItem[] items, PlatformBuildArtifact[] artifacts) {
        PsVitaStopAfterStagingNativeBuildExecutor boundary = new();
        var report = await new PsVitaPlatformAssetBuilder(boundary).BuildAsync(Request(items, artifacts), new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        Assert.False(report.Succeeded);
        Assert.NotEmpty(report.Diagnostics);
        Assert.False(boundary.WasCalled);
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Output, "previous.txt")));
        Assert.Equal("previous", File.ReadAllText(Path.Combine(Stage, "previous.txt")));
    }

    /// <summary>Writes a real font serializer payload whose atlas metadata is already resolved by the editor.</summary>
    string WriteFontSource(string relative, string atlasRelative) {
        string path = Path.Combine(Path.GetDirectoryName(Source), relative);
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        using TextureAsset raw = PsVitaNativeTextureCookTests.Source(5, 3);
        using FontAsset font = new(new FontInfo("Body", 16, 8), null, new Dictionary<char, FontChar>(), 16, 5, 3) {
            SourceTextureAsset = raw, CookedAtlasTextureRelativePath = atlasRelative
        };
        using (FileStream stream = File.Create(path)) helengine.files.FontAssetBinarySerializer.Serialize(stream, font);
        font.SourceTextureAsset = null;
        return path;
    }

    /// <summary>Builds work-item metadata without changing the existing platform source processor interface.</summary>
    PlatformCookWorkItem Item(string output, string formatId = "Vita.U8U8U8U8_ABGR", string kind = "texture", string source = null) {
        VitaNativeTextureFormatCatalog.TryGetFormat(formatId, out var format);
        return new PlatformCookWorkItem(output, source ?? Source, kind, "psvita", "runtime-texture", output, output, "source", "settings",
            PsVitaTextureCookSettingsSerializer.Serialize(PsVitaNativeTextureCookTests.Settings(formatId, format.DefaultAlphaPrecision)), []);
    }

    /// <summary>Constructs a real builder request that isolates staging tests from native SDK compilation and packaging.</summary>
    PlatformBuildRequest Request(PlatformCookWorkItem[] items, PlatformBuildArtifact[] artifacts) {
        var manifest = new PlatformBuildManifest(3, "project", "1.0.0", "1.0.0", "psvita", "1.0.0", "", [], [], artifacts, [], [],
            new PlatformContainerWritePlan(string.Empty, []), items, PlatformBuildRuntimeFeatureManifest.Empty);
        return new PlatformBuildRequest(manifest, [new PlatformBuildTargetVariant("psvita-default", "psvita", "psvita", "debug")],
            [new PlatformCookProfile("debug", "Debug", new PlatformCookProfileCapabilities("psvita", "raw", "rgba", "scene", PlatformSerializationEndianness.LittleEndian))],
            Output, Path.Combine(Root, "working"), "debug", "psvita-forward", "default", null, null, null, Path.Combine(Root, "generated-core"));
    }

    /// <summary>Restores process-local fixture state and deletes only the resolved directory beneath this test's fixed artifacts root.</summary>
    public void Dispose() {
        Directory.SetCurrentDirectory(PreviousDirectory);
        Environment.SetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT", PreviousRepositoryRoot);
        string expected = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "vita-texture-work-items"));
        if (!Path.GetFullPath(Root).Equals(expected, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Fixture cleanup escaped its test artifacts root.");
        if (Directory.Exists(Root)) Directory.Delete(Root, true);
    }

    /// <summary>Locates the actual checkout from an explicit runner override or fixed relocated artifacts without assuming the current project directory.</summary>
    static string ResolveRepositoryRoot() {
        string configured = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT");
        if (!string.IsNullOrEmpty(configured) && File.Exists(Path.Combine(configured, "builder", "helengine.psvita.builder.csproj"))) return configured;
        string current = AppContext.BaseDirectory;
        while (!string.IsNullOrEmpty(current)) {
            string candidate = Path.Combine(current, "helengine-psvita");
            if (File.Exists(Path.Combine(candidate, "builder", "helengine.psvita.builder.csproj"))) return candidate;
            current = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(current));
        }
        throw new DirectoryNotFoundException("The Vita texture fixture could not locate its checkout.");
    }
}
