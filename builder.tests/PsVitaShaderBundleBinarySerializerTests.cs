using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the versioned Vita shader bundle preserves the separate shader asset identity and its device-compiled program pair.
/// </summary>
public sealed class PsVitaShaderBundleBinarySerializerTests {
    /// <summary>
    /// Ensures a bundle round trip retains every lookup key, source identity, and compiled stage payload.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_whenBundleContainsOneShader_preservesLookupAndPrograms() {
        PsVitaShaderBundle bundle = new([
            new PsVitaShaderBundleEntry(
                "Rendering.Custom.Water",
                new string('A', 64),
                "Rendering.Custom.Water.vs",
                "Rendering.Custom.Water.ps",
                "default",
                [1, 2, 3],
                [4, 5, 6])
        ]);

        PsVitaShaderBundle decoded = new PsVitaShaderBundleBinarySerializer().Deserialize(
            new PsVitaShaderBundleBinarySerializer().Serialize(bundle));

        PsVitaShaderBundleEntry entry = Assert.Single(decoded.Entries);
        Assert.Equal("Rendering.Custom.Water", entry.ShaderAssetId);
        Assert.Equal(new string('A', 64), entry.SourceHash);
        Assert.Equal("Rendering.Custom.Water.vs", entry.VertexProgramName);
        Assert.Equal("Rendering.Custom.Water.ps", entry.PixelProgramName);
        Assert.Equal("default", entry.VariantName);
        Assert.Equal(new byte[] { 1, 2, 3 }, entry.VertexArtifactBytes);
        Assert.Equal(new byte[] { 4, 5, 6 }, entry.FragmentArtifactBytes);
    }

    /// <summary>
    /// Ensures multiple Vita-only shader variants sharing one material shader identity remain independently addressable after a binary round trip.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_whenStandardShaderContainsShadowVariants_preservesEachVariant() {
        PsVitaShaderBundle bundle = new([
            new PsVitaShaderBundleEntry("ForwardStandardShader", new string('B', 64), "ForwardStandardShader.vs", "ForwardStandardShader.ps", "ForwardStandardTextured", [1], [2]),
            new PsVitaShaderBundleEntry("ForwardStandardShader", new string('B', 64), "ForwardStandardShader.vs", "ForwardStandardShader.ps", "ForwardStandardShadowed", [3], [4]),
            new PsVitaShaderBundleEntry("ForwardStandardShader", new string('B', 64), "ForwardStandardShader.vs", "ForwardStandardShader.ps", "ShadowDepth", [5], [6])
        ]);

        PsVitaShaderBundle decoded = new PsVitaShaderBundleBinarySerializer().Deserialize(
            new PsVitaShaderBundleBinarySerializer().Serialize(bundle));

        Assert.Equal(3, decoded.Entries.Count);
        Assert.Contains(decoded.Entries, entry => entry.VariantName == "ForwardStandardTextured" && entry.VertexArtifactBytes.SequenceEqual(new byte[] { 1 }));
        Assert.Contains(decoded.Entries, entry => entry.VariantName == "ForwardStandardShadowed" && entry.VertexArtifactBytes.SequenceEqual(new byte[] { 3 }));
        Assert.Contains(decoded.Entries, entry => entry.VariantName == "ShadowDepth" && entry.VertexArtifactBytes.SequenceEqual(new byte[] { 5 }));
    }
}
