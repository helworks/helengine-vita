#include "platform/psvita/shaders/PsVitaShaderArtifactWriter.hpp"

#include "platform/psvita/shaders/PsVitaShaderArtifactFormat.hpp"

#include <cstdlib>
#include <cstring>
#include <climits>
#include <fstream>
#include <sstream>

#include <psp2/kernel/modulemgr.h>
#include <shacccg_ext.h>

#include "platform/psvita/shaders/PsVitaShaderArtifactHash.hpp"

namespace {
    /// Stores the synthetic filename used when the source callback has no include request.
    constexpr const char* DefaultSourceFileName = "HelengineShader.hlsl";

    /// Stores the PSM Runtime shader compiler module installed by the Vita homebrew environment.
    constexpr const char* ShaderCompilerModulePath = "ur0:/data/libshacccg.suprx";

    /// Stores the source currently being compiled for the synchronous compiler callback.
    std::string ActiveSourceText;

    /// Stores the source filename currently being compiled for the synchronous compiler callback.
    std::string ActiveSourceFileName;

    /// Provides the Vita compiler with the source text owned by one export request.
    SceShaccCgSourceFile* OpenShaderSourceFile(
        const char* fileName,
        const SceShaccCgSourceLocation* includedFrom,
        const SceShaccCgCompileOptions* compileOptions,
        const char** errorString) {
        (void)includedFrom;
        (void)compileOptions;
        static SceShaccCgSourceFile sourceFile;
        static std::string sourceText;
        static std::string sourceFileName;
        (void)compileOptions;
        sourceText = ActiveSourceText;
        sourceFileName = ActiveSourceFileName.empty() ? DefaultSourceFileName : ActiveSourceFileName;
        sourceFile.fileName = sourceFileName.c_str();
        sourceFile.text = sourceText.c_str();
        sourceFile.size = static_cast<SceUInt32>(sourceText.size());
        if (fileName == nullptr || std::strcmp(fileName, sourceFile.fileName) == 0) {
            return &sourceFile;
        }

        if (errorString != nullptr) {
            *errorString = "Vita shader artifact exporter could not resolve the requested source file.";
        }

        return nullptr;
    }

    /// Releases a source callback result that is owned by the exporter process.
    void ReleaseShaderSourceFile(const SceShaccCgSourceFile* file, const SceShaccCgCompileOptions* compileOptions) {
        (void)file;
        (void)compileOptions;
    }

    /// Writes one little-endian 32-bit unsigned field.
    void WriteUInt32(std::ofstream& stream, std::uint32_t value) {
        stream.put(static_cast<char>(value & 0xFFu));
        stream.put(static_cast<char>((value >> 8u) & 0xFFu));
        stream.put(static_cast<char>((value >> 16u) & 0xFFu));
        stream.put(static_cast<char>((value >> 24u) & 0xFFu));
    }

    /// Writes one UTF-8 string with a signed byte-length prefix compatible with the host serializer.
    bool WriteString(std::ofstream& stream, const std::string& value) {
        if (value.empty() || value.size() > static_cast<std::size_t>(INT32_MAX)) {
            return false;
        }

        WriteUInt32(stream, static_cast<std::uint32_t>(value.size()));
        stream.write(value.data(), static_cast<std::streamsize>(value.size()));
        return stream.good();
    }

    /// Computes one uppercase SHA-256 string over canonical artifact bytes.
    std::string ComputeHash(const std::string& stageProfile, const std::string& compilerVersion, const std::string& sourceHash, const std::string& entryPoint, const std::string& optionsSignature, const std::uint8_t* programData, std::uint32_t programSize) {
        std::ostringstream canonical;
        canonical.write(reinterpret_cast<const char*>(helengine::psvita::shaders::PsVitaShaderArtifactMagic), 4);
        canonical.put(1);
        canonical.put(0);
        canonical.put(0);
        canonical.put(0);
        auto appendString = [&canonical](const std::string& value) {
            std::uint32_t size = static_cast<std::uint32_t>(value.size());
            canonical.put(static_cast<char>(size & 0xFFu));
            canonical.put(static_cast<char>((size >> 8u) & 0xFFu));
            canonical.put(static_cast<char>((size >> 16u) & 0xFFu));
            canonical.put(static_cast<char>((size >> 24u) & 0xFFu));
            canonical.write(value.data(), static_cast<std::streamsize>(value.size()));
        };
        appendString(stageProfile);
        appendString(compilerVersion);
        appendString(sourceHash);
        appendString(entryPoint);
        appendString(optionsSignature);
        canonical.put(static_cast<char>(programSize & 0xFFu));
        canonical.put(static_cast<char>((programSize >> 8u) & 0xFFu));
        canonical.put(static_cast<char>((programSize >> 16u) & 0xFFu));
        canonical.put(static_cast<char>((programSize >> 24u) & 0xFFu));
        canonical.write(reinterpret_cast<const char*>(programData), static_cast<std::streamsize>(programSize));
        std::string canonicalBytes = canonical.str();
        return helengine::psvita::shaders::PsVitaShaderArtifactHash::Compute(
            reinterpret_cast<const std::uint8_t*>(canonicalBytes.data()), canonicalBytes.size());
    }
}

namespace helengine::psvita::shaders {
    /// Compiles and writes one shader artifact, returning zero on success or a negative error code on failure.
    int PsVitaShaderArtifactWriter::Write(const PsVitaShaderArtifactRequest& request) {
        LastDiagnostic.clear();
        if (request.SourceText.empty() || request.SourceFileName.empty() || request.OutputPath.empty() || request.EntryPoint.empty() || request.SourceHash.empty() || request.OptionsSignature.empty()) {
            LastDiagnostic = "artifact-request-invalid";
            return -1;
        } else if (request.TargetProfile != SCE_SHACCCG_PROFILE_VP && request.TargetProfile != SCE_SHACCCG_PROFILE_FP) {
            LastDiagnostic = "artifact-request-profile-invalid";
            return -1;
        }

        std::uint8_t* programData = nullptr;
        std::uint32_t programSize = 0u;
        std::string compilerVersion;
        const int compileResult = Compile(request, &programData, &programSize, compilerVersion);
        if (compileResult != 0) {
            return compileResult;
        }

        bool written = WriteArtifact(request, compilerVersion, programData, programSize);
        std::free(programData);
        if (!written) {
            LastDiagnostic = "artifact-write-failed";
            return -20;
        }

        return 0;
    }

    /// Gets the detailed outcome from the most recent write attempt for hardware-side export diagnostics.
    const std::string& PsVitaShaderArtifactWriter::GetLastDiagnostic() const {
        return LastDiagnostic;
    }

    /// Compiles one requested stage and returns a copied compiler output payload.
    int PsVitaShaderArtifactWriter::Compile(const PsVitaShaderArtifactRequest& request, std::uint8_t** programData, std::uint32_t* programSize, std::string& compilerVersion) {
        if (programData == nullptr || programSize == nullptr) {
            LastDiagnostic = "compiler-output-target-invalid";
            return -2;
        }

        const SceUID moduleId = sceKernelLoadStartModule(ShaderCompilerModulePath, 0, nullptr, 0, nullptr, nullptr);
        if (moduleId < 0) {
            LastDiagnostic = "compiler-module-load=" + std::to_string(moduleId) + " path=" + ShaderCompilerModulePath;
            return -10;
        }

        const int extensionsResult = sceShaccCgExtEnableExtensions();
        if (extensionsResult < 0) {
            LastDiagnostic = "compiler-extensions-enable=" + std::to_string(extensionsResult);
            sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
            return -11;
        }

        const int allocatorResult = sceShaccCgSetDefaultAllocator(&AllocateCompilerMemory, &ReleaseCompilerMemory);
        if (allocatorResult < 0) {
            LastDiagnostic = "compiler-allocator-initialize=" + std::to_string(allocatorResult);
            sceShaccCgExtDisableExtensions();
            sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
            return -12;
        }

        SceShaccCgCompileOptions options;
        const int optionsResult = sceShaccCgInitializeCompileOptions(&options);
        if (optionsResult < 0) {
            LastDiagnostic = "compiler-options-initialize=" + std::to_string(optionsResult);
            sceShaccCgReleaseCompiler();
            sceShaccCgExtDisableExtensions();
            sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
            return -13;
        }

        options.mainSourceFile = request.SourceFileName.c_str();
        options.targetProfile = request.TargetProfile;
        options.entryFunctionName = request.EntryPoint.c_str();
        options.useFx = 1;
        options.optimizationLevel = 3;
        options.warningLevel = 4;

        SceShaccCgCallbackList callbacks;
        sceShaccCgInitializeCallbackList(&callbacks, SCE_SHACCCG_TRIVIAL);
        callbacks.openFile = &OpenShaderSourceFile;
        callbacks.releaseFile = &ReleaseShaderSourceFile;

        ActiveSourceText = request.SourceText;
        ActiveSourceFileName = request.SourceFileName;

        const SceShaccCgCompileOutput* output = sceShaccCgCompileProgram(&options, &callbacks, 0);
        if (output == nullptr || output->programData == nullptr || output->programSize == 0u) {
            if (output != nullptr) {
                CaptureCompilerDiagnostics(output);
                sceShaccCgDestroyCompileOutput(output);
            } else {
                LastDiagnostic = "compiler-output-null";
            }
            ActiveSourceText.clear();
            ActiveSourceFileName.clear();
            sceShaccCgReleaseCompiler();
            sceShaccCgExtDisableExtensions();
            sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
            return -14;
        }

        std::uint8_t* copiedData = static_cast<std::uint8_t*>(std::malloc(output->programSize));
        if (copiedData == nullptr) {
            LastDiagnostic = "compiler-output-allocation=" + std::to_string(output->programSize);
            sceShaccCgDestroyCompileOutput(output);
            ActiveSourceText.clear();
            ActiveSourceFileName.clear();
            sceShaccCgReleaseCompiler();
            sceShaccCgExtDisableExtensions();
            sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
            return -15;
        }

        std::memcpy(copiedData, output->programData, output->programSize);
        *programData = copiedData;
        *programSize = output->programSize;
        const char* version = sceShaccCgGetVersionString();
        compilerVersion = version == nullptr ? "unknown" : version;
        sceShaccCgDestroyCompileOutput(output);
        ActiveSourceText.clear();
        ActiveSourceFileName.clear();
        sceShaccCgReleaseCompiler();
        sceShaccCgExtDisableExtensions();
        sceKernelStopUnloadModule(moduleId, 0, nullptr, 0, nullptr, nullptr);
        return 0;
    }

    /// Copies compiler diagnostics from one output into the persisted exporter diagnostic message.
    void PsVitaShaderArtifactWriter::CaptureCompilerDiagnostics(const SceShaccCgCompileOutput* output) {
        if (output == nullptr || output->diagnosticCount <= 0 || output->diagnostics == nullptr) {
            LastDiagnostic = "compiler-output-empty";
            return;
        }

        std::ostringstream diagnostics;
        for (int index = 0; index < output->diagnosticCount; index++) {
            const SceShaccCgDiagnosticMessage& diagnostic = output->diagnostics[index];
            if (index > 0) {
                diagnostics << " | ";
            }

            diagnostics << "compiler-diagnostic=" << diagnostic.code;
            if (diagnostic.location != nullptr) {
                diagnostics << " line=" << diagnostic.location->lineNumber;
            }

            if (diagnostic.message != nullptr) {
                diagnostics << " message=" << diagnostic.message;
            }
        }

        LastDiagnostic = diagnostics.str();
    }

    /// Allocates memory for the compiler's internal lifetime using the process heap.
    void* PsVitaShaderArtifactWriter::AllocateCompilerMemory(unsigned int size) {
        return std::malloc(static_cast<std::size_t>(size));
    }

    /// Releases memory previously allocated through AllocateCompilerMemory.
    void PsVitaShaderArtifactWriter::ReleaseCompilerMemory(void* memory) {
        std::free(memory);
    }

    /// Writes one complete artifact with explicit little-endian metadata and payload lengths.
    bool PsVitaShaderArtifactWriter::WriteArtifact(const PsVitaShaderArtifactRequest& request, const std::string& compilerVersion, const std::uint8_t* programData, std::uint32_t programSize) const {
        if (programData == nullptr || programSize == 0u) {
            return false;
        }

        const char* profileName = request.TargetProfile == SCE_SHACCCG_PROFILE_VP ? "VP" : "FP";
        std::ofstream stream(request.OutputPath, std::ios::binary | std::ios::trunc);
        if (!stream.good()) {
            return false;
        }

        stream.write(reinterpret_cast<const char*>(PsVitaShaderArtifactMagic), sizeof(PsVitaShaderArtifactMagic));
        WriteUInt32(stream, PsVitaShaderArtifactVersion);
        if (!WriteString(stream, profileName)
            || !WriteString(stream, compilerVersion)
            || !WriteString(stream, request.SourceHash)
            || !WriteString(stream, request.EntryPoint)
            || !WriteString(stream, request.OptionsSignature)) {
            return false;
        }

        WriteUInt32(stream, programSize);
        stream.write(reinterpret_cast<const char*>(programData), static_cast<std::streamsize>(programSize));
        if (!stream.good()) {
            return false;
        }

        return WriteString(stream, ComputeHash(profileName, compilerVersion, request.SourceHash, request.EntryPoint, request.OptionsSignature, programData, programSize));
    }
}
