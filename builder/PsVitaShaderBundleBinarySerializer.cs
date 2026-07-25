using System.Text;

namespace helengine.psvita.builder;

/// <summary>
/// Serializes the versioned PS Vita shader bundle read by the native runtime before it creates GXM programs.
/// </summary>
public sealed class PsVitaShaderBundleBinarySerializer {
    /// <summary>
    /// Stable four-byte prefix identifying PS Vita shader bundle files.
    /// </summary>
    public static ReadOnlySpan<byte> Magic => "PVSB"u8;

    /// <summary>
    /// Stable shader bundle format version.
    /// </summary>
    public const uint Version = 1u;

    /// <summary>
    /// Serializes one complete shader bundle into deterministic binary bytes.
    /// </summary>
    /// <param name="bundle">Bundle containing complete device-compiled program pairs.</param>
    /// <returns>Serialized shader bundle bytes.</returns>
    public byte[] Serialize(PsVitaShaderBundle bundle) {
        if (bundle == null) {
            throw new ArgumentNullException(nameof(bundle));
        }

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, true);
        writer.Write(Magic);
        writer.Write(Version);
        writer.Write(bundle.Entries.Count);
        for (int index = 0; index < bundle.Entries.Count; index++) {
            WriteEntry(writer, bundle.Entries[index]);
        }

        return stream.ToArray();
    }

    /// <summary>
    /// Deserializes one complete shader bundle from bytes.
    /// </summary>
    /// <param name="bytes">Serialized shader bundle bytes.</param>
    /// <returns>Validated shader bundle.</returns>
    public PsVitaShaderBundle Deserialize(byte[] bytes) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        using MemoryStream stream = new(bytes, false);
        using BinaryReader reader = new(stream, Encoding.UTF8, true);
        ValidateMagic(reader);
        uint version = reader.ReadUInt32();
        if (version != Version) {
            throw new InvalidOperationException($"Unsupported PS Vita shader bundle version '{version}'.");
        }

        int entryCount = reader.ReadInt32();
        if (entryCount < 0) {
            throw new InvalidOperationException("PS Vita shader bundle entry counts cannot be negative.");
        }

        PsVitaShaderBundleEntry[] entries = new PsVitaShaderBundleEntry[entryCount];
        for (int index = 0; index < entries.Length; index++) {
            entries[index] = ReadEntry(reader);
        }
        if (stream.Position != stream.Length) {
            throw new InvalidOperationException("PS Vita shader bundles cannot contain trailing bytes.");
        }

        return new PsVitaShaderBundle(entries);
    }

    /// <summary>
    /// Writes one complete material-lookup entry and both complete PVSA artifacts.
    /// </summary>
    /// <param name="writer">Writer receiving entry data.</param>
    /// <param name="entry">Entry to serialize.</param>
    static void WriteEntry(BinaryWriter writer, PsVitaShaderBundleEntry entry) {
        WriteString(writer, entry.ShaderAssetId);
        WriteString(writer, entry.SourceHash);
        WriteString(writer, entry.VertexProgramName);
        WriteString(writer, entry.PixelProgramName);
        WriteString(writer, entry.VariantName);
        WriteBytes(writer, entry.VertexArtifactBytes);
        WriteBytes(writer, entry.FragmentArtifactBytes);
    }

    /// <summary>
    /// Reads one complete material-lookup entry and both complete PVSA artifacts.
    /// </summary>
    /// <param name="reader">Reader supplying entry data.</param>
    /// <returns>Validated bundle entry.</returns>
    static PsVitaShaderBundleEntry ReadEntry(BinaryReader reader) {
        return new PsVitaShaderBundleEntry(
            ReadString(reader),
            ReadString(reader),
            ReadString(reader),
            ReadString(reader),
            ReadString(reader),
            ReadBytes(reader),
            ReadBytes(reader));
    }

    /// <summary>
    /// Writes one UTF-8 string with an explicit byte length.
    /// </summary>
    /// <param name="writer">Writer receiving the text.</param>
    /// <param name="value">Text value to serialize.</param>
    static void WriteString(BinaryWriter writer, string value) {
        byte[] encodedValue = Encoding.UTF8.GetBytes(value);
        writer.Write(encodedValue.Length);
        writer.Write(encodedValue);
    }

    /// <summary>
    /// Reads one required UTF-8 string with an explicit byte length.
    /// </summary>
    /// <param name="reader">Reader supplying the text.</param>
    /// <returns>Decoded non-empty text.</returns>
    static string ReadString(BinaryReader reader) {
        int byteCount = reader.ReadInt32();
        if (byteCount <= 0) {
            throw new InvalidOperationException("PS Vita shader bundle strings must have a positive byte length.");
        }

        byte[] bytes = ReadExactBytes(reader, byteCount, "string");
        return Encoding.UTF8.GetString(bytes);
    }

    /// <summary>
    /// Writes one complete serialized PVSA artifact with an explicit byte length.
    /// </summary>
    /// <param name="writer">Writer receiving artifact bytes.</param>
    /// <param name="bytes">Non-empty artifact bytes.</param>
    static void WriteBytes(BinaryWriter writer, byte[] bytes) {
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    /// <summary>
    /// Reads one non-empty serialized PVSA artifact with an explicit byte length.
    /// </summary>
    /// <param name="reader">Reader supplying artifact bytes.</param>
    /// <returns>Complete artifact bytes.</returns>
    static byte[] ReadBytes(BinaryReader reader) {
        int byteCount = reader.ReadInt32();
        if (byteCount <= 0) {
            throw new InvalidOperationException("PS Vita shader bundle artifacts must have a positive byte length.");
        }

        return ReadExactBytes(reader, byteCount, "artifact");
    }

    /// <summary>
    /// Reads an exact positive byte count and rejects truncated bundle data.
    /// </summary>
    /// <param name="reader">Reader supplying bytes.</param>
    /// <param name="byteCount">Required byte count.</param>
    /// <param name="valueName">Diagnostic name for the encoded value.</param>
    /// <returns>Exact decoded bytes.</returns>
    static byte[] ReadExactBytes(BinaryReader reader, int byteCount, string valueName) {
        byte[] bytes = reader.ReadBytes(byteCount);
        if (bytes.Length != byteCount) {
            throw new InvalidOperationException($"PS Vita shader bundle {valueName} bytes are truncated.");
        }

        return bytes;
    }

    /// <summary>
    /// Validates the binary prefix before decoding bundle metadata.
    /// </summary>
    /// <param name="reader">Reader positioned at the bundle prefix.</param>
    static void ValidateMagic(BinaryReader reader) {
        byte[] magic = reader.ReadBytes(Magic.Length);
        if (magic.Length != Magic.Length || !magic.AsSpan().SequenceEqual(Magic)) {
            throw new InvalidOperationException("The supplied payload is not one PS Vita shader bundle.");
        }
    }
}
