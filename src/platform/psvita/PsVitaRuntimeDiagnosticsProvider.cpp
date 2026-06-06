#include "platform/psvita/PsVitaRuntimeDiagnosticsProvider.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdio>
#include <string>

namespace helengine::psvita {
    namespace {
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";
    }

    /// Captures one runtime memory snapshot when the generated core requests it.
    RuntimeMemoryDiagnosticsSnapshot* PsVitaRuntimeDiagnosticsProvider::CaptureSnapshot() {
        return nullptr;
    }

    /// Records one scene-transition stage emitted by the generated runtime scene manager.
    void PsVitaRuntimeDiagnosticsProvider::ReportSceneTransitionStage(std::string stage, std::string sceneId, int32_t loadedSceneCount, int32_t pendingOperationCount) {
        AppendTraceLine("RuntimeSceneTransition: stage=" + stage
            + " scene=" + sceneId
            + " loaded=" + std::to_string(loadedSceneCount)
            + " pending=" + std::to_string(pendingOperationCount));
    }

    /// Records one entity-disposal stage emitted during runtime scene transitions.
    void PsVitaRuntimeDiagnosticsProvider::ReportEntityDisposalStage(std::string stage, int32_t entityChildCount, int32_t componentCount, int32_t componentIndex) {
        AppendTraceLine("RuntimeEntityDisposal: stage=" + stage
            + " children=" + std::to_string(entityChildCount)
            + " components=" + std::to_string(componentCount)
            + " componentIndex=" + std::to_string(componentIndex));
    }

    /// Appends one runtime diagnostics line to the PS Vita boot trace.
    void PsVitaRuntimeDiagnosticsProvider::AppendTraceLine(const std::string& message) {
        std::FILE* file = std::fopen(BootTracePath, "a");
        if (file == nullptr) {
            return;
        }

        std::fputs(message.c_str(), file);
        std::fputc('\n', file);
        std::fclose(file);
    }
}

#endif
