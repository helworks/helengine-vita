#pragma once

#include <cstdint>
#include <string>

#include <psp2/shacccg.h>

namespace helengine::psvita::shaders {
    /// Describes one source compilation request issued by the Vita shader artifact exporter.
    struct PsVitaShaderArtifactRequest final {
        /// Stores the source text submitted to the Vita compiler.
        std::string SourceText;

        /// Stores the source filename reported in compiler diagnostics.
        std::string SourceFileName;

        /// Stores the output artifact path written by the exporter.
        std::string OutputPath;

        /// Stores the compiler stage profile requested by the export command.
        SceShaccCgTargetProfile TargetProfile;

        /// Stores the entry function compiled from the source text.
        std::string EntryPoint;

        /// Stores the source hash used by the host cache key.
        std::string SourceHash;

        /// Stores the canonical compiler option signature used by the host cache key.
        std::string OptionsSignature;
    };

    /// Compiles one shader stage through the Vita runtime compiler and writes its complete program payload to disk.
    class PsVitaShaderArtifactWriter final {
    public:
        /// Compiles and writes one shader artifact, returning zero on success or a negative error code on failure.
        /// <param name="request">Source, compiler, and output settings.</param>
        /// <returns>Zero for success or a negative failure code.</returns>
        int Write(const PsVitaShaderArtifactRequest& request);

        /// Gets the detailed outcome from the most recent write attempt for hardware-side export diagnostics.
        /// <returns>Empty when the most recent write completed successfully.</returns>
        const std::string& GetLastDiagnostic() const;

    private:
        /// Stores the detailed outcome from the most recent compiler or artifact-write failure.
        std::string LastDiagnostic;

        /// Compiles one requested stage and returns a copied compiler output payload.
        int Compile(const PsVitaShaderArtifactRequest& request, std::uint8_t** programData, std::uint32_t* programSize, std::string& compilerVersion);

        /// Copies compiler diagnostics from one output into the persisted exporter diagnostic message.
        void CaptureCompilerDiagnostics(const SceShaccCgCompileOutput* output);

        /// Allocates memory for the compiler's internal lifetime using the process heap.
        /// <param name="size">Number of bytes requested by the compiler.</param>
        /// <returns>Allocated memory or null when the request cannot be satisfied.</returns>
        static void* AllocateCompilerMemory(unsigned int size);

        /// Releases memory previously allocated through AllocateCompilerMemory.
        /// <param name="memory">Compiler-owned allocation to release.</param>
        static void ReleaseCompilerMemory(void* memory);

        /// Writes one complete artifact with explicit little-endian metadata and payload lengths.
        bool WriteArtifact(const PsVitaShaderArtifactRequest& request, const std::string& compilerVersion, const std::uint8_t* programData, std::uint32_t programSize) const;
    };
}
