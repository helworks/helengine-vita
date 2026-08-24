#include "platform/psvita/rendering/PsVitaGxmShadowSubmissionMemory.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <limits>
#include <string>

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

#include "platform/psvita/rendering/PsVitaGxmMemoryBlockSize.hpp"

namespace helengine::psvita::rendering {
    /// Creates one empty shadow-submission allocation owner.
    PsVitaGxmShadowSubmissionMemory::PsVitaGxmShadowSubmissionMemory()
        : MemoryBlockId(-1)
        , Data(nullptr)
        , LastDiagnostic() {
    }

    /// Releases any retained GXM mapping and kernel memory block.
    PsVitaGxmShadowSubmissionMemory::~PsVitaGxmShadowSubmissionMemory() {
        Reset();
    }

    /// Allocates one GPU-readable memory block with the requested byte size.
    bool PsVitaGxmShadowSubmissionMemory::Allocate(std::size_t sizeBytes) {
        Reset();
        LastDiagnostic.clear();
        const std::size_t allocationSizeBytes = CalculatePsVitaGxmMemoryBlockSize(
            sizeBytes,
            static_cast<std::size_t>(std::numeric_limits<SceSize>::max()));
        if (allocationSizeBytes == 0u) {
            LastDiagnostic = "size-alignment requested=" + std::to_string(sizeBytes);
            return false;
        }

        const int allocationResult = sceKernelAllocMemBlock(
            "HelenginePsVitaShadowSubmission",
            SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
            static_cast<SceSize>(allocationSizeBytes),
            nullptr);
        if (allocationResult < 0) {
            LastDiagnostic = "sceKernelAllocMemBlock result=" + std::to_string(allocationResult)
                + " requested=" + std::to_string(sizeBytes)
                + " aligned=" + std::to_string(allocationSizeBytes);
            MemoryBlockId = -1;
            return false;
        }
        MemoryBlockId = allocationResult;

        const int getBaseResult = sceKernelGetMemBlockBase(MemoryBlockId, &Data);
        if (getBaseResult < 0 || Data == nullptr) {
            LastDiagnostic = "sceKernelGetMemBlockBase result=" + std::to_string(getBaseResult)
                + " aligned=" + std::to_string(allocationSizeBytes);
            Reset();
            return false;
        }

        const int mapResult = sceGxmMapMemory(
            Data,
            static_cast<SceSize>(allocationSizeBytes),
            SCE_GXM_MEMORY_ATTRIB_READ);
        if (mapResult < 0) {
            LastDiagnostic = "sceGxmMapMemory result=" + std::to_string(mapResult)
                + " aligned=" + std::to_string(allocationSizeBytes);
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

    /// Gets the native operation and result details recorded by the most recent failed allocation attempt.
    const std::string& PsVitaGxmShadowSubmissionMemory::GetLastDiagnostic() const {
        return LastDiagnostic;
    }
}

#endif
