#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace helengine::psvita::shaders {
    /// Computes SHA-256 digests without requiring an external Vita cryptography library.
    class PsVitaShaderArtifactHash final {
    public:
        /// Computes an uppercase hexadecimal SHA-256 digest for one byte sequence.
        /// <param name="bytes">Bytes to hash.</param>
        /// <param name="byteCount">Number of bytes to hash.</param>
        /// <returns>Uppercase hexadecimal digest, or an empty string for invalid input.</returns>
        static std::string Compute(const std::uint8_t* bytes, std::size_t byteCount);
    };
}
