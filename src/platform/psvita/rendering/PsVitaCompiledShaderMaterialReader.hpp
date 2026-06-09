#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <string>

class FileStream;

namespace helengine::psvita::rendering {
    /// Stores one cooked PS Vita compiled-shader material payload read from staged content.
    struct PsVitaCompiledShaderMaterial final {
        /// Stores the shared shader asset identifier referenced by this cooked material.
        std::string ShaderAssetId;

        /// Stores the vertex-program name referenced by this cooked material.
        std::string VertexProgramName;

        /// Stores the pixel-program name referenced by this cooked material.
        std::string PixelProgramName;

        /// Stores the shader variant name referenced by this cooked material.
        std::string VariantName;

        /// Stores the packed ABGR base color referenced by this cooked material.
        std::uint32_t BaseColorAbgr = 0xFFFFFFFFu;
    };

    /// Reads the PS Vita cooked compiled-shader material payload emitted by the builder.
    class PsVitaCompiledShaderMaterialReader final {
    public:
        /// Tries to read one compiled-shader material payload from the supplied staged content path.
        static bool TryRead(const std::string& path, PsVitaCompiledShaderMaterial& material);

    private:
        /// Reads an exact byte count from the supplied stream.
        static bool TryReadExact(FileStream* stream, std::uint8_t* destination, std::size_t byteCount);

        /// Reads one little-endian unsigned 32-bit integer from the supplied stream.
        static bool TryReadUInt32(FileStream* stream, std::uint32_t* value);

        /// Reads one little-endian signed 32-bit integer from the supplied stream.
        static bool TryReadInt32(FileStream* stream, int32_t* value);

        /// Reads one UTF-8 string with an explicit byte-length prefix from the supplied stream.
        static bool TryReadString(FileStream* stream, std::string& value);
    };
}

#endif
