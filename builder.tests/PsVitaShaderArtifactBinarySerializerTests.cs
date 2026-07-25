using System.Security.Cryptography;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the versioned binary contract used to move compiled Vita shader programs between the Vita and host builder.
/// </summary>
public sealed class PsVitaShaderArtifactBinarySerializerTests {
    /// <summary>
    /// Verifies that a serialized vertex artifact preserves metadata and complete program bytes after deserialization.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_validVertexArtifact_preservesCompletePayload() {
        PsVitaShaderArtifact artifact = CreateArtifact("VP", [1, 2, 3, 4]);

        byte[] bytes = new PsVitaShaderArtifactBinarySerializer().Serialize(artifact);
        PsVitaShaderArtifact decoded = new PsVitaShaderArtifactBinarySerializer().Deserialize(bytes);

        Assert.Equal(artifact.StageProfile, decoded.StageProfile);
        Assert.Equal(artifact.CompilerVersion, decoded.CompilerVersion);
        Assert.Equal(artifact.SourceHash, decoded.SourceHash);
        Assert.Equal(artifact.EntryPoint, decoded.EntryPoint);
        Assert.Equal(artifact.OptionsSignature, decoded.OptionsSignature);
        Assert.Equal(artifact.ProgramBytes, decoded.ProgramBytes);
        Assert.Equal(artifact.ArtifactHash, decoded.ArtifactHash);
    }

    /// <summary>
    /// Verifies that a serialized fragment artifact preserves its fragment profile and bytecode payload.
    /// </summary>
    [Fact]
    public void SerializeAndDeserialize_validFragmentArtifact_preservesFragmentIdentity() {
        PsVitaShaderArtifact artifact = CreateArtifact("FP", [9, 8, 7]);

        PsVitaShaderArtifact decoded = new PsVitaShaderArtifactBinarySerializer().Deserialize(
            new PsVitaShaderArtifactBinarySerializer().Serialize(artifact));

        Assert.Equal("FP", decoded.StageProfile);
        Assert.Equal(new byte[] { 9, 8, 7 }, decoded.ProgramBytes);
    }

    /// <summary>
    /// Verifies that malformed artifact prefixes are rejected before any program bytes are exposed.
    /// </summary>
    [Fact]
    public void Deserialize_invalidMagic_throwsArtifactFormatError() {
        byte[] bytes = new PsVitaShaderArtifactBinarySerializer().Serialize(CreateArtifact("VP", [1]));
        bytes[0] = (byte)'X';

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(
            () => new PsVitaShaderArtifactBinarySerializer().Deserialize(bytes));

        Assert.Contains("magic", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Verifies that changing a serialized program byte invalidates the artifact hash.
    /// </summary>
    [Fact]
    public void Deserialize_changedProgramByte_throwsHashError() {
        PsVitaShaderArtifactBinarySerializer serializer = new();
        byte[] bytes = serializer.Serialize(CreateArtifact("VP", [1, 2, 3]));
        bytes[^1] ^= 0xFF;

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));

        Assert.Contains("hash", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Verifies that a future artifact schema cannot be interpreted as the current schema.
    /// </summary>
    [Fact]
    public void Deserialize_unsupportedVersion_throwsVersionError() {
        PsVitaShaderArtifactBinarySerializer serializer = new();
        byte[] bytes = serializer.Serialize(CreateArtifact("VP", [1]));
        bytes[4] = 2;

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));

        Assert.Contains("version", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Verifies that bytes after the signed artifact payload are rejected instead of ignored.
    /// </summary>
    [Fact]
    public void Deserialize_trailingBytes_throwsTrailingDataError() {
        PsVitaShaderArtifactBinarySerializer serializer = new();
        byte[] serialized = serializer.Serialize(CreateArtifact("VP", [1]));
        byte[] bytes = new byte[serialized.Length + 1];
        serialized.CopyTo(bytes, 0);
        bytes[^1] = 0x7F;

        InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() => serializer.Deserialize(bytes));

        Assert.Contains("trailing", exception.Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Creates one deterministic artifact fixture with a valid SHA-256 source identity.
    /// </summary>
    /// <param name="stageProfile">Vita shader profile identifier.</param>
    /// <param name="programBytes">Compiled program payload.</param>
    /// <returns>A valid shader artifact fixture.</returns>
    static PsVitaShaderArtifact CreateArtifact(string stageProfile, byte[] programBytes) {
        return new PsVitaShaderArtifact(
            stageProfile,
            "libshacccg-test",
            Convert.ToHexString(SHA256.HashData("source"u8)),
            "VS",
            "O3-W4",
            programBytes);
    }
}
