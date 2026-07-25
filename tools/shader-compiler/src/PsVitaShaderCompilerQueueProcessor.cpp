#include "PsVitaShaderCompilerQueueProcessor.hpp"

#include "PsVitaShaderCompilerJobReader.hpp"
#include "PsVitaShaderCompilerResultWriter.hpp"

#include "platform/psvita/shaders/PsVitaShaderArtifactFormat.hpp"
#include "platform/psvita/shaders/PsVitaShaderArtifactHash.hpp"
#include "platform/psvita/shaders/PsVitaShaderArtifactWriter.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>
#include <vector>

#include <psp2/shacccg.h>

namespace {
    /// Stores the maximum size of one submitted device shader source file.
    constexpr std::size_t MaximumSourceByteCount = 1024u * 1024u;

    /// Determines whether a directory already exists at one fixed Vita path.
    bool DirectoryExists(const std::string& path) {
        struct stat information {};
        return stat(path.c_str(), &information) == 0 && S_ISDIR(information.st_mode);
    }

    /// Reads one bounded file as binary text.
    bool TryReadBoundedFile(const std::string& path, std::string& content) {
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream.good()) {
            return false;
        }
        const std::streamoff size = stream.tellg();
        if (size <= 0 || static_cast<std::size_t>(size) > MaximumSourceByteCount) {
            return false;
        }

        content.resize(static_cast<std::size_t>(size));
        stream.seekg(0, std::ios::beg);
        stream.read(content.data(), size);
        return stream.good();
    }

    /// Reads one little-endian unsigned 32-bit artifact field.
    bool TryReadUInt32(std::ifstream& stream, std::uint32_t& value) {
        std::uint8_t bytes[4u] {};
        stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
        if (!stream.good()) {
            return false;
        }
        value = static_cast<std::uint32_t>(bytes[0u])
            | (static_cast<std::uint32_t>(bytes[1u]) << 8u)
            | (static_cast<std::uint32_t>(bytes[2u]) << 16u)
            | (static_cast<std::uint32_t>(bytes[3u]) << 24u);
        return true;
    }

    /// Reads one bounded string from a serialized PVSA artifact.
    bool TryReadArtifactString(std::ifstream& stream, std::string& value) {
        std::uint32_t size = 0u;
        if (!TryReadUInt32(stream, size) || size == 0u || size > 4096u) {
            return false;
        }
        value.resize(size);
        stream.read(value.data(), static_cast<std::streamsize>(size));
        return stream.good();
    }

    /// Reads the generated PVSA result metadata required by the host outbox protocol.
    bool TryReadArtifactOutcome(const std::string& path, std::uint32_t& programByteCount, std::string& artifactHash) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.good()) {
            return false;
        }
        std::uint8_t magic[4u] {};
        stream.read(reinterpret_cast<char*>(magic), sizeof(magic));
        std::uint32_t version = 0u;
        std::string stageProfile;
        std::string compilerVersion;
        std::string sourceHash;
        std::string entryPoint;
        std::string optionsSignature;
        if (!stream.good()
            || std::memcmp(magic, helengine::psvita::shaders::PsVitaShaderArtifactMagic, sizeof(magic)) != 0
            || !TryReadUInt32(stream, version)
            || version != helengine::psvita::shaders::PsVitaShaderArtifactVersion
            || !TryReadArtifactString(stream, stageProfile)
            || !TryReadArtifactString(stream, compilerVersion)
            || !TryReadArtifactString(stream, sourceHash)
            || !TryReadArtifactString(stream, entryPoint)
            || !TryReadArtifactString(stream, optionsSignature)
            || !TryReadUInt32(stream, programByteCount)
            || programByteCount == 0u) {
            return false;
        }
        stream.seekg(programByteCount, std::ios::cur);
        return stream.good() && TryReadArtifactString(stream, artifactHash) && artifactHash.size() == 64u;
    }

    /// Fills unprocessed stages with required result records after one job failure.
    void AppendUnprocessedFailures(
        const helengine::psvita::shadercompiler::PsVitaShaderCompilerJob& job,
        std::size_t nextStageIndex,
        std::vector<helengine::psvita::shadercompiler::PsVitaShaderCompilerStageOutput>& outputs) {
        for (std::size_t index = nextStageIndex; index < job.Stages.size(); ++index) {
            outputs.push_back({ job.Stages[index].StageId, false, "not-compiled-after-previous-stage-failure", std::string(), std::string(), 0u });
        }
    }
}

namespace helengine::psvita::shadercompiler {
    /// Initializes a processor for its fixed device storage paths.
    PsVitaShaderCompilerQueueProcessor::PsVitaShaderCompilerQueueProcessor(std::string inboxManifestPath, std::string inboxRootPath, std::string outboxRootPath)
        : InboxManifestPath(std::move(inboxManifestPath)), InboxRootPath(std::move(inboxRootPath)), OutboxRootPath(std::move(outboxRootPath)) {
    }

    /// Processes the single job that was copied into the fixed inbox before the VPK launch.
    int PsVitaShaderCompilerQueueProcessor::ProcessSingleJob() {
        PsVitaShaderCompilerJob job;
        std::string diagnostic;
        PsVitaShaderCompilerJobReader jobReader;
        if (!jobReader.TryRead(InboxManifestPath, job, diagnostic)) {
            return -1;
        }
        if (!EnsureDirectoryChain(OutboxRootPath)) {
            return -2;
        }

        const std::string temporaryDirectory = OutboxRootPath + "/" + job.JobHash + ".tmp";
        const std::string completedDirectory = OutboxRootPath + "/" + job.JobHash;
        if (DirectoryExists(temporaryDirectory) || DirectoryExists(completedDirectory) || !EnsureDirectoryChain(temporaryDirectory)) {
            return -3;
        }

        std::vector<PsVitaShaderCompilerStageOutput> outputs;
        bool allStagesSucceeded = true;
        for (std::size_t index = 0u; index < job.Stages.size(); ++index) {
            const PsVitaShaderCompilerStage& stage = job.Stages[index];
            std::string sourceText;
            if (!TryReadSource(stage.SourcePath, sourceText, diagnostic)) {
                outputs.push_back({ stage.StageId, false, diagnostic, std::string(), std::string(), 0u });
                AppendUnprocessedFailures(job, index + 1u, outputs);
                allStagesSucceeded = false;
                break;
            }

            const std::string artifactFileName = stage.StageId + ".pvsa";
            const std::string artifactPath = temporaryDirectory + "/" + artifactFileName;
            const std::string sourceHash = shaders::PsVitaShaderArtifactHash::Compute(
                reinterpret_cast<const std::uint8_t*>(sourceText.data()), sourceText.size());
            shaders::PsVitaShaderArtifactRequest request {
                sourceText,
                stage.SourcePath,
                artifactPath,
                stage.Profile == "VP" ? SCE_SHACCCG_PROFILE_VP : SCE_SHACCCG_PROFILE_FP,
                stage.EntryPoint,
                sourceHash,
                stage.OptionsSignature
            };
            shaders::PsVitaShaderArtifactWriter artifactWriter;
            if (artifactWriter.Write(request) != 0) {
                outputs.push_back({ stage.StageId, false, artifactWriter.GetLastDiagnostic(), std::string(), std::string(), 0u });
                AppendUnprocessedFailures(job, index + 1u, outputs);
                allStagesSucceeded = false;
                break;
            }

            std::uint32_t programByteCount = 0u;
            std::string artifactHash;
            if (!TryReadArtifactOutcome(artifactPath, programByteCount, artifactHash)) {
                outputs.push_back({ stage.StageId, false, "artifact-metadata-read-failed", std::string(), std::string(), 0u });
                AppendUnprocessedFailures(job, index + 1u, outputs);
                allStagesSucceeded = false;
                break;
            }
            outputs.push_back({ stage.StageId, true, std::string(), artifactFileName, artifactHash, programByteCount });
        }

        PsVitaShaderCompilerResultWriter resultWriter;
        if (!resultWriter.TryWrite(temporaryDirectory + "/results.json", job.JobHash, outputs)) {
            return -4;
        }
        if (std::rename(temporaryDirectory.c_str(), completedDirectory.c_str()) != 0) {
            return -5;
        }

        return allStagesSucceeded ? 0 : 1;
    }

    /// Creates fixed Vita outbox directories without removing paths created by another job.
    bool PsVitaShaderCompilerQueueProcessor::EnsureDirectoryChain(const std::string& path) {
        if (path.empty()) {
            return false;
        }

        std::string current;
        for (std::size_t index = 0u; index < path.size(); ++index) {
            current.push_back(path[index]);
            if (path[index] != '/' || current.size() <= 1u) {
                continue;
            }
            if (!current.empty() && !DirectoryExists(current) && mkdir(current.c_str(), 0777) != 0 && errno != EEXIST) {
                return false;
            }
        }
        return DirectoryExists(path) || (mkdir(path.c_str(), 0777) == 0 || errno == EEXIST);
    }

    /// Reads one bounded stage source beneath the fixed compiler inbox root.
    bool PsVitaShaderCompilerQueueProcessor::TryReadSource(const std::string& relativePath, std::string& sourceText, std::string& diagnostic) const {
        if (relativePath.empty() || relativePath.find("..") != std::string::npos || relativePath.front() == '/' || relativePath.front() == '\\') {
            diagnostic = "source-path-invalid";
            return false;
        }
        if (!TryReadBoundedFile(InboxRootPath + "/" + relativePath, sourceText)) {
            diagnostic = "source-read-failed-or-size-invalid";
            return false;
        }

        diagnostic.clear();
        return true;
    }
}
