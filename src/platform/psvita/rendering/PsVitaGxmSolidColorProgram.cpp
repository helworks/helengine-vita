#include "platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <string>

#include <psp2/kernel/modulemgr.h>
#include <psp2/shacccg.h>

extern "C" {
    extern SceGxmContext* _vita2d_context;
}

namespace {
    /// <summary>
    /// Stores the module path used to load the legal PS Vita runtime shader compiler.
    /// </summary>
    constexpr const char* ShaderCompilerModulePath = "ur0:data/libshacccg.suprx";

    /// <summary>
    /// Stores the synthetic source-file name passed to the runtime shader compiler for the shared solid-color shader.
    /// </summary>
    constexpr const char* ForwardSolidColorShaderFileName = "ForwardSolidColorShader.hlsl";

    /// <summary>
    /// Stores the runtime shader entry point used for the solid-color vertex program.
    /// </summary>
    constexpr const char* ForwardSolidColorVertexEntryPoint = "VS";

    /// <summary>
    /// Stores the runtime shader entry point used for the solid-color fragment program.
    /// </summary>
    constexpr const char* ForwardSolidColorFragmentEntryPoint = "PS";

    /// <summary>
    /// Stores the shared solid-color vertex position parameter name emitted by the runtime shader compiler.
    /// </summary>
    constexpr const char* ForwardSolidColorPositionParameterName = "pos";

    /// <summary>
    /// Stores the world-view-projection uniform name emitted by the shared solid-color vertex shader.
    /// </summary>
    constexpr const char* ForwardSolidColorWorldViewProjectionParameterName = "HelengineWorldViewProjection";

    /// <summary>
    /// Stores the base-color uniform name emitted by the shared solid-color fragment shader.
    /// </summary>
    constexpr const char* ForwardSolidColorBaseColorParameterName = "HelengineBaseColor";

    /// <summary>
    /// Stores the patcher backing-buffer size reserved for one minimal solid-color shader pair.
    /// </summary>
    constexpr unsigned int SolidColorShaderPatcherBufferSize = 64u * 1024u;

    /// <summary>
    /// Stores the vertex USSE buffer size reserved for one minimal solid-color shader pair.
    /// </summary>
    constexpr unsigned int SolidColorShaderPatcherVertexUsseSize = 16u * 1024u;

    /// <summary>
    /// Stores the fragment USSE buffer size reserved for one minimal solid-color shader pair.
    /// </summary>
    constexpr unsigned int SolidColorShaderPatcherFragmentUsseSize = 16u * 1024u;

    /// <summary>
    /// Stores the alignment used for mapped GPU-visible allocations that back the runtime shader patcher.
    /// </summary>
    constexpr unsigned int GxmMappedMemoryAlignment = 4096u;

    /// <summary>
    /// Stores the alignment used for persistent program-header copies kept after runtime shader compilation completes.
    /// </summary>
    constexpr unsigned int ProgramHeaderAlignment = 16u;

    /// <summary>
    /// Stores the shared HLSL source compiled at runtime for the temporary PS Vita solid-color mesh path.
    /// </summary>
    constexpr const char* ForwardSolidColorShaderSource = R"(cbuffer HelenginePerDraw : register(b0)
{
    float4x4 HelengineWorldViewProjection;
    float4 HelengineBaseColor;
};

struct VS_IN
{
    float3 pos : POSITION;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
};

PS_IN VS(VS_IN input)
{
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), HelengineWorldViewProjection);
    return output;
}

float4 PS(PS_IN input) : SV_TARGET
{
    return HelengineBaseColor;
}
)";

    /// <summary>
    /// Opens the embedded shared solid-color shader source for the PS Vita runtime shader compiler.
    /// </summary>
    SceShaccCgSourceFile* OpenForwardSolidColorShaderSourceFile(
        const char* fileName,
        const SceShaccCgSourceLocation* includedFrom,
        const SceShaccCgCompileOptions* compileOptions,
        const char** errorString) {
        (void)includedFrom;
        (void)compileOptions;

        static SceShaccCgSourceFile sourceFile = {
            ForwardSolidColorShaderFileName,
            ForwardSolidColorShaderSource,
            static_cast<SceUInt32>(std::strlen(ForwardSolidColorShaderSource))
        };

        if (fileName != nullptr && std::strcmp(fileName, ForwardSolidColorShaderFileName) == 0) {
            return &sourceFile;
        }

        if (errorString != nullptr) {
            *errorString = "PS Vita solid-color runtime shader compiler could not resolve the embedded shared shader source.";
        }

        return nullptr;
    }

    /// <summary>
    /// Releases one embedded shared solid-color shader source-file handle after the PS Vita runtime shader compiler finishes with it.
    /// </summary>
    void ReleaseForwardSolidColorShaderSourceFile(
        const SceShaccCgSourceFile* file,
        const SceShaccCgCompileOptions* compileOptions) {
        (void)file;
        (void)compileOptions;
    }

    /// <summary>
    /// Allocates one persistent block of host memory aligned for a runtime-compiled GXM program header copy.
    /// </summary>
    void* AllocatePersistentProgramMemory(std::size_t sizeBytes) {
        return memalign(ProgramHeaderAlignment, sizeBytes);
    }

    /// <summary>
    /// Allocates one GPU-visible block of host memory aligned for GXM runtime mappings.
    /// </summary>
    void* AllocateMappedGxmMemory(std::size_t sizeBytes) {
        return memalign(GxmMappedMemoryAlignment, sizeBytes);
    }

    /// <summary>
    /// Allocates one host-memory block for the GXM shader patcher bookkeeping heap.
    /// </summary>
    void* AllocateShaderPatcherHostMemory(void* userData, unsigned int sizeBytes) {
        (void)userData;
        return std::malloc(sizeBytes);
    }

    /// <summary>
    /// Frees one host-memory block previously allocated for the GXM shader patcher bookkeeping heap.
    /// </summary>
    void FreeShaderPatcherHostMemory(void* userData, void* memory) {
        (void)userData;
        std::free(memory);
    }

    /// <summary>
    /// Loads the legal runtime PS Vita shader-compiler module before the first shader compilation request.
    /// </summary>
    bool TryLoadShaderCompilerModule(SceUID* shaderCompilerModuleId) {
        if (shaderCompilerModuleId == nullptr) {
            return false;
        }

        if (*shaderCompilerModuleId >= 0) {
            return true;
        }

        SceUID moduleId = sceKernelLoadStartModule(ShaderCompilerModulePath, 0u, nullptr, 0, nullptr, nullptr);
        if (moduleId < 0) {
            return false;
        }

        *shaderCompilerModuleId = moduleId;
        return true;
    }

    /// <summary>
    /// Copies one runtime shader-compiler output program into persistent memory so the GXM patcher can keep using it after compilation output cleanup.
    /// </summary>
    bool TryCopyCompileOutputProgram(
        const SceShaccCgCompileOutput* compileOutput,
        void** programData,
        std::size_t* programSizeBytes) {
        if (compileOutput == nullptr
            || compileOutput->programData == nullptr
            || compileOutput->programSize == 0u
            || programData == nullptr
            || programSizeBytes == nullptr) {
            return false;
        }

        void* copiedProgramData = AllocatePersistentProgramMemory(static_cast<std::size_t>(compileOutput->programSize));
        if (copiedProgramData == nullptr) {
            return false;
        }

        std::memcpy(copiedProgramData, compileOutput->programData, static_cast<std::size_t>(compileOutput->programSize));
        *programData = copiedProgramData;
        *programSizeBytes = static_cast<std::size_t>(compileOutput->programSize);
        return true;
    }

    /// <summary>
    /// Formats the first runtime shader-compiler diagnostic into one short failure message when compilation does not succeed.
    /// </summary>
    std::string BuildCompileFailureMessage(const SceShaccCgCompileOutput* compileOutput, const char* entryPoint) {
        std::string message = "PS Vita runtime shader compilation failed";
        if (entryPoint != nullptr) {
            message += " for entry point ";
            message += entryPoint;
        }

        if (compileOutput != nullptr && compileOutput->diagnosticCount > 0 && compileOutput->diagnostics != nullptr) {
            const SceShaccCgDiagnosticMessage& diagnostic = compileOutput->diagnostics[0];
            if (diagnostic.message != nullptr) {
                message += ": ";
                message += diagnostic.message;
            }
        }

        return message;
    }

    /// <summary>
    /// Compiles one embedded shared solid-color shader stage through libshacccg and copies the resulting GXM program header into persistent memory.
    /// </summary>
    bool TryCompileSolidColorShaderStage(
        SceShaccCgTargetProfile profile,
        const char* entryPoint,
        void** programData,
        std::size_t* programSizeBytes,
        std::string& failureMessage) {
        if (programData == nullptr || programSizeBytes == nullptr || entryPoint == nullptr) {
            return false;
        }

        SceShaccCgCompileOptions compileOptions;
        if (sceShaccCgInitializeCompileOptions(&compileOptions) < 0) {
            failureMessage = "PS Vita runtime shader compiler could not initialize compile options.";
            return false;
        }

        compileOptions.mainSourceFile = ForwardSolidColorShaderFileName;
        compileOptions.targetProfile = profile;
        compileOptions.entryFunctionName = entryPoint;
        compileOptions.warningLevel = 4;
        compileOptions.optimizationLevel = 3;

        SceShaccCgCallbackList callbacks;
        std::memset(&callbacks, 0, sizeof(callbacks));
        callbacks.openFile = &OpenForwardSolidColorShaderSourceFile;
        callbacks.releaseFile = &ReleaseForwardSolidColorShaderSourceFile;

        const SceShaccCgCompileOutput* compileOutput = sceShaccCgCompileProgram(&compileOptions, &callbacks, 0);
        if (compileOutput == nullptr) {
            failureMessage = "PS Vita runtime shader compiler returned no output.";
            return false;
        }

        bool copiedProgram = TryCopyCompileOutputProgram(compileOutput, programData, programSizeBytes);
        if (!copiedProgram) {
            failureMessage = BuildCompileFailureMessage(compileOutput, entryPoint);
            sceShaccCgDestroyCompileOutput(compileOutput);
            return false;
        }

        sceShaccCgDestroyCompileOutput(compileOutput);
        return true;
    }

    /// <summary>
    /// Maps one GPU-visible memory block for generic shader patcher backing storage.
    /// </summary>
    bool TryCreateMappedShaderPatcherBuffer(
        std::size_t sizeBytes,
        void** mappedMemory,
        SceGxmMemoryAttribFlags attributes) {
        if (mappedMemory == nullptr) {
            return false;
        }

        void* allocation = AllocateMappedGxmMemory(sizeBytes);
        if (allocation == nullptr) {
            return false;
        }

        if (sceGxmMapMemory(allocation, sizeBytes, attributes) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }

    /// <summary>
    /// Maps one GPU-visible memory block for shader patcher vertex USSE storage.
    /// </summary>
    bool TryCreateMappedShaderPatcherVertexUsseBuffer(
        std::size_t sizeBytes,
        void** mappedMemory,
        unsigned int* mappedOffset) {
        if (mappedMemory == nullptr || mappedOffset == nullptr) {
            return false;
        }

        void* allocation = AllocateMappedGxmMemory(sizeBytes);
        if (allocation == nullptr) {
            return false;
        }

        if (sceGxmMapVertexUsseMemory(allocation, sizeBytes, mappedOffset) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }

    /// <summary>
    /// Maps one GPU-visible memory block for shader patcher fragment USSE storage.
    /// </summary>
    bool TryCreateMappedShaderPatcherFragmentUsseBuffer(
        std::size_t sizeBytes,
        void** mappedMemory,
        unsigned int* mappedOffset) {
        if (mappedMemory == nullptr || mappedOffset == nullptr) {
            return false;
        }

        void* allocation = AllocateMappedGxmMemory(sizeBytes);
        if (allocation == nullptr) {
            return false;
        }

        if (sceGxmMapFragmentUsseMemory(allocation, sizeBytes, mappedOffset) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }
}

namespace helengine::psvita::rendering {
    /// <summary>
    /// Creates one empty runtime-program wrapper that will compile and patch its shader state during renderer initialization.
    /// </summary>
    PsVitaGxmSolidColorProgram::PsVitaGxmSolidColorProgram()
        : Context(nullptr)
        , ShaderPatcher(nullptr)
        , VertexProgramId(nullptr)
        , FragmentProgramId(nullptr)
        , VertexProgram(nullptr)
        , FragmentProgram(nullptr)
        , WorldViewProjectionParameter(nullptr)
        , BaseColorParameter(nullptr)
        , VertexProgramData(nullptr)
        , VertexProgramDataSize(0u)
        , FragmentProgramData(nullptr)
        , FragmentProgramDataSize(0u)
        , PatcherBufferMemory(nullptr)
        , PatcherBufferMemorySize(static_cast<std::size_t>(SolidColorShaderPatcherBufferSize))
        , PatcherVertexUsseMemory(nullptr)
        , PatcherVertexUsseMemorySize(static_cast<std::size_t>(SolidColorShaderPatcherVertexUsseSize))
        , PatcherVertexUsseOffset(0u)
        , PatcherFragmentUsseMemory(nullptr)
        , PatcherFragmentUsseMemorySize(static_cast<std::size_t>(SolidColorShaderPatcherFragmentUsseSize))
        , PatcherFragmentUsseOffset(0u)
        , ShaderCompilerModuleId(-1) {
    }

    /// <summary>
    /// Compiles, patches, and binds the PS Vita solid-color shader programs needed by runtime mesh draws.
    /// </summary>
    bool PsVitaGxmSolidColorProgram::Initialize() {
        Reset();

        if (_vita2d_context == nullptr) {
            return false;
        }

        if (!TryLoadShaderCompilerModule(&ShaderCompilerModuleId)) {
            return false;
        }

        std::string compileFailureMessage;
        if (!TryCompileSolidColorShaderStage(
            SCE_SHACCCG_PROFILE_VP,
            ForwardSolidColorVertexEntryPoint,
            &VertexProgramData,
            &VertexProgramDataSize,
            compileFailureMessage)) {
            Reset();
            return false;
        }

        if (!TryCompileSolidColorShaderStage(
            SCE_SHACCCG_PROFILE_FP,
            ForwardSolidColorFragmentEntryPoint,
            &FragmentProgramData,
            &FragmentProgramDataSize,
            compileFailureMessage)) {
            Reset();
            return false;
        }

        if (!TryCreateMappedShaderPatcherBuffer(PatcherBufferMemorySize, &PatcherBufferMemory, SCE_GXM_MEMORY_ATTRIB_RW)) {
            Reset();
            return false;
        }

        if (!TryCreateMappedShaderPatcherVertexUsseBuffer(
            PatcherVertexUsseMemorySize,
            &PatcherVertexUsseMemory,
            &PatcherVertexUsseOffset)) {
            Reset();
            return false;
        }

        if (!TryCreateMappedShaderPatcherFragmentUsseBuffer(
            PatcherFragmentUsseMemorySize,
            &PatcherFragmentUsseMemory,
            &PatcherFragmentUsseOffset)) {
            Reset();
            return false;
        }

        SceGxmShaderPatcherParams shaderPatcherParams;
        std::memset(&shaderPatcherParams, 0, sizeof(shaderPatcherParams));
        shaderPatcherParams.userData = nullptr;
        shaderPatcherParams.hostAllocCallback = &AllocateShaderPatcherHostMemory;
        shaderPatcherParams.hostFreeCallback = &FreeShaderPatcherHostMemory;
        shaderPatcherParams.bufferMem = PatcherBufferMemory;
        shaderPatcherParams.bufferMemSize = PatcherBufferMemorySize;
        shaderPatcherParams.vertexUsseMem = PatcherVertexUsseMemory;
        shaderPatcherParams.vertexUsseMemSize = PatcherVertexUsseMemorySize;
        shaderPatcherParams.vertexUsseOffset = PatcherVertexUsseOffset;
        shaderPatcherParams.fragmentUsseMem = PatcherFragmentUsseMemory;
        shaderPatcherParams.fragmentUsseMemSize = PatcherFragmentUsseMemorySize;
        shaderPatcherParams.fragmentUsseOffset = PatcherFragmentUsseOffset;

        if (sceGxmShaderPatcherCreate(&shaderPatcherParams, &ShaderPatcher) < 0 || ShaderPatcher == nullptr) {
            Reset();
            return false;
        }

        const SceGxmProgram* vertexProgramGxp = static_cast<const SceGxmProgram*>(VertexProgramData);
        const SceGxmProgram* fragmentProgramGxp = static_cast<const SceGxmProgram*>(FragmentProgramData);
        if (vertexProgramGxp == nullptr || fragmentProgramGxp == nullptr) {
            Reset();
            return false;
        }

        if (sceGxmShaderPatcherRegisterProgram(ShaderPatcher, vertexProgramGxp, &VertexProgramId) < 0
            || VertexProgramId == nullptr
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, fragmentProgramGxp, &FragmentProgramId) < 0
            || FragmentProgramId == nullptr) {
            Reset();
            return false;
        }

        const SceGxmProgramParameter* positionParameter = sceGxmProgramFindParameterByName(vertexProgramGxp, ForwardSolidColorPositionParameterName);
        WorldViewProjectionParameter = sceGxmProgramFindParameterByName(vertexProgramGxp, ForwardSolidColorWorldViewProjectionParameterName);
        BaseColorParameter = sceGxmProgramFindParameterByName(fragmentProgramGxp, ForwardSolidColorBaseColorParameterName);
        if (positionParameter == nullptr || WorldViewProjectionParameter == nullptr || BaseColorParameter == nullptr) {
            Reset();
            return false;
        }

        SceGxmVertexAttribute vertexAttributes[1];
        std::memset(vertexAttributes, 0, sizeof(vertexAttributes));
        vertexAttributes[0].streamIndex = 0u;
        vertexAttributes[0].offset = 0u;
        vertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        vertexAttributes[0].componentCount = 3u;
        vertexAttributes[0].regIndex = static_cast<std::uint16_t>(sceGxmProgramParameterGetResourceIndex(positionParameter));

        SceGxmVertexStream vertexStreams[1];
        std::memset(vertexStreams, 0, sizeof(vertexStreams));
        vertexStreams[0].stride = static_cast<std::uint16_t>(sizeof(float) * 3u);
        vertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_32BIT;

        if (sceGxmShaderPatcherCreateVertexProgram(
            ShaderPatcher,
            VertexProgramId,
            vertexAttributes,
            1u,
            vertexStreams,
            1u,
            &VertexProgram) < 0
            || VertexProgram == nullptr) {
            Reset();
            return false;
        }

        if (sceGxmShaderPatcherCreateFragmentProgram(
            ShaderPatcher,
            FragmentProgramId,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
            SCE_GXM_MULTISAMPLE_NONE,
            nullptr,
            vertexProgramGxp,
            &FragmentProgram) < 0
            || FragmentProgram == nullptr) {
            Reset();
            return false;
        }

        Context = _vita2d_context;
        return IsReady();
    }

    /// <summary>
    /// Releases the compiled shader programs, patcher allocations, and resolved uniform bindings owned by this runtime-program wrapper.
    /// </summary>
    void PsVitaGxmSolidColorProgram::Reset() {
        if (ShaderPatcher != nullptr && VertexProgram != nullptr) {
            sceGxmShaderPatcherReleaseVertexProgram(ShaderPatcher, VertexProgram);
        }

        if (ShaderPatcher != nullptr && FragmentProgram != nullptr) {
            sceGxmShaderPatcherReleaseFragmentProgram(ShaderPatcher, FragmentProgram);
        }

        if (ShaderPatcher != nullptr && VertexProgramId != nullptr) {
            sceGxmShaderPatcherUnregisterProgram(ShaderPatcher, VertexProgramId);
        }

        if (ShaderPatcher != nullptr && FragmentProgramId != nullptr) {
            sceGxmShaderPatcherUnregisterProgram(ShaderPatcher, FragmentProgramId);
        }

        if (ShaderPatcher != nullptr) {
            sceGxmShaderPatcherDestroy(ShaderPatcher);
        }

        if (PatcherBufferMemory != nullptr) {
            sceGxmUnmapMemory(PatcherBufferMemory);
            std::free(PatcherBufferMemory);
        }

        if (PatcherVertexUsseMemory != nullptr) {
            sceGxmUnmapVertexUsseMemory(PatcherVertexUsseMemory);
            std::free(PatcherVertexUsseMemory);
        }

        if (PatcherFragmentUsseMemory != nullptr) {
            sceGxmUnmapFragmentUsseMemory(PatcherFragmentUsseMemory);
            std::free(PatcherFragmentUsseMemory);
        }

        if (VertexProgramData != nullptr) {
            std::free(VertexProgramData);
        }

        if (FragmentProgramData != nullptr) {
            std::free(FragmentProgramData);
        }

        Context = nullptr;
        ShaderPatcher = nullptr;
        VertexProgramId = nullptr;
        FragmentProgramId = nullptr;
        VertexProgram = nullptr;
        FragmentProgram = nullptr;
        WorldViewProjectionParameter = nullptr;
        BaseColorParameter = nullptr;
        VertexProgramData = nullptr;
        VertexProgramDataSize = 0u;
        FragmentProgramData = nullptr;
        FragmentProgramDataSize = 0u;
        PatcherBufferMemory = nullptr;
        PatcherVertexUsseMemory = nullptr;
        PatcherVertexUsseOffset = 0u;
        PatcherFragmentUsseMemory = nullptr;
        PatcherFragmentUsseOffset = 0u;
    }

    /// <summary>
    /// Gets whether the runtime-compiled GXM context, shader programs, and uniform bindings are all available.
    /// </summary>
    bool PsVitaGxmSolidColorProgram::IsReady() const {
        return Context != nullptr
            && ShaderPatcher != nullptr
            && VertexProgram != nullptr
            && FragmentProgram != nullptr
            && WorldViewProjectionParameter != nullptr
            && BaseColorParameter != nullptr;
    }

    /// <summary>
    /// Gets the active GXM context used for direct programmable draw calls.
    /// </summary>
    SceGxmContext* PsVitaGxmSolidColorProgram::GetContext() const {
        return Context;
    }

    /// <summary>
    /// Gets the runtime-compiled solid-color vertex program.
    /// </summary>
    SceGxmVertexProgram* PsVitaGxmSolidColorProgram::GetVertexProgram() const {
        return VertexProgram;
    }

    /// <summary>
    /// Gets the runtime-compiled solid-color fragment program.
    /// </summary>
    SceGxmFragmentProgram* PsVitaGxmSolidColorProgram::GetFragmentProgram() const {
        return FragmentProgram;
    }

    /// <summary>
    /// Gets the world-view-projection uniform parameter used by the runtime-compiled solid-color vertex program.
    /// </summary>
    const SceGxmProgramParameter* PsVitaGxmSolidColorProgram::GetWorldViewProjectionParameter() const {
        return WorldViewProjectionParameter;
    }

    /// <summary>
    /// Gets the base-color uniform parameter used by the runtime-compiled solid-color fragment program.
    /// </summary>
    const SceGxmProgramParameter* PsVitaGxmSolidColorProgram::GetBaseColorParameter() const {
        return BaseColorParameter;
    }
}

#endif
