#pragma once

#include <cstdint>
#include <vector>

namespace helengine::psvita::rendering {
    /// Decodes the shared standard-material base-color constant buffer into Vita's packed ABGR color format.
    class PsVitaMaterialColorDecoder final {
    public:
        /// Decodes one little-endian float4 base-color payload.
        /// <param name="data">Exactly sixteen bytes containing finite red, green, blue, and alpha floats.</param>
        /// <returns>Packed ABGR color using the Vita renderer's byte ordering.</returns>
        static std::uint32_t DecodeBaseColorAbgr(const std::vector<std::uint8_t>& data);

    private:
        /// Converts one normalized color channel into an 8-bit channel value.
        static std::uint32_t PackChannel(float channel);
    };
}
