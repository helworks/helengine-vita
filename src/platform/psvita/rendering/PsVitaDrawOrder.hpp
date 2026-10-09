#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace helengine::psvita::rendering {
    /// Combines camera ordering with the untruncated ordinal of an already hierarchy-ordered draw request.
    constexpr std::uint64_t ComposePsVitaDrawOrder(std::uint16_t cameraOrder, std::uint32_t requestOrdinal) {
        return (static_cast<std::uint64_t>(cameraOrder) << 32u) | requestOrdinal;
    }

    /// Allocates request ordinals across ordered camera visits and resets them only when the complete frame drains.
    class PsVitaDrawRequestOrder final {
    public:
        /// Returns the next untruncated request ordinal; all subcommands of one request reuse this value.
        std::uint32_t Next() {
            if (Ordinal == std::numeric_limits<std::uint32_t>::max()) {
                throw std::overflow_error("PS Vita draw request count exceeds the native frame ordering range.");
            }
            return ++Ordinal;
        }

        /// Starts a fresh frame after the captured solid and textured command streams have drained.
        void Reset() {
            Ordinal = 0;
        }
    private:
        /// Counts complete draw requests without compressing hierarchy order into an eight-bit layer.
        std::uint32_t Ordinal = 0;
    };
}
