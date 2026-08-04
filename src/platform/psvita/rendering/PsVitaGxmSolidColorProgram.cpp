#include "platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <psp2/gxm.h>
#include <vita2d.h>

#include "platform/psvita/shaders/PsVitaShaderArtifactReader.hpp"

namespace {
    /// Stores the staged vertex artifact used by the solid-color mesh program.
    constexpr const char* SolidColorVertexArtifactPath = "app0:/cooked/shaders/ForwardSolidColorShader.vp.pvsa";

    /// Stores the staged fragment artifact used by the solid-color mesh program.
    constexpr const char* SolidColorFragmentArtifactPath = "app0:/cooked/shaders/ForwardSolidColorShader.fp.pvsa";

    /// Stores the position attribute name reflected by the solid-color vertex program.
    constexpr const char* SolidColorPositionParameterName = "pos";

    /// Stores the world-view-projection uniform name reflected by the solid-color vertex program.
    constexpr const char* SolidColorWorldViewProjectionParameterName = "HelengineWorldViewProjection";

    /// Stores the base-color uniform name reflected by the solid-color fragment program.
    constexpr const char* SolidColorBaseColorParameterName = "HelengineBaseColor";

    /// Stores the shader patcher backing-buffer size reserved for one solid-color shader pair.
    constexpr std::size_t SolidColorShaderPatcherBufferSize = 64u * 1024u;

    /// Stores the vertex USSE buffer size reserved for one solid-color shader pair.
    constexpr std::size_t SolidColorShaderPatcherVertexUsseSize = 16u * 1024u;

    /// Stores the fragment USSE buffer size reserved for one solid-color shader pair.
    constexpr std::size_t SolidColorShaderPatcherFragmentUsseSize = 16u * 1024u;

    /// Stores the alignment used for GPU-visible GXM allocations.
    constexpr unsigned int GxmMappedMemoryAlignment = 4096u;

    /// Allocates host memory owned by the GXM shader patcher.
    void* AllocateShaderPatcherHostMemory(void* userData, unsigned int sizeBytes) {
        (void)userData;
        return std::malloc(sizeBytes);
    }

    /// Releases host memory owned by the GXM shader patcher.
    void FreeShaderPatcherHostMemory(void* userData, void* memory) {
        (void)userData;
        std::free(memory);
    }

    /// Maps one generic GXM patcher buffer.
    bool TryCreateMappedBuffer(std::size_t sizeBytes, void** mappedMemory, SceGxmMemoryAttribFlags attributes) {
        if (mappedMemory == nullptr) {
            return false;
        }

        void* allocation = memalign(GxmMappedMemoryAlignment, sizeBytes);
        if (allocation == nullptr || sceGxmMapMemory(allocation, sizeBytes, attributes) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }

    /// Maps one vertex USSE patcher buffer.
    bool TryCreateMappedVertexUsseBuffer(std::size_t sizeBytes, void** mappedMemory, unsigned int* mappedOffset) {
        if (mappedMemory == nullptr || mappedOffset == nullptr) {
            return false;
        }

        void* allocation = memalign(GxmMappedMemoryAlignment, sizeBytes);
        if (allocation == nullptr || sceGxmMapVertexUsseMemory(allocation, sizeBytes, mappedOffset) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }

    /// Maps one fragment USSE patcher buffer.
    bool TryCreateMappedFragmentUsseBuffer(std::size_t sizeBytes, void** mappedMemory, unsigned int* mappedOffset) {
        if (mappedMemory == nullptr || mappedOffset == nullptr) {
            return false;
        }

        void* allocation = memalign(GxmMappedMemoryAlignment, sizeBytes);
        if (allocation == nullptr || sceGxmMapFragmentUsseMemory(allocation, sizeBytes, mappedOffset) < 0) {
            std::free(allocation);
            return false;
        }

        *mappedMemory = allocation;
        return true;
    }
}

namespace helengine::psvita::rendering {
    /// Creates one empty artifact-backed solid-color program wrapper.
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
        , PatcherBufferMemorySize(SolidColorShaderPatcherBufferSize)
        , PatcherVertexUsseMemory(nullptr)
        , PatcherVertexUsseMemorySize(SolidColorShaderPatcherVertexUsseSize)
        , PatcherVertexUsseOffset(0u)
        , PatcherFragmentUsseMemory(nullptr)
        , PatcherFragmentUsseMemorySize(SolidColorShaderPatcherFragmentUsseSize)
        , PatcherFragmentUsseOffset(0u)
        , InitializationFailed(false) {
    }

    /// Loads, patches, and binds the staged solid-color shader programs needed by runtime mesh draws.
    bool PsVitaGxmSolidColorProgram::Initialize() {
        if (IsReady()) {
            return true;
        }

        SceGxmContext* activeContext = vita2d_get_context();
        if (InitializationFailed || activeContext == nullptr) {
            InitializationFailed = true;
            return false;
        }

        Reset();
        if (!shaders::PsVitaShaderArtifactReader::TryRead(SolidColorVertexArtifactPath, "VP", &VertexProgramData, &VertexProgramDataSize)
            || !shaders::PsVitaShaderArtifactReader::TryRead(SolidColorFragmentArtifactPath, "FP", &FragmentProgramData, &FragmentProgramDataSize)) {
            Reset();
            InitializationFailed = true;
            return false;
        }

        if (!TryCreateMappedBuffer(PatcherBufferMemorySize, &PatcherBufferMemory, SCE_GXM_MEMORY_ATTRIB_RW)
            || !TryCreateMappedVertexUsseBuffer(PatcherVertexUsseMemorySize, &PatcherVertexUsseMemory, &PatcherVertexUsseOffset)
            || !TryCreateMappedFragmentUsseBuffer(PatcherFragmentUsseMemorySize, &PatcherFragmentUsseMemory, &PatcherFragmentUsseOffset)) {
            Reset();
            InitializationFailed = true;
            return false;
        }

        SceGxmShaderPatcherParams patcherParams;
        std::memset(&patcherParams, 0, sizeof(patcherParams));
        patcherParams.hostAllocCallback = &AllocateShaderPatcherHostMemory;
        patcherParams.hostFreeCallback = &FreeShaderPatcherHostMemory;
        patcherParams.bufferMem = PatcherBufferMemory;
        patcherParams.bufferMemSize = PatcherBufferMemorySize;
        patcherParams.vertexUsseMem = PatcherVertexUsseMemory;
        patcherParams.vertexUsseMemSize = PatcherVertexUsseMemorySize;
        patcherParams.vertexUsseOffset = PatcherVertexUsseOffset;
        patcherParams.fragmentUsseMem = PatcherFragmentUsseMemory;
        patcherParams.fragmentUsseMemSize = PatcherFragmentUsseMemorySize;
        patcherParams.fragmentUsseOffset = PatcherFragmentUsseOffset;
        if (sceGxmShaderPatcherCreate(&patcherParams, &ShaderPatcher) < 0 || ShaderPatcher == nullptr) {
            Reset();
            InitializationFailed = true;
            return false;
        }

        const SceGxmProgram* vertexProgramGxp = static_cast<const SceGxmProgram*>(VertexProgramData);
        const SceGxmProgram* fragmentProgramGxp = static_cast<const SceGxmProgram*>(FragmentProgramData);
        if (sceGxmShaderPatcherRegisterProgram(ShaderPatcher, vertexProgramGxp, &VertexProgramId) < 0
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, fragmentProgramGxp, &FragmentProgramId) < 0) {
            Reset();
            InitializationFailed = true;
            return false;
        }

        const SceGxmProgramParameter* positionParameter = sceGxmProgramFindParameterByName(vertexProgramGxp, SolidColorPositionParameterName);
        WorldViewProjectionParameter = sceGxmProgramFindParameterByName(vertexProgramGxp, SolidColorWorldViewProjectionParameterName);
        BaseColorParameter = sceGxmProgramFindParameterByName(fragmentProgramGxp, SolidColorBaseColorParameterName);
        if (positionParameter == nullptr || WorldViewProjectionParameter == nullptr || BaseColorParameter == nullptr) {
            Reset();
            InitializationFailed = true;
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
        if (sceGxmShaderPatcherCreateVertexProgram(ShaderPatcher, VertexProgramId, vertexAttributes, 1u, vertexStreams, 1u, &VertexProgram) < 0
            || sceGxmShaderPatcherCreateFragmentProgram(ShaderPatcher, FragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, nullptr, vertexProgramGxp, &FragmentProgram) < 0) {
            Reset();
            InitializationFailed = true;
            return false;
        }

        Context = activeContext;
        InitializationFailed = false;
        return IsReady();
    }

    /// Releases loaded programs, patcher allocations, and uniform bindings.
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
        shaders::PsVitaShaderArtifactReader::Release(VertexProgramData);
        shaders::PsVitaShaderArtifactReader::Release(FragmentProgramData);
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

    /// Gets whether the artifact-backed GXM program is ready for drawing.
    bool PsVitaGxmSolidColorProgram::IsReady() const {
        return Context != nullptr && ShaderPatcher != nullptr && VertexProgram != nullptr && FragmentProgram != nullptr
            && WorldViewProjectionParameter != nullptr && BaseColorParameter != nullptr;
    }

    /// Gets the active GXM context.
    SceGxmContext* PsVitaGxmSolidColorProgram::GetContext() const { return Context; }

    /// Gets the loaded vertex program.
    SceGxmVertexProgram* PsVitaGxmSolidColorProgram::GetVertexProgram() const { return VertexProgram; }

    /// Gets the loaded fragment program.
    SceGxmFragmentProgram* PsVitaGxmSolidColorProgram::GetFragmentProgram() const { return FragmentProgram; }

    /// Gets the world-view-projection uniform parameter.
    const SceGxmProgramParameter* PsVitaGxmSolidColorProgram::GetWorldViewProjectionParameter() const { return WorldViewProjectionParameter; }

    /// Gets the base-color uniform parameter.
    const SceGxmProgramParameter* PsVitaGxmSolidColorProgram::GetBaseColorParameter() const { return BaseColorParameter; }
}

#endif
