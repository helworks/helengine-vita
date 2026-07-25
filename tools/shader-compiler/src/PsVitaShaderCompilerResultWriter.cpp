#include "PsVitaShaderCompilerResultWriter.hpp"

#include <fstream>

namespace helengine::psvita::shadercompiler {
    /// Writes one strict host-compatible result manifest.
    bool PsVitaShaderCompilerResultWriter::TryWrite(const std::string& resultPath, const std::string& jobHash, const std::vector<PsVitaShaderCompilerStageOutput>& stages) const {
        std::ofstream stream(resultPath, std::ios::binary | std::ios::trunc);
        if (!stream.good() || jobHash.empty() || stages.empty()) {
            return false;
        }

        stream << "{\"formatVersion\":1,\"jobHash\":\"" << EscapeJsonString(jobHash) << "\",\"stages\":[";
        for (std::size_t index = 0u; index < stages.size(); ++index) {
            const PsVitaShaderCompilerStageOutput& stage = stages[index];
            if (index != 0u) {
                stream << ',';
            }
            stream << "{\"stageId\":\"" << EscapeJsonString(stage.StageId)
                << "\",\"success\":" << (stage.Success ? "true" : "false")
                << ",\"diagnostic\":\"" << EscapeJsonString(stage.Diagnostic)
                << "\",\"artifactPath\":\"" << EscapeJsonString(stage.ArtifactPath)
                << "\",\"artifactHash\":\"" << EscapeJsonString(stage.ArtifactHash)
                << "\",\"programByteCount\":" << stage.ProgramByteCount << '}';
        }
        stream << "]}";
        stream.flush();
        return stream.good();
    }

    /// Escapes control and quotation characters used by the compiler diagnostics.
    std::string PsVitaShaderCompilerResultWriter::EscapeJsonString(const std::string& value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (char character : value) {
            if (character == '"') {
                escaped += "\\\"";
            } else if (character == '\\') {
                escaped += "\\\\";
            } else if (character == '\n') {
                escaped += "\\n";
            } else if (character == '\r') {
                escaped += "\\r";
            } else if (character == '\t') {
                escaped += "\\t";
            } else if (static_cast<unsigned char>(character) < 0x20u) {
                escaped += ' ';
            } else {
                escaped += character;
            }
        }
        return escaped;
    }
}
