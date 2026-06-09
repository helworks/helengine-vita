#include "platform/psvita/rendering/PsVitaCompiledShaderMaterialReader.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstring>
#include <stdexcept>
#include <vector>

#include "system/io/file.hpp"

namespace {
    /// Stores the stable four-byte magic prefix used to identify cooked PS Vita compiled-shader material payloads.
    constexpr std::uint8_t PsVitaCompiledShaderMaterialMagic[4] = { 'P', 'V', 'M', 'T' };

    /// Stores the supported cooked PS Vita compiled-shader material payload version.
    constexpr std::uint32_t PsVitaCompiledShaderMaterialVersion = 1u;
}

namespace helengine::psvita::rendering {
    /// Tries to read one compiled-shader material payload from the supplied staged content path.
    bool PsVitaCompiledShaderMaterialReader::TryRead(const std::string& path, PsVitaCompiledShaderMaterial& material) {
        if (path.empty()) {
            throw std::invalid_argument("PS Vita compiled-shader material reading requires one staged content path.");
        }

        FileStream* stream = nullptr;
        try {
            stream = ::File::OpenRead(path);

            std::uint8_t magic[sizeof(PsVitaCompiledShaderMaterialMagic)];
            if (!TryReadExact(stream, magic, sizeof(magic))) {
                delete stream;
                return false;
            }

            if (std::memcmp(magic, PsVitaCompiledShaderMaterialMagic, sizeof(magic)) != 0) {
                delete stream;
                return false;
            }

            std::uint32_t version = 0u;
            if (!TryReadUInt32(stream, &version)) {
                throw std::runtime_error("PS Vita compiled-shader material payloads must contain one version field.");
            } else if (version != PsVitaCompiledShaderMaterialVersion) {
                throw std::runtime_error("PS Vita compiled-shader material payload version is not supported.");
            }

            PsVitaCompiledShaderMaterial decodedMaterial;
            if (!TryReadString(stream, decodedMaterial.ShaderAssetId)
                || !TryReadString(stream, decodedMaterial.VertexProgramName)
                || !TryReadString(stream, decodedMaterial.PixelProgramName)
                || !TryReadString(stream, decodedMaterial.VariantName)
                || !TryReadUInt32(stream, &decodedMaterial.BaseColorAbgr)) {
                throw std::runtime_error("PS Vita compiled-shader material payload is truncated.");
            }

            delete stream;
            material = decodedMaterial;
            return true;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }

            throw;
        }
    }

    /// Reads an exact byte count from the supplied stream.
    bool PsVitaCompiledShaderMaterialReader::TryReadExact(FileStream* stream, std::uint8_t* destination, std::size_t byteCount) {
        if (stream == nullptr || destination == nullptr) {
            return false;
        }

        return stream->Read(destination, 0u, byteCount) == byteCount;
    }

    /// Reads one little-endian unsigned 32-bit integer from the supplied stream.
    bool PsVitaCompiledShaderMaterialReader::TryReadUInt32(FileStream* stream, std::uint32_t* value) {
        if (stream == nullptr || value == nullptr) {
            return false;
        }

        std::uint8_t bytes[4];
        if (!TryReadExact(stream, bytes, sizeof(bytes))) {
            return false;
        }

        *value = static_cast<std::uint32_t>(bytes[0])
            | (static_cast<std::uint32_t>(bytes[1]) << 8)
            | (static_cast<std::uint32_t>(bytes[2]) << 16)
            | (static_cast<std::uint32_t>(bytes[3]) << 24);
        return true;
    }

    /// Reads one little-endian signed 32-bit integer from the supplied stream.
    bool PsVitaCompiledShaderMaterialReader::TryReadInt32(FileStream* stream, int32_t* value) {
        if (value == nullptr) {
            return false;
        }

        std::uint32_t unsignedValue = 0u;
        if (!TryReadUInt32(stream, &unsignedValue)) {
            return false;
        }

        *value = static_cast<int32_t>(unsignedValue);
        return true;
    }

    /// Reads one UTF-8 string with an explicit byte-length prefix from the supplied stream.
    bool PsVitaCompiledShaderMaterialReader::TryReadString(FileStream* stream, std::string& value) {
        int32_t byteCount = 0;
        if (!TryReadInt32(stream, &byteCount)) {
            return false;
        } else if (byteCount < 0) {
            throw std::runtime_error("PS Vita compiled-shader material string lengths cannot be negative.");
        }

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(byteCount));
        if (byteCount > 0 && !TryReadExact(stream, bytes.data(), static_cast<std::size_t>(byteCount))) {
            return false;
        }

        value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }
}

#endif
