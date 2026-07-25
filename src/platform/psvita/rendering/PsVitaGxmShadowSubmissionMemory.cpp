#include "platform/psvita/rendering/PsVitaGxmShadowSubmissionMemory.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <limits>

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

namespace helengine::psvita::rendering {
    /// Creates one empty shadow-submission allocation owner.
    PsVitaGxmShadowSubmissionMemory::PsVitaGxmShadowSubmissionMemory()
        : MemoryBlockId(-1)
        , Data(nullptr) {
    }

    /// Releases any retained GXM mapping and kernel memory block.
    PsVitaGxmShadowSubmissionMemory::~PsVitaGxmShadowSubmissionMemory() {
        Reset();
    }

    /// Allocates one GPU-readable memory block with the requested byte size.
    bool PsVitaGxmShadowSubmissionMemory::Allocate(std::size_t sizeBytes) {
        Reset();
        if (sizeBytes == 0u || sizeBytes > static_cast<std::size_t>(std::numeric_limits<SceSize>::max())) {
            return false;
        }

        MemoryBlockId = sceKernelAllocMemBlock(
            "HelenginePsVitaShadowSubmission",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            static_cast<SceSize>(sizeBytes),
            nullptr);
        if (MemoryBlockId < 0) {
            MemoryBlockId = -1;
            return false;
        }
        if (sceKernelGetMemBlockBase(MemoryBlockId, &Data) < 0 || Data == nullptr) {
            Reset();
            return false;
        }
        if (sceGxmMapMemory(Data, static_cast<SceSize>(sizeBytes), SCE_GXM_MEMORY_ATTRIB_READ) < 0) {
            Reset();
            return false;
        }

        return true;
    }

    /// Releases the owned GPU-visible allocation after its commands have completed.
    void PsVitaGxmShadowSubmissionMemory::Reset() {
        if (Data != nullptr) {
            sceGxmUnmapMemory(Data);
            Data = nullptr;
        }
        if (MemoryBlockId >= 0) {
            sceKernelFreeMemBlock(MemoryBlockId);
            MemoryBlockId = -1;
        }
    }

    /// Gets the base address used by GXM vertex and index streams.
    void* PsVitaGxmShadowSubmissionMemory::GetData() const {
        return Data;
    }
}

#endif
