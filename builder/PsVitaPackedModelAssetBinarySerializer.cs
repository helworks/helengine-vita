using System.Text;

namespace helengine.psvita.builder;

/// <summary>
/// Serializes and deserializes the PS Vita packed model payload used to move triangle-list preparation out of the runtime load path.
/// </summary>
public static class PsVitaPackedModelAssetBinarySerializer {
    /// <summary>
    /// Stable four-byte magic prefix used to identify packed PS Vita model payloads.
    /// </summary>
    public static ReadOnlySpan<byte> Magic => "PVM1"u8;

    /// <summary>
    /// Stable packed model payload version.
    /// </summary>
    public const uint Version = 1u;

    /// <summary>
    /// Serializes one packed PS Vita model payload into bytes.
    /// </summary>
    /// <param name="asset">Packed model payload to encode.</param>
    /// <returns>Serialized payload bytes.</returns>
    public static byte[] SerializeToBytes(PsVitaPackedModelAsset asset) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (asset.Positions == null) {
            throw new ArgumentNullException(nameof(asset.Positions));
        } else if (asset.Submeshes == null) {
            throw new ArgumentNullException(nameof(asset.Submeshes));
        } else if (asset.IndexElementSizeBytes != 2 && asset.IndexElementSizeBytes != 4) {
            throw new InvalidOperationException("PS Vita packed model index width must be either 2 or 4 bytes.");
        }

        using MemoryStream stream = new();
        using BinaryWriter writer = new(stream, Encoding.UTF8, leaveOpen: true);

        writer.Write(Magic);
        writer.Write(Version);
        writer.Write(asset.IndexElementSizeBytes);
        writer.Write(asset.Positions.Length);
        for (int positionIndex = 0; positionIndex < asset.Positions.Length; positionIndex++) {
            WriteFloat3(writer, asset.Positions[positionIndex]);
        }

        WriteFloat3(writer, asset.BoundsMin);
        WriteFloat3(writer, asset.BoundsMax);
        writer.Write(asset.Submeshes.Length);
        for (int submeshIndex = 0; submeshIndex < asset.Submeshes.Length; submeshIndex++) {
            PsVitaPackedModelSubmesh submesh = asset.Submeshes[submeshIndex] ?? throw new InvalidOperationException("PS Vita packed model submeshes cannot contain null entries.");
            WriteString(writer, submesh.MaterialSlotName);
            writer.Write(submesh.IndexStart);
            writer.Write(submesh.IndexCount);
            writer.Write(submesh.TriangleIndices.Length);
            for (int triangleIndex = 0; triangleIndex < submesh.TriangleIndices.Length; triangleIndex++) {
                uint indexValue = submesh.TriangleIndices[triangleIndex];
                if (asset.IndexElementSizeBytes == 2) {
                    if (indexValue > ushort.MaxValue) {
                        throw new InvalidOperationException("PS Vita packed model 16-bit indices cannot exceed 65535.");
                    }

                    writer.Write((ushort)indexValue);
                } else {
                    writer.Write(indexValue);
                }
            }
        }

        return stream.ToArray();
    }

    /// <summary>
    /// Deserializes one packed PS Vita model payload from bytes.
    /// </summary>
    /// <param name="bytes">Serialized packed payload bytes.</param>
    /// <returns>Decoded packed model payload.</returns>
    public static PsVitaPackedModelAsset DeserializeFromBytes(byte[] bytes) {
        if (bytes == null) {
            throw new ArgumentNullException(nameof(bytes));
        }

        using MemoryStream stream = new(bytes, writable: false);
        using BinaryReader reader = new(stream, Encoding.UTF8, leaveOpen: true);
        ValidateMagic(reader);

        uint version = reader.ReadUInt32();
        if (version != Version) {
            throw new InvalidOperationException($"Unsupported PS Vita packed model payload version '{version}'.");
        }

        int indexElementSizeBytes = reader.ReadInt32();
        if (indexElementSizeBytes != 2 && indexElementSizeBytes != 4) {
            throw new InvalidOperationException("PS Vita packed model index width must be either 2 or 4 bytes.");
        }

        int positionCount = reader.ReadInt32();
        float3[] positions = new float3[positionCount];
        for (int positionIndex = 0; positionIndex < positionCount; positionIndex++) {
            positions[positionIndex] = ReadFloat3(reader);
        }

        float3 boundsMin = ReadFloat3(reader);
        float3 boundsMax = ReadFloat3(reader);

        int submeshCount = reader.ReadInt32();
        PsVitaPackedModelSubmesh[] submeshes = new PsVitaPackedModelSubmesh[submeshCount];
        for (int submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++) {
            string materialSlotName = ReadString(reader);
            int indexStart = reader.ReadInt32();
            int indexCount = reader.ReadInt32();
            int triangleIndexCount = reader.ReadInt32();
            uint[] triangleIndices = new uint[triangleIndexCount];
            for (int triangleIndex = 0; triangleIndex < triangleIndexCount; triangleIndex++) {
                triangleIndices[triangleIndex] = indexElementSizeBytes == 2
                    ? reader.ReadUInt16()
                    : reader.ReadUInt32();
            }

            submeshes[submeshIndex] = new PsVitaPackedModelSubmesh {
                MaterialSlotName = materialSlotName,
                IndexStart = indexStart,
                IndexCount = indexCount,
                TriangleIndices = triangleIndices
            };
        }

        return new PsVitaPackedModelAsset {
            Positions = positions,
            BoundsMin = boundsMin,
            BoundsMax = boundsMax,
            IndexElementSizeBytes = indexElementSizeBytes,
            Submeshes = submeshes
        };
    }

    /// <summary>
    /// Determines whether the supplied bytes start with the PS Vita packed model payload magic prefix.
    /// </summary>
    /// <param name="bytes">Serialized payload bytes to inspect.</param>
    /// <returns><c>true</c> when the payload uses the PS Vita packed model format.</returns>
    public static bool HasMagicPrefix(byte[] bytes) {
        if (bytes == null || bytes.Length < Magic.Length) {
            return false;
        }

        for (int index = 0; index < Magic.Length; index++) {
            if (bytes[index] != Magic[index]) {
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Packs one raw model asset into a PS Vita staged payload whose triangle lists are already resolved.
    /// </summary>
    /// <param name="asset">Raw model asset to normalize.</param>
    /// <returns>Serialized PS Vita packed model bytes.</returns>
    public static byte[] SerializeModelAssetToBytes(ModelAsset asset) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (asset.Positions == null || asset.Positions.Length == 0) {
            throw new InvalidOperationException("PS Vita packed model generation requires at least one position.");
        }

        ModelAssetIndexData indexData = ModelAssetIndexData.Resolve(asset);
        uint[] resolvedIndices = ResolveIndices(indexData);
        int indexElementSizeBytes = CanUse16BitIndices(asset, resolvedIndices) ? 2 : 4;
        PsVitaPackedModelSubmesh[] submeshes = ResolveSubmeshes(asset, resolvedIndices);

        return SerializeToBytes(new PsVitaPackedModelAsset {
            Positions = asset.Positions,
            BoundsMin = asset.BoundsMin,
            BoundsMax = asset.BoundsMax,
            IndexElementSizeBytes = indexElementSizeBytes,
            Submeshes = submeshes
        });
    }

    /// <summary>
    /// Writes one three-component floating-point vector.
    /// </summary>
    /// <param name="writer">Binary writer receiving the value.</param>
    /// <param name="value">Vector value to encode.</param>
    static void WriteFloat3(BinaryWriter writer, float3 value) {
        writer.Write(value.X);
        writer.Write(value.Y);
        writer.Write(value.Z);
    }

    /// <summary>
    /// Reads one three-component floating-point vector.
    /// </summary>
    /// <param name="reader">Binary reader supplying the value.</param>
    /// <returns>Decoded vector value.</returns>
    static float3 ReadFloat3(BinaryReader reader) {
        return new float3(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
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
            throw new InvalidOperationException("PS Vita packed model string lengths cannot be negative.");
        }

        return Encoding.UTF8.GetString(reader.ReadBytes(byteCount));
    }

    /// <summary>
    /// Validates that the reader is positioned at one PS Vita packed model payload.
    /// </summary>
    /// <param name="reader">Binary reader whose prefix should be validated.</param>
    static void ValidateMagic(BinaryReader reader) {
        byte[] magic = reader.ReadBytes(Magic.Length);
        if (magic.Length != Magic.Length || !magic.AsSpan().SequenceEqual(Magic)) {
            throw new InvalidOperationException("The supplied payload is not one PS Vita packed model asset.");
        }
    }

    /// <summary>
    /// Resolves one model asset into a flat unsigned 32-bit index buffer.
    /// </summary>
    /// <param name="indexData">Resolved index-buffer metadata.</param>
    /// <returns>Flattened index buffer.</returns>
    static uint[] ResolveIndices(ModelAssetIndexData indexData) {
        if (indexData == null) {
            throw new ArgumentNullException(nameof(indexData));
        }

        if (indexData.Uses32BitIndices) {
            return indexData.Indices32 == null
                ? []
                : [.. indexData.Indices32];
        }

        if (indexData.Indices16 == null || indexData.Indices16.Length == 0) {
            return [];
        }

        uint[] resolvedIndices = new uint[indexData.Indices16.Length];
        for (int index = 0; index < indexData.Indices16.Length; index++) {
            resolvedIndices[index] = indexData.Indices16[index];
        }

        return resolvedIndices;
    }

    /// <summary>
    /// Determines whether the packed model can use a 16-bit index width.
    /// </summary>
    /// <param name="asset">Model asset being packed.</param>
    /// <param name="resolvedIndices">Resolved flat index buffer.</param>
    /// <returns><c>true</c> when all indices fit into 16 bits.</returns>
    static bool CanUse16BitIndices(ModelAsset asset, uint[] resolvedIndices) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        }

        if ((asset.Positions?.Length ?? 0) > ushort.MaxValue) {
            return false;
        }

        for (int index = 0; index < resolvedIndices.Length; index++) {
            if (resolvedIndices[index] > ushort.MaxValue) {
                return false;
            }
        }

        return true;
    }

    /// <summary>
    /// Resolves authored submesh ranges into fully flattened triangle-index lists.
    /// </summary>
    /// <param name="asset">Model asset being packed.</param>
    /// <param name="resolvedIndices">Resolved flat index buffer.</param>
    /// <returns>Packed submesh collection.</returns>
    static PsVitaPackedModelSubmesh[] ResolveSubmeshes(ModelAsset asset, uint[] resolvedIndices) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        }

        int positionCount = asset.Positions == null ? 0 : asset.Positions.Length;
        int elementCount = resolvedIndices.Length == 0 ? positionCount : resolvedIndices.Length;
        if (asset.Submeshes == null || asset.Submeshes.Length == 0) {
            if (elementCount == 0) {
                return [];
            }

            return [
                new PsVitaPackedModelSubmesh {
                    MaterialSlotName = string.Empty,
                    IndexStart = 0,
                    IndexCount = elementCount,
                    TriangleIndices = CreateDefaultTriangleIndices(resolvedIndices, elementCount)
                }
            ];
        }

        PsVitaPackedModelSubmesh[] packedSubmeshes = new PsVitaPackedModelSubmesh[asset.Submeshes.Length];
        for (int submeshIndex = 0; submeshIndex < asset.Submeshes.Length; submeshIndex++) {
            ModelSubmeshAsset authoredSubmesh = asset.Submeshes[submeshIndex] ?? throw new InvalidOperationException("Model submesh collections cannot contain null entries.");
            if (authoredSubmesh.IndexStart < 0) {
                throw new InvalidOperationException("Model submesh index starts must be zero or greater.");
            } else if (authoredSubmesh.IndexCount <= 0) {
                throw new InvalidOperationException("Model submesh index counts must be greater than zero.");
            } else if (authoredSubmesh.IndexStart + authoredSubmesh.IndexCount > elementCount) {
                throw new InvalidOperationException("Model submesh ranges cannot exceed the resolved model element count.");
            }

            uint[] triangleIndices = new uint[authoredSubmesh.IndexCount];
            if (resolvedIndices.Length == 0) {
                for (int index = 0; index < authoredSubmesh.IndexCount; index++) {
                    triangleIndices[index] = (uint)(authoredSubmesh.IndexStart + index);
                }
            } else {
                Array.Copy(resolvedIndices, authoredSubmesh.IndexStart, triangleIndices, 0, authoredSubmesh.IndexCount);
            }

            packedSubmeshes[submeshIndex] = new PsVitaPackedModelSubmesh {
                MaterialSlotName = authoredSubmesh.MaterialSlotName,
                IndexStart = authoredSubmesh.IndexStart,
                IndexCount = authoredSubmesh.IndexCount,
                TriangleIndices = triangleIndices
            };
        }

        return packedSubmeshes;
    }

    /// <summary>
    /// Creates the default triangle list used when the authored model omitted explicit submesh ranges.
    /// </summary>
    /// <param name="resolvedIndices">Resolved flat index buffer.</param>
    /// <param name="elementCount">Resolved element count.</param>
    /// <returns>Default triangle index list.</returns>
    static uint[] CreateDefaultTriangleIndices(uint[] resolvedIndices, int elementCount) {
        if (resolvedIndices.Length != 0) {
            return [.. resolvedIndices];
        }

        uint[] defaultTriangleIndices = new uint[elementCount];
        for (int index = 0; index < elementCount; index++) {
            defaultTriangleIndices[index] = (uint)index;
        }

        return defaultTriangleIndices;
    }
}
