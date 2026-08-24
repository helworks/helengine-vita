#pragma once

#include <cstddef>

namespace helengine::psvita::rendering {
    /// Native page size required by ordinary PS Vita user memory blocks mapped for GXM access.
    inline constexpr std::size_t PsVitaGxmMemoryBlockAlignmentBytes = 4096u;

    /// Rounds one requested GXM allocation to a representable Vita memory-block size, returning zero for invalid requests.
    constexpr std::size_t CalculatePsVitaGxmMemoryBlockSize(std::size_t requestedSizeBytes, std::size_t maximumSizeBytes) {
        constexpr std::size_t alignmentMask = PsVitaGxmMemoryBlockAlignmentBytes - 1u;
        const std::size_t maximumAlignedSizeBytes = maximumSizeBytes & ~alignmentMask;
        if (requestedSizeBytes == 0u || requestedSizeBytes > maximumAlignedSizeBytes) {
            return 0u;
        }

        return (requestedSizeBytes + alignmentMask) & ~alignmentMask;
    }
}
