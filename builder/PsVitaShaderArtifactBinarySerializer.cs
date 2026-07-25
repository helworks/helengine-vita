using System.Security.Cryptography;
using System.Text;

namespace helengine.psvita.builder;

/// <summary>
/// Serializes and validates complete Vita shader compiler artifacts without interpreting their program payload.
/// </summary>
public sealed class PsVitaShaderArtifactBinarySerializer {
    /// <summary>
    /// Stable four-byte prefix identifying one Vita shader artifact.
    /// </summary>
    public static ReadOnlySpan<byte> Magic => "PVSA"u8;

    /// <summary>
    /// Current serialized Vita shader artifact version.
    /// </summary>
    public const uint Version = 1u;

    /// <summary>
    /// Serializes one complete Vita shader artifact and its deterministic hash.
    /// </summary>
    /// <param name="artifact">Artifact to serialize.</param>
    /// <returns>Serialized artifact bytes.</returns>
    public byte[] Serialize(PsVitaShaderArtifact artifact) {
        if (artifact == null) {
            throw new ArgumentNullException(nameof(artifact));
        }

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, leaveOpen: true);
        WriteUnsignedHeader(writer, artifact);
        byte[] programBytes = artifact.ProgramBytes;
        writer.Write((uint)programBytes.Length);
        writer.Write(programBytes);
        WriteString(writer, ComputeArtifactHash(artifact));
        return stream.ToArray();
    }

    /// <summary>
    /// Deserializes one complete Vita shader artifact and verifies its content hash.
    /// </summary>
    /// <param name="bytes">Serialized artifact bytes.</param>
    /// <returns>Validated artifact.</returns>
    public PsVitaShaderArtifact Deserialize(byte[] bytes) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        using MemoryStream stream = new(bytes, writable: false);
        using BinaryReader reader = new(stream, Encoding.UTF8, leaveOpen: true);
        ValidateMagic(reader);
        uint version = ReadUInt32(reader, "version");
        if (version != Version) {
            throw new InvalidOperationException($"Unsupported Vita shader artifact version '{version}'.");
        }

        string stageProfile = ReadString(reader, "stage profile");
        string compilerVersion = ReadString(reader, "compiler version");
        string sourceHash = ReadString(reader, "source hash");
        string entryPoint = ReadString(reader, "entry point");
        string optionsSignature = ReadString(reader, "compiler options");
        uint programSize = ReadUInt32(reader, "program size");
        if (programSize == 0u || programSize > stream.Length - stream.Position) {
            throw new InvalidOperationException("Vita shader artifact program size is invalid or truncated.");
        }

        byte[] programBytes = reader.ReadBytes(checked((int)programSize));
        if (programBytes.Length != programSize) {
            throw new InvalidOperationException("Vita shader artifact program bytes are truncated.");
        }

        string storedHash = ReadString(reader, "artifact hash");
        if (stream.Position != stream.Length) {
            throw new InvalidOperationException("Vita shader artifact contains trailing bytes.");
        }

        PsVitaShaderArtifact artifact = new(
            stageProfile,
            compilerVersion,
            sourceHash,
            entryPoint,
            optionsSignature,
            programBytes);
        if (!string.Equals(storedHash, ComputeArtifactHash(artifact), StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException("Vita shader artifact hash does not match its content.");
        }

        return artifact;
    }

    /// <summary>
    /// Computes the canonical SHA-256 hash over artifact identity fields and complete program bytes.
    /// </summary>
    /// <param name="artifact">Artifact to hash.</param>
    /// <returns>Uppercase hexadecimal SHA-256 hash.</returns>
    public static string ComputeArtifactHash(PsVitaShaderArtifact artifact) {
        if (artifact == null) {
            throw new ArgumentNullException(nameof(artifact));
        }

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, leaveOpen: true);
        WriteUnsignedHeader(writer, artifact);
        byte[] programBytes = artifact.ProgramBytes;
        writer.Write((uint)programBytes.Length);
        writer.Write(programBytes);
        writer.Flush();
        return Convert.ToHexString(SHA256.HashData(stream.ToArray()));
    }

    /// <summary>
    /// Writes the fields that define the artifact hash and serialized payload.
    /// </summary>
    /// <param name="writer">Destination writer.</param>
    /// <param name="artifact">Artifact metadata.</param>
    static void WriteUnsignedHeader(BinaryWriter writer, PsVitaShaderArtifact artifact) {
        writer.Write(Magic);
        writer.Write(Version);
        WriteString(writer, artifact.StageProfile);
        WriteString(writer, artifact.CompilerVersion);
        WriteString(writer, artifact.SourceHash);
        WriteString(writer, artifact.EntryPoint);
        WriteString(writer, artifact.OptionsSignature);
    }

    /// <summary>
    /// Writes one bounded UTF-8 string with an explicit byte count.
    /// </summary>
    /// <param name="writer">Destination writer.</param>
    /// <param name="value">String value.</param>
    static void WriteString(BinaryWriter writer, string value) {
        byte[] bytes = Encoding.UTF8.GetBytes(value);
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    /// <summary>
    /// Reads one bounded UTF-8 string from the artifact stream.
    /// </summary>
    /// <param name="reader">Source reader.</param>
    /// <param name="fieldName">Field name for diagnostics.</param>
    /// <returns>Decoded string.</returns>
    static string ReadString(BinaryReader reader, string fieldName) {
        int byteCount = ReadInt32(reader, fieldName);
        if (byteCount <= 0 || byteCount > reader.BaseStream.Length - reader.BaseStream.Position) {
            throw new InvalidOperationException($"Vita shader artifact {fieldName} length is invalid.");
        }

        byte[] bytes = reader.ReadBytes(byteCount);
        if (bytes.Length != byteCount) {
            throw new InvalidOperationException($"Vita shader artifact {fieldName} is truncated.");
        }

        return Encoding.UTF8.GetString(bytes);
    }

    /// <summary>
    /// Reads one little-endian signed length field with a field-specific error.
    /// </summary>
    /// <param name="reader">Source reader.</param>
    /// <param name="fieldName">Field name.</param>
    /// <returns>Decoded length.</returns>
    static int ReadInt32(BinaryReader reader, string fieldName) {
        try {
            return reader.ReadInt32();
        } catch (EndOfStreamException exception) {
            throw new InvalidOperationException($"Vita shader artifact {fieldName} is truncated.", exception);
        }
    }

    /// <summary>
    /// Reads one little-endian unsigned field with a field-specific error.
    /// </summary>
    /// <param name="reader">Source reader.</param>
    /// <param name="fieldName">Field name.</param>
    /// <returns>Decoded value.</returns>
    static uint ReadUInt32(BinaryReader reader, string fieldName) {
        try {
            return reader.ReadUInt32();
        } catch (EndOfStreamException exception) {
            throw new InvalidOperationException($"Vita shader artifact {fieldName} is truncated.", exception);
        }
    }

    /// <summary>
    /// Validates the fixed artifact prefix.
    /// </summary>
    /// <param name="reader">Source reader.</param>
    static void ValidateMagic(BinaryReader reader) {
        byte[] magic = reader.ReadBytes(Magic.Length);
        if (magic.Length != Magic.Length || !magic.AsSpan().SequenceEqual(Magic)) {
            throw new InvalidOperationException("Vita shader artifact magic is invalid.");
        }
    }
}
