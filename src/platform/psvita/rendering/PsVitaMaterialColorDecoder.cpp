#include "platform/psvita/rendering/PsVitaMaterialColorDecoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace helengine::psvita::rendering {
    /// Decodes one little-endian float4 base-color payload.
    std::uint32_t PsVitaMaterialColorDecoder::DecodeBaseColorAbgr(const std::vector<std::uint8_t>& data) {
        if (data.size() != sizeof(float) * 4u) {
            throw std::runtime_error("PS Vita BaseColorBuffer must contain exactly four float channels.");
        }

        float channels[4];
        std::memcpy(channels, data.data(), sizeof(channels));
        for (float channel : channels) {
            if (!std::isfinite(channel)) {
                throw std::runtime_error("PS Vita BaseColorBuffer channels must be finite.");
            }
        }

        return PackChannel(channels[0])
            | (PackChannel(channels[1]) << 8u)
            | (PackChannel(channels[2]) << 16u)
            | (PackChannel(channels[3]) << 24u);
    }

    std::uint32_t PsVitaMaterialColorDecoder::PackChannel(float channel) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
    }
}
