#include "platform/psvita/shaders/PsVitaShaderArtifactReader.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "platform/psvita/shaders/PsVitaShaderArtifactFormat.hpp"

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <malloc.h>
#include <vector>

#include <psp2/gxm.h>

#include "system/io/file.hpp"
#include "platform/psvita/shaders/PsVitaShaderArtifactHash.hpp"

namespace {
    /// Reads exact bytes from one staged file stream.
    bool ReadExact(FileStream* stream, std::uint8_t* destination, std::size_t byteCount) {
        return stream != nullptr && destination != nullptr && stream->Read(destination, 0u, byteCount) == byteCount;
    }

    /// Reads one little-endian unsigned 32-bit field.
    bool ReadUInt32(FileStream* stream, std::uint32_t* value) {
        std::uint8_t bytes[4];
        if (value == nullptr || !ReadExact(stream, bytes, sizeof(bytes))) {
            return false;
        }

        *value = static_cast<std::uint32_t>(bytes[0])
            | (static_cast<std::uint32_t>(bytes[1]) << 8u)
            | (static_cast<std::uint32_t>(bytes[2]) << 16u)
            | (static_cast<std::uint32_t>(bytes[3]) << 24u);
        return true;
    }

    /// Reads one bounded UTF-8 string from one artifact stream.
    bool ReadString(FileStream* stream, std::string& value) {
        std::uint32_t byteCount = 0u;
        if (!ReadUInt32(stream, &byteCount) || byteCount == 0u || byteCount > 4096u) {
            return false;
        }

        std::vector<std::uint8_t> bytes(byteCount);
        if (!ReadExact(stream, bytes.data(), bytes.size())) {
            return false;
        }

        value.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }

    /// Reads the complete artifact payload after its metadata has been validated.
    bool ReadProgramPayload(FileStream* stream, std::uint32_t byteCount, std::vector<std::uint8_t>& payload) {
        if (stream == nullptr || byteCount == 0u || byteCount > 4u * 1024u * 1024u) {
            return false;
        }

        payload.resize(byteCount);
        return ReadExact(stream, payload.data(), payload.size());
    }

    /// Appends one little-endian unsigned 32-bit field to canonical hash input.
    void AppendUInt32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xFFu));
        bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xFFu));
    }

    /// Appends one serialized string to canonical hash input.
    void AppendString(std::vector<std::uint8_t>& bytes, const std::string& value) {
        AppendUInt32(bytes, static_cast<std::uint32_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    /// Verifies the artifact hash over metadata and the complete compiler payload.
    bool VerifyArtifactHash(const std::string& stageProfile, const std::string& compilerVersion, const std::string& sourceHash, const std::string& entryPoint, const std::string& optionsSignature, const std::vector<std::uint8_t>& payload, const std::string& expectedHash) {
        std::vector<std::uint8_t> canonical;
        canonical.insert(canonical.end(), helengine::psvita::shaders::PsVitaShaderArtifactMagic, helengine::psvita::shaders::PsVitaShaderArtifactMagic + 4);
        AppendUInt32(canonical, helengine::psvita::shaders::PsVitaShaderArtifactVersion);
        AppendString(canonical, stageProfile);
        AppendString(canonical, compilerVersion);
        AppendString(canonical, sourceHash);
        AppendString(canonical, entryPoint);
        AppendString(canonical, optionsSignature);
        AppendUInt32(canonical, static_cast<std::uint32_t>(payload.size()));
        canonical.insert(canonical.end(), payload.begin(), payload.end());

        std::string actualHash = helengine::psvita::shaders::PsVitaShaderArtifactHash::Compute(canonical.data(), canonical.size());
        if (actualHash.empty() || expectedHash.size() != actualHash.size()) {
            return false;
        }

        for (std::size_t index = 0u; index < actualHash.size(); ++index) {
            if (std::toupper(static_cast<unsigned char>(expectedHash[index])) != actualHash[index]) {
                return false;
            }
        }
        return true;
    }
}

namespace helengine::psvita::shaders {
    /// Reads and validates one stage artifact, returning an owned program header allocation.
    bool PsVitaShaderArtifactReader::TryRead(const std::string& path, const std::string& expectedStageProfile, void** programData, std::size_t* programSize) {
        if (path.empty() || expectedStageProfile.empty() || programData == nullptr || programSize == nullptr) {
            return false;
        }

        *programData = nullptr;
        *programSize = 0u;
        FileStream* stream = ::File::OpenRead(path);
        if (stream == nullptr) {
            return false;
        }

        bool succeeded = false;
        try {
            std::uint8_t magic[sizeof(PsVitaShaderArtifactMagic)];
            std::uint32_t version = 0u;
            std::string stageProfile;
            std::string compilerVersion;
            std::string sourceHash;
            std::string entryPoint;
            std::string optionsSignature;
            std::uint32_t payloadSize = 0u;
            std::vector<std::uint8_t> payload;
            std::string artifactHash;

            succeeded = ReadExact(stream, magic, sizeof(magic))
                && std::memcmp(magic, PsVitaShaderArtifactMagic, sizeof(magic)) == 0
                && ReadUInt32(stream, &version)
                && version == PsVitaShaderArtifactVersion
                && ReadString(stream, stageProfile)
                && stageProfile == expectedStageProfile
                && ReadString(stream, compilerVersion)
                && ReadString(stream, sourceHash)
                && ReadString(stream, entryPoint)
                && ReadString(stream, optionsSignature)
                && ReadUInt32(stream, &payloadSize)
                && ReadProgramPayload(stream, payloadSize, payload)
                && ReadString(stream, artifactHash);
            succeeded = succeeded && VerifyArtifactHash(stageProfile, compilerVersion, sourceHash, entryPoint, optionsSignature, payload, artifactHash);
            if (!succeeded) {
                delete stream;
                return false;
            }

            void* copiedProgram = memalign(16u, payload.size());
            if (copiedProgram == nullptr) {
                delete stream;
                return false;
            }

            std::memcpy(copiedProgram, payload.data(), payload.size());
            if (sceGxmProgramCheck(static_cast<const SceGxmProgram*>(copiedProgram)) < 0) {
                std::free(copiedProgram);
                delete stream;
                return false;
            }

            *programData = copiedProgram;
            *programSize = payload.size();
            delete stream;
            return true;
        } catch (...) {
            delete stream;
            throw;
        }
    }

    /// Reads one complete in-memory artifact stored by the runtime shader bundle.
    bool PsVitaShaderArtifactReader::TryReadBytes(const std::vector<std::uint8_t>& artifactBytes, const std::string& expectedStageProfile, void** programData, std::size_t* programSize) {
        if (artifactBytes.empty() || expectedStageProfile.empty() || programData == nullptr || programSize == nullptr) return false;
        *programData = nullptr;
        *programSize = 0u;
        std::size_t offset = 0u;
        auto readBytes = [&artifactBytes, &offset](std::uint8_t* destination, std::size_t count) {
            if (destination == nullptr || count > artifactBytes.size() - offset) return false;
            std::memcpy(destination, artifactBytes.data() + offset, count);
            offset += count;
            return true;
        };
        auto readUInt32 = [&readBytes](std::uint32_t* value) {
            std::uint8_t bytes[4];
            if (value == nullptr || !readBytes(bytes, sizeof(bytes))) return false;
            *value = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8u) | (static_cast<std::uint32_t>(bytes[2]) << 16u) | (static_cast<std::uint32_t>(bytes[3]) << 24u);
            return true;
        };
        auto readString = [&artifactBytes, &offset, &readBytes, &readUInt32](std::string& value) {
            std::uint32_t count = 0u;
            if (!readUInt32(&count) || count == 0u || count > 4096u || count > artifactBytes.size() - offset) return false;
            value.assign(reinterpret_cast<const char*>(artifactBytes.data() + offset), count);
            offset += count;
            return true;
        };
        std::uint8_t magic[4]; std::uint32_t version = 0u; std::string stageProfile; std::string compilerVersion; std::string sourceHash; std::string entryPoint; std::string optionsSignature; std::uint32_t payloadSize = 0u; std::string artifactHash;
        if (!readBytes(magic, sizeof(magic)) || std::memcmp(magic, PsVitaShaderArtifactMagic, sizeof(magic)) != 0 || !readUInt32(&version) || version != PsVitaShaderArtifactVersion || !readString(stageProfile) || stageProfile != expectedStageProfile || !readString(compilerVersion) || !readString(sourceHash) || !readString(entryPoint) || !readString(optionsSignature) || !readUInt32(&payloadSize) || payloadSize == 0u || payloadSize > 4u * 1024u * 1024u || payloadSize > artifactBytes.size() - offset) return false;
        std::vector<std::uint8_t> payload(payloadSize);
        if (!readBytes(payload.data(), payload.size()) || !readString(artifactHash) || offset != artifactBytes.size() || !VerifyArtifactHash(stageProfile, compilerVersion, sourceHash, entryPoint, optionsSignature, payload, artifactHash)) return false;
        void* copiedProgram = memalign(16u, payload.size());
        if (copiedProgram == nullptr) return false;
        std::memcpy(copiedProgram, payload.data(), payload.size());
        if (sceGxmProgramCheck(static_cast<const SceGxmProgram*>(copiedProgram)) < 0) { std::free(copiedProgram); return false; }
        *programData = copiedProgram;
        *programSize = payload.size();
        return true;
    }

    /// Releases one program allocation returned by TryRead.
    void PsVitaShaderArtifactReader::Release(void* programData) {
        std::free(programData);
    }
}

#endif
