#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace helengine::psvita::shadercompiler {
    /// Describes one stage outcome written into the Vita compiler outbox result manifest.
    struct PsVitaShaderCompilerStageOutput final {
        /// Stores the submitted stage identifier.
        std::string StageId;

        /// Stores whether the stage produced a complete PVSA artifact.
        bool Success;

        /// Stores the compiler diagnostic for a failed stage.
        std::string Diagnostic;

        /// Stores the artifact path relative to the completed job directory.
        std::string ArtifactPath;

        /// Stores the uppercase SHA-256 hash read from the generated PVSA artifact.
        std::string ArtifactHash;

        /// Stores the compiler program payload byte count.
        std::uint32_t ProgramByteCount;
    };

    /// Writes result manifests in the strict JSON schema consumed by the host compiler exchange.
    class PsVitaShaderCompilerResultWriter final {
    public:
        /// Writes one complete result manifest into a temporary outbox directory.
        /// <param name="resultPath">Result file path inside the temporary directory.</param>
        /// <param name="jobHash">Completed job identity.</param>
        /// <param name="stages">Complete stage outcomes in submitted order.</param>
        /// <returns>True when the result manifest was fully written.</returns>
        bool TryWrite(const std::string& resultPath, const std::string& jobHash, const std::vector<PsVitaShaderCompilerStageOutput>& stages) const;

    private:
        /// Escapes one UTF-8 string as a JSON string value.
        /// <param name="value">Text to encode.</param>
        /// <returns>JSON-safe string content without enclosing quotes.</returns>
        static std::string EscapeJsonString(const std::string& value);
    };
}
