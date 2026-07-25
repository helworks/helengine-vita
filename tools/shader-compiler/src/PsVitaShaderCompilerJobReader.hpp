#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace helengine::psvita::shadercompiler {
    /// Describes one stage requested by the host-side Vita shader compiler manifest.
    struct PsVitaShaderCompilerStage final {
        /// Stores the stable stage result identifier.
        std::string StageId;

        /// Stores the relative inbox source path.
        std::string SourcePath;

        /// Stores the shader entry point requested by the host.
        std::string EntryPoint;

        /// Stores the requested Vita compiler profile.
        std::string Profile;

        /// Stores the canonical compiler option signature.
        std::string OptionsSignature;
    };

    /// Describes one validated shader compiler job read from the fixed device inbox manifest.
    struct PsVitaShaderCompilerJob final {
        /// Stores the host-side job identity.
        std::string JobHash;

        /// Stores the ordered stage requests.
        std::vector<PsVitaShaderCompilerStage> Stages;
    };

    /// Reads and validates the narrow JSON manifest schema shared with the host exchange implementation.
    class PsVitaShaderCompilerJobReader final {
    public:
        /// Reads one complete compiler job from a manifest file.
        /// <param name="manifestPath">Fixed manifest location inside the Vita inbox.</param>
        /// <param name="job">Receives the validated job on success.</param>
        /// <param name="diagnostic">Receives a concise error description on failure.</param>
        /// <returns>True when the manifest is valid and complete.</returns>
        bool TryRead(const std::string& manifestPath, PsVitaShaderCompilerJob& job, std::string& diagnostic) const;

    private:
        /// Validates a parsed job after syntax-level JSON parsing has completed.
        /// <param name="job">Parsed job to validate.</param>
        /// <param name="diagnostic">Receives a concise error description on failure.</param>
        /// <returns>True when every job field is safe for fixed-path processing.</returns>
        static bool Validate(const PsVitaShaderCompilerJob& job, std::string& diagnostic);
    };
}
