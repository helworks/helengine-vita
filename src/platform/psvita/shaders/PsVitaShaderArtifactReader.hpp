#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace helengine::psvita::shaders {
    /// Loads one complete Vita shader artifact into persistent aligned memory for GXM registration.
    class PsVitaShaderArtifactReader final {
    public:
        /// Reads and validates one stage artifact, returning an owned program header allocation.
        /// <param name="path">Staged artifact path.</param>
        /// <param name="expectedStageProfile">Expected VP or FP profile.</param>
        /// <param name="programData">Receives the allocated complete program blob.</param>
        /// <param name="programSize">Receives the program blob size.</param>
        /// <returns>True when the artifact is valid and GXM accepts its program header.</returns>
        static bool TryRead(const std::string& path, const std::string& expectedStageProfile, void** programData, std::size_t* programSize);

        /// Reads and validates one complete in-memory artifact stored by the runtime shader bundle.
        /// <param name="artifactBytes">Complete serialized PVSA artifact bytes.</param>
        /// <param name="expectedStageProfile">Expected VP or FP profile.</param>
        /// <param name="programData">Receives the allocated complete program blob.</param>
        /// <param name="programSize">Receives the program blob size.</param>
        /// <returns>True when the artifact is valid and GXM accepts its program header.</returns>
        static bool TryReadBytes(const std::vector<std::uint8_t>& artifactBytes, const std::string& expectedStageProfile, void** programData, std::size_t* programSize);

        /// Releases one program allocation returned by TryRead.
        /// <param name="programData">Program allocation to release.</param>
        static void Release(void* programData);
    };
}
