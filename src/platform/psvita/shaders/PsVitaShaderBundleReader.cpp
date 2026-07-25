#include "platform/psvita/shaders/PsVitaShaderBundleReader.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstring>
#include <vector>

#include "system/io/file.hpp"

namespace {
    bool ReadExact(FileStream* stream, std::uint8_t* destination, std::size_t count) { return stream != nullptr && destination != nullptr && stream->Read(destination, 0u, count) == count; }
    bool ReadUInt32(FileStream* stream, std::uint32_t* value) { std::uint8_t bytes[4]; if (value == nullptr || !ReadExact(stream, bytes, sizeof(bytes))) return false; *value = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8u) | (static_cast<std::uint32_t>(bytes[2]) << 16u) | (static_cast<std::uint32_t>(bytes[3]) << 24u); return true; }
    bool ReadString(FileStream* stream, std::string& value) { std::uint32_t count = 0u; if (!ReadUInt32(stream, &count) || count == 0u || count > 4096u) return false; std::vector<std::uint8_t> bytes(count); if (!ReadExact(stream, bytes.data(), bytes.size())) return false; value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size()); return true; }
    bool ReadBlob(FileStream* stream, std::vector<std::uint8_t>& value) { std::uint32_t count = 0u; if (!ReadUInt32(stream, &count) || count == 0u || count > 4u * 1024u * 1024u) return false; value.resize(count); return ReadExact(stream, value.data(), value.size()); }
}

namespace helengine::psvita::shaders {
    bool PsVitaShaderBundleReader::Load(const std::string& path) {
        Entries.clear();
        FileStream* stream = ::File::OpenRead(path);
        if (stream == nullptr) return false;
        try {
            std::uint8_t magic[4]; std::uint32_t version = 0u; std::uint32_t count = 0u;
            if (!ReadExact(stream, magic, sizeof(magic)) || std::memcmp(magic, "PVSB", 4u) != 0 || !ReadUInt32(stream, &version) || version != 1u || !ReadUInt32(stream, &count) || count > 4096u) { delete stream; return false; }
            Entries.reserve(count);
            for (std::uint32_t index = 0u; index < count; ++index) {
                PsVitaShaderBundleEntry entry; std::string ignoredSourceHash;
                if (!ReadString(stream, entry.ShaderAssetId) || !ReadString(stream, ignoredSourceHash) || !ReadString(stream, entry.VertexProgramName) || !ReadString(stream, entry.PixelProgramName) || !ReadString(stream, entry.VariantName) || !ReadBlob(stream, entry.VertexArtifactBytes) || !ReadBlob(stream, entry.FragmentArtifactBytes)) { Entries.clear(); delete stream; return false; }
                Entries.push_back(entry);
            }
            delete stream;
            return true;
        } catch (...) { delete stream; Entries.clear(); throw; }
    }

    const PsVitaShaderBundleEntry* PsVitaShaderBundleReader::Find(const std::string& shaderAssetId, const std::string& vertexProgramName, const std::string& pixelProgramName, const std::string& variantName) const {
        for (const PsVitaShaderBundleEntry& entry : Entries) if (entry.ShaderAssetId == shaderAssetId && entry.VertexProgramName == vertexProgramName && entry.PixelProgramName == pixelProgramName && entry.VariantName == variantName) return &entry;
        return nullptr;
    }
}

#endif
