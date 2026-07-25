#pragma once

#include <string>

namespace helengine::psvita::shadercompiler {
    /// Processes exactly one fixed-inbox compiler job before the standalone VPK exits.
    class PsVitaShaderCompilerQueueProcessor final {
    public:
        /// Initializes a processor for the fixed Vita inbox manifest and outbox root.
        /// <param name="inboxManifestPath">Absolute fixed manifest path on the Vita storage device.</param>
        /// <param name="inboxRootPath">Absolute fixed inbox directory containing source files.</param>
        /// <param name="outboxRootPath">Absolute fixed outbox directory receiving completed jobs.</param>
        PsVitaShaderCompilerQueueProcessor(std::string inboxManifestPath, std::string inboxRootPath, std::string outboxRootPath);

        /// Reads, compiles, publishes, and returns after one job.
        /// <returns>Zero for a fully successful job or a nonzero failure status.</returns>
        int ProcessSingleJob();

    private:
        /// Stores the fixed manifest path read on launch.
        std::string InboxManifestPath;

        /// Stores the fixed inbox root that contains submitted stage source files.
        std::string InboxRootPath;

        /// Stores the fixed outbox root that receives atomically published results.
        std::string OutboxRootPath;

        /// Creates the required outbox directory chain without deleting existing job state.
        /// <param name="path">Directory chain to create.</param>
        /// <returns>True when the directory chain exists.</returns>
        static bool EnsureDirectoryChain(const std::string& path);

        /// Reads a bounded source file whose path remains beneath the fixed inbox root.
        /// <param name="relativePath">Validated relative source path from the manifest.</param>
        /// <param name="sourceText">Receives complete source text on success.</param>
        /// <param name="diagnostic">Receives a concise failure diagnostic.</param>
        /// <returns>True when source text was read within the byte limit.</returns>
        bool TryReadSource(const std::string& relativePath, std::string& sourceText, std::string& diagnostic) const;
    };
}
