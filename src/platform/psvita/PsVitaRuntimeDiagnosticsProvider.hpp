#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "IRuntimeDiagnosticsProvider.hpp"
#include "IRuntimeEntityDisposalDiagnosticsProvider.hpp"
#include "IRuntimeSceneTransitionDiagnosticsProvider.hpp"

#include <string>

namespace helengine::psvita {
    /// Streams generated-core runtime diagnostics into the persisted PS Vita boot trace for emulator-side debugging.
    class PsVitaRuntimeDiagnosticsProvider final
        : public ::IRuntimeDiagnosticsProvider
        , public ::IRuntimeSceneTransitionDiagnosticsProvider
        , public ::IRuntimeEntityDisposalDiagnosticsProvider {
    public:
        /// Captures one runtime memory snapshot when the generated core requests it.
        ::RuntimeMemoryDiagnosticsSnapshot* CaptureSnapshot() override;

        /// Records one scene-transition stage emitted by the generated runtime scene manager.
        void ReportSceneTransitionStage(std::string stage, std::string sceneId, int32_t loadedSceneCount, int32_t pendingOperationCount) override;

        /// Records one entity-disposal stage emitted during runtime scene transitions.
        void ReportEntityDisposalStage(std::string stage, int32_t entityChildCount, int32_t componentCount, int32_t componentIndex) override;

    private:
        /// Appends one runtime diagnostics line to the PS Vita boot trace.
        void AppendTraceLine(const std::string& message);
    };
}

#endif
