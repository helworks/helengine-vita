using System.Text;

namespace helengine.psvita.builder;

/// <summary>
/// Serializes and deserializes the PS Vita cooked compiled-shader material payload used by the runtime bridge path.
/// </summary>
public sealed class PsVitaCompiledShaderMaterialBinarySerializer {
    /// <summary>
    /// Stable four-byte magic prefix used to identify PS Vita compiled-shader material payloads.
    /// </summary>
    public static ReadOnlySpan<byte> Magic => "PVMT"u8;

    /// <summary>
    /// Stable compiled-shader material payload version.
    /// </summary>
    public const uint Version = 1u;

    /// <summary>
    /// Serializes one PS Vita compiled-shader material payload into bytes.
    /// </summary>
    /// <param name="asset">Compiled-shader material payload to encode.</param>
    /// <returns>Serialized payload bytes.</returns>
    public byte[] Serialize(PsVitaCompiledShaderMaterialAsset asset) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (string.IsNullOrWhiteSpace(asset.ShaderAssetId)) {
            throw new InvalidOperationException("PS Vita compiled-shader materials require one shader asset id.");
        } else if (string.IsNullOrWhiteSpace(asset.VertexProgramName)) {
            throw new InvalidOperationException("PS Vita compiled-shader materials require one vertex-program name.");
        } else if (string.IsNullOrWhiteSpace(asset.PixelProgramName)) {
            throw new InvalidOperationException("PS Vita compiled-shader materials require one pixel-program name.");
        } else if (string.IsNullOrWhiteSpace(asset.VariantName)) {
            throw new InvalidOperationException("PS Vita compiled-shader materials require one shader variant name.");
        }

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, leaveOpen: true);
        writer.Write(Magic);
        writer.Write(Version);
        WriteString(writer, asset.ShaderAssetId);
        WriteString(writer, asset.VertexProgramName);
        WriteString(writer, asset.PixelProgramName);
        WriteString(writer, asset.VariantName);
        writer.Write(asset.BaseColorAbgr);
        return stream.ToArray();
    }

    /// <summary>
    /// Deserializes one PS Vita compiled-shader material payload from bytes.
    /// </summary>
    /// <param name="bytes">Serialized payload bytes.</param>
    /// <returns>Decoded compiled-shader material payload.</returns>
    public PsVitaCompiledShaderMaterialAsset Deserialize(byte[] bytes) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        using MemoryStream stream = new(bytes, writable: false);
        using BinaryReader reader = new(stream, Encoding.UTF8, leaveOpen: true);
        ValidateMagic(reader);

        uint version = reader.ReadUInt32();
        if (version != Version) {
            throw new InvalidOperationException($"Unsupported PS Vita compiled-shader material payload version '{version}'.");
        }

        return new PsVitaCompiledShaderMaterialAsset {
            ShaderAssetId = ReadString(reader),
            VertexProgramName = ReadString(reader),
            PixelProgramName = ReadString(reader),
            VariantName = ReadString(reader),
            BaseColorAbgr = reader.ReadUInt32()
        };
    }

    /// <summary>
    /// Writes one UTF-8 string with an explicit byte-length prefix.
    /// </summary>
    /// <param name="writer">Binary writer receiving the string.</param>
    /// <param name="value">String value to encode.</param>
    static void WriteString(BinaryWriter writer, string value) {
        byte[] bytes = Encoding.UTF8.GetBytes(value ?? string.Empty);
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    /// <summary>
    /// Reads one UTF-8 string with an explicit byte-length prefix.
    /// </summary>
    /// <param name="reader">Binary reader supplying the string.</param>
    /// <returns>Decoded string value.</returns>
    static string ReadString(BinaryReader reader) {
        int byteCount = reader.ReadInt32();
        if (byteCount < 0) {
            throw new InvalidOperationException("PS Vita compiled-shader material string lengths cannot be negative.");
        }

        return Encoding.UTF8.GetString(reader.ReadBytes(byteCount));
    }

    /// <summary>
    /// Validates that the reader is positioned at one PS Vita compiled-shader material payload.
    /// </summary>
    /// <param name="reader">Binary reader whose prefix should be validated.</param>
    static void ValidateMagic(BinaryReader reader) {
        byte[] magic = reader.ReadBytes(Magic.Length);
        if (magic.Length != Magic.Length || !magic.AsSpan().SequenceEqual(Magic)) {
            throw new InvalidOperationException("The supplied payload is not one PS Vita compiled-shader material asset.");
        }
    }
}
