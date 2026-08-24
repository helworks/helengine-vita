#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <string>

#include <psp2/types.h>

namespace helengine::psvita::rendering {
    /// Owns one GPU-visible shadow-caster submission until a completed frame makes its memory safe to reclaim.
    class PsVitaGxmShadowSubmissionMemory final {
    public:
        /// Creates one empty shadow-submission allocation owner.
        PsVitaGxmShadowSubmissionMemory();

        /// Releases any retained GXM mapping and kernel memory block.
        ~PsVitaGxmShadowSubmissionMemory();

        /// Allocates one GPU-readable memory block with the requested byte size.
        bool Allocate(std::size_t sizeBytes);

        /// Releases the owned GPU-visible allocation after its commands have completed.
        void Reset();

        /// Gets the base address used by GXM vertex and index streams.
        void* GetData() const;

        /// Gets the native operation and result details recorded by the most recent failed allocation attempt.
        const std::string& GetLastDiagnostic() const;

    private:
        /// Stores the kernel allocation that owns the mapped GPU memory.
        SceUID MemoryBlockId;

        /// Stores the CPU and GPU-visible base pointer for the allocation.
        void* Data;

        /// Stores actionable native failure details for the outer boot trace.
        std::string LastDiagnostic;
    };
}

#endif
