#include "platform/psvita/rendering/PsVitaGxmMemoryBlockSize.hpp"

#include <cstddef>

using helengine::psvita::rendering::CalculatePsVitaGxmMemoryBlockSize;

static_assert(CalculatePsVitaGxmMemoryBlockSize(0u, 0xFFFFFFFFu) == 0u, "Zero-byte allocations must remain invalid.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(1u, 0xFFFFFFFFu) == 4096u, "The smallest GPU allocation must occupy one Vita memory page.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(4095u, 0xFFFFFFFFu) == 4096u, "Sub-page GPU allocations must round up to one Vita memory page.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(4096u, 0xFFFFFFFFu) == 4096u, "Page-aligned GPU allocations must preserve their size.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(4097u, 0xFFFFFFFFu) == 8192u, "GPU allocations crossing a page boundary must round up to the next page.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(0xFFFFF000u, 0xFFFFFFFFu) == 0xFFFFF000u, "The largest representable aligned Vita allocation must remain valid.");
static_assert(CalculatePsVitaGxmMemoryBlockSize(0xFFFFF001u, 0xFFFFFFFFu) == 0u, "Rounding beyond the Vita size limit must fail without overflowing.");
