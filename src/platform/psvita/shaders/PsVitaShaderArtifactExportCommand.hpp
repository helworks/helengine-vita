#pragma once

#include <string>

namespace helengine::psvita::shaders {
    /// Runs the explicit PS Vita shader export command used to retrieve compiler artifacts from hardware.
    class PsVitaShaderArtifactExportCommand final {
    public:
        /// Compiles the forward-Lambert vertex and fragment stages into the Vita data partition.
        /// <returns>Zero when both artifacts were written successfully.</returns>
        int Run() const;

    private:
        /// Appends one diagnostic line to the Vita export log so hardware failures remain inspectable after the process exits.
        void AppendLog(const std::string& message) const;
    };
}
