#include "platform/psvita/rendering/PsVitaPackedModelReader.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

#include "platform/psvita/rendering/PsVitaRuntimeModel.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeSubmesh.hpp"
#include "runtime/native_exceptions.hpp"
#include "runtime/native_string.hpp"

namespace {
    constexpr std::array<unsigned char, 4> PackedModelMagic = {'P', 'V', 'M', '1'};
    constexpr std::uint32_t PackedModelVersion = 1u;

    /// Reads one primitive value from the supplied packed-model file.
    template<typename TValue>
    TValue ReadValue(std::FILE* file) {
        TValue value {};
        if (std::fread(&value, sizeof(TValue), 1u, file) != 1u) {
            throw new InvalidOperationException("PS Vita packed model payload ended unexpectedly.");
        }

        return value;
    }

    /// Reads one UTF-8 string from the supplied packed-model file.
    std::string ReadString(std::FILE* file) {
        const std::int32_t byteCount = ReadValue<std::int32_t>(file);
        if (byteCount < 0) {
            throw new InvalidOperationException("PS Vita packed model string lengths cannot be negative.");
        } else if (byteCount == 0) {
            return std::string();
        }

        std::vector<char> bytes(static_cast<std::size_t>(byteCount));
        if (std::fread(bytes.data(), sizeof(char), bytes.size(), file) != bytes.size()) {
            throw new InvalidOperationException("PS Vita packed model string payload ended unexpectedly.");
        }

        return std::string(bytes.begin(), bytes.end());
    }

    /// Reads one three-component floating-point vector from the supplied packed-model file.
    ::float3 ReadFloat3(std::FILE* file) {
        return ::float3(
            ReadValue<float>(file),
            ReadValue<float>(file),
            ReadValue<float>(file));
    }
}

namespace helengine::psvita::rendering {
    /// Attempts to read one packed PS Vita model payload and returns <c>nullptr</c> when the file uses a different asset format.
    PsVitaRuntimeModel* PsVitaPackedModelReader::TryRead(const std::string& cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Cooked model asset path must be provided.", "cookedAssetPath");
        }

        std::FILE* file = std::fopen(cookedAssetPath.c_str(), "rb");
        if (file == nullptr) {
            throw new InvalidOperationException("PS Vita packed model payload could not be opened.");
        }

        try {
            std::array<unsigned char, 4> magic {};
            if (std::fread(magic.data(), sizeof(unsigned char), magic.size(), file) != magic.size()) {
                throw new InvalidOperationException("PS Vita packed model payload ended unexpectedly.");
            }

            if (magic != PackedModelMagic) {
                std::fclose(file);
                return nullptr;
            }

            const std::uint32_t version = ReadValue<std::uint32_t>(file);
            if (version != PackedModelVersion) {
                throw new InvalidOperationException("Unsupported PS Vita packed model payload version.");
            }

            const std::int32_t indexElementSizeBytes = ReadValue<std::int32_t>(file);
            if (indexElementSizeBytes != 2 && indexElementSizeBytes != 4) {
                throw new InvalidOperationException("PS Vita packed model index width must be either 2 or 4 bytes.");
            }

            const std::int32_t positionCount = ReadValue<std::int32_t>(file);
            if (positionCount < 0) {
                throw new InvalidOperationException("PS Vita packed model position counts cannot be negative.");
            }

            std::vector<::float3> positions;
            positions.reserve(static_cast<std::size_t>(positionCount));
            for (std::int32_t positionIndex = 0; positionIndex < positionCount; ++positionIndex) {
                positions.push_back(ReadFloat3(file));
            }

            ReadFloat3(file);
            ReadFloat3(file);

            const std::int32_t submeshCount = ReadValue<std::int32_t>(file);
            if (submeshCount < 0) {
                throw new InvalidOperationException("PS Vita packed model submesh counts cannot be negative.");
            }

            auto* runtimeModel = new PsVitaRuntimeModel(std::move(positions));
            auto* runtimeSubmeshes = new Array<rendering::PsVitaRuntimeSubmesh*>(submeshCount);
            for (std::int32_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex) {
                std::string materialSlotName = ReadString(file);
                const std::int32_t indexStart = ReadValue<std::int32_t>(file);
                const std::int32_t indexCount = ReadValue<std::int32_t>(file);
                const std::int32_t triangleIndexCount = ReadValue<std::int32_t>(file);
                if (triangleIndexCount < 0) {
                    throw new InvalidOperationException("PS Vita packed model triangle-index counts cannot be negative.");
                }

                std::vector<std::uint32_t> triangleIndices;
                triangleIndices.reserve(static_cast<std::size_t>(triangleIndexCount));
                for (std::int32_t triangleIndex = 0; triangleIndex < triangleIndexCount; ++triangleIndex) {
                    triangleIndices.push_back(indexElementSizeBytes == 2
                        ? static_cast<std::uint32_t>(ReadValue<std::uint16_t>(file))
                        : ReadValue<std::uint32_t>(file));
                }

                (*runtimeSubmeshes)[submeshIndex] = new PsVitaRuntimeSubmesh(
                    materialSlotName,
                    indexStart,
                    indexCount,
                    std::move(triangleIndices));
            }

            runtimeModel->SetSubmeshes(runtimeSubmeshes);
            std::fclose(file);
            return runtimeModel;
        } catch (...) {
            std::fclose(file);
            throw;
        }
    }
}

#endif
