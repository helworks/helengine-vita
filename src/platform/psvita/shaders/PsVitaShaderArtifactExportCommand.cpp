#include "platform/psvita/shaders/PsVitaShaderArtifactExportCommand.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <cstdio>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <string>

#include "platform/psvita/shaders/ForwardLambertShaderSource.hpp"
#include "platform/psvita/shaders/PsVitaShaderArtifactHash.hpp"
#include "platform/psvita/shaders/PsVitaShaderArtifactWriter.hpp"

namespace {
    /// Stores the retrieval directory used by the explicit hardware export command.
    constexpr const char* ExportDirectory = "ux0:data/helengine/shaders";

    /// Stores the source filename recorded in exported compiler diagnostics.
    constexpr const char* SourceFileName = "ForwardLambertShader.cg";

    /// Stores the Vita file receiving one export attempt's diagnostic trace.
    constexpr const char* ExportLogPath = "ux0:data/helengine/shader-export.log";

    /// Stores the compiler options represented by the exported artifacts.
    constexpr const char* OptionsSignature = "O3-W4";

    /// Creates the export directory hierarchy before writing artifacts.
    bool EnsureExportDirectory() {
        sceIoMkdir("ux0:data/helengine", 0777);
        sceIoMkdir(ExportDirectory, 0777);
        SceIoStat directoryStat;
        return sceIoGetstat(ExportDirectory, &directoryStat) >= 0;
    }

    /// Computes the uppercase source identity used by host artifact caching.
    std::string ComputeSourceHash(const std::string& source) {
        return helengine::psvita::shaders::PsVitaShaderArtifactHash::Compute(
            reinterpret_cast<const std::uint8_t*>(source.data()), source.size());
    }
}

namespace helengine::psvita::shaders {
    /// Appends one diagnostic line to the Vita export log so hardware failures remain inspectable after the process exits.
    void PsVitaShaderArtifactExportCommand::AppendLog(const std::string& message) const {
        const SceUID file = sceIoOpen(ExportLogPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0666);
        if (file < 0) {
            return;
        }

        sceIoWrite(file, message.data(), message.size());
        sceIoClose(file);
    }

    /// Compiles both forward-Lambert stages and writes complete artifacts to ux0 for retrieval.
    int PsVitaShaderArtifactExportCommand::Run() const {
        AppendLog("forward-lambert-export: started\n");
        const std::string source = GetForwardLambertShaderSource();
        const std::string sourceHash = ComputeSourceHash(source);
        const bool directoryReady = EnsureExportDirectory();
        if (source.empty() || sourceHash.empty() || !directoryReady) {
            AppendLog("forward-lambert-export: setup-failed\n");
            return -1;
        }

        PsVitaShaderArtifactWriter writer;
        PsVitaShaderArtifactRequest vertexRequest;
        vertexRequest.SourceText = source;
        vertexRequest.SourceFileName = SourceFileName;
        vertexRequest.OutputPath = std::string(ExportDirectory) + "/ForwardLambertShader.vp.pvsa";
        vertexRequest.TargetProfile = SCE_SHACCCG_PROFILE_VP;
        vertexRequest.EntryPoint = "VS";
        vertexRequest.SourceHash = sourceHash;
        vertexRequest.OptionsSignature = OptionsSignature;
        const int vertexResult = writer.Write(vertexRequest);
        if (vertexResult != 0) {
            AppendLog("forward-lambert-export: vertex-result=" + std::to_string(vertexResult) + " diagnostic=" + writer.GetLastDiagnostic() + "\n");
            return -2;
        }

        PsVitaShaderArtifactRequest fragmentRequest = vertexRequest;
        fragmentRequest.OutputPath = std::string(ExportDirectory) + "/ForwardLambertShader.fp.pvsa";
        fragmentRequest.TargetProfile = SCE_SHACCCG_PROFILE_FP;
        fragmentRequest.EntryPoint = "PS";
        const int fragmentResult = writer.Write(fragmentRequest);
        AppendLog("forward-lambert-export: fragment-result=" + std::to_string(fragmentResult) + " diagnostic=" + writer.GetLastDiagnostic() + "\n");
        return fragmentResult;
    }
}

#endif
