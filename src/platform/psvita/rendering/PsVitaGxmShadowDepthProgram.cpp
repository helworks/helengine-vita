#include "platform/psvita/rendering/PsVitaGxmShadowDepthProgram.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <vita2d.h>

#include "platform/psvita/shaders/PsVitaShaderArtifactReader.hpp"

namespace {
    /// Stores the generic patcher backing memory size for the compact depth-only program.
    constexpr std::size_t ShadowDepthPatcherBufferSize = 96u * 1024u;

    /// Stores the vertex-USSE memory size for the compact depth-only program.
    constexpr std::size_t ShadowDepthVertexUsseSize = 16u * 1024u;

    /// Stores the fragment-USSE memory size for the compact depth-only program.
    constexpr std::size_t ShadowDepthFragmentUsseSize = 16u * 1024u;

    /// Allocates host memory owned by the GXM shader patcher.
    void* AllocateHostMemory(void* userData, unsigned int sizeBytes) {
        (void)userData;
        return std::malloc(sizeBytes);
    }

    /// Releases host memory owned by the GXM shader patcher.
    void FreeHostMemory(void* userData, void* memory) {
        (void)userData;
        std::free(memory);
    }

    /// Maps one generic GXM allocation required by the shader patcher.
    bool MapBuffer(std::size_t sizeBytes, void** memory) {
        if (memory == nullptr) {
            return false;
        }

        void* allocation = memalign(4096u, sizeBytes);
        if (allocation == nullptr || sceGxmMapMemory(allocation, sizeBytes, SCE_GXM_MEMORY_ATTRIB_RW) < 0) {
            std::free(allocation);
            return false;
        }

        *memory = allocation;
        return true;
    }

    /// Maps one vertex-USSE allocation required by the shader patcher.
    bool MapVertexUsse(std::size_t sizeBytes, void** memory, unsigned int* offset) {
        if (memory == nullptr || offset == nullptr) {
            return false;
        }

        void* allocation = memalign(4096u, sizeBytes);
        if (allocation == nullptr || sceGxmMapVertexUsseMemory(allocation, sizeBytes, offset) < 0) {
            std::free(allocation);
            return false;
        }

        *memory = allocation;
        return true;
    }

    /// Maps one fragment-USSE allocation required by the shader patcher.
    bool MapFragmentUsse(std::size_t sizeBytes, void** memory, unsigned int* offset) {
        if (memory == nullptr || offset == nullptr) {
            return false;
        }

        void* allocation = memalign(4096u, sizeBytes);
        if (allocation == nullptr || sceGxmMapFragmentUsseMemory(allocation, sizeBytes, offset) < 0) {
            std::free(allocation);
            return false;
        }

        *memory = allocation;
        return true;
    }
}

namespace helengine::psvita::rendering {
    /// Creates one uninitialized depth-only GXM program.
    PsVitaGxmShadowDepthProgram::PsVitaGxmShadowDepthProgram()
        : Context(nullptr)
        , ShaderPatcher(nullptr)
        , VertexProgramId(nullptr)
        , FragmentProgramId(nullptr)
        , VertexProgram(nullptr)
        , FragmentProgram(nullptr)
        , VertexProgramData(nullptr)
        , FragmentProgramData(nullptr)
        , PatcherBufferMemory(nullptr)
        , PatcherVertexUsseMemory(nullptr)
        , PatcherFragmentUsseMemory(nullptr)
        , PatcherVertexUsseOffset(0u)
        , PatcherFragmentUsseOffset(0u)
        , LightViewProjectionParameter(nullptr) {
    }

    /// Loads, patches, and reflects one compiled ShadowDepth artifact pair.
    bool PsVitaGxmShadowDepthProgram::Initialize(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes) {
        if (IsReady()) {
            return true;
        }

        Reset();
        Context = vita2d_get_context();
        if (Context == nullptr) {
            Reset();
            return false;
        }

        std::size_t vertexProgramSize = 0u;
        std::size_t fragmentProgramSize = 0u;
        if (!shaders::PsVitaShaderArtifactReader::TryReadBytes(vertexArtifactBytes, "VP", &VertexProgramData, &vertexProgramSize)
            || !shaders::PsVitaShaderArtifactReader::TryReadBytes(fragmentArtifactBytes, "FP", &FragmentProgramData, &fragmentProgramSize)) {
            Reset();
            return false;
        }

        const SceGxmProgram* vertexProgram = static_cast<const SceGxmProgram*>(VertexProgramData);
        const SceGxmProgram* fragmentProgram = static_cast<const SceGxmProgram*>(FragmentProgramData);
        if (sceGxmProgramCheck(vertexProgram) < 0 || sceGxmProgramCheck(fragmentProgram) < 0
            || !MapBuffer(ShadowDepthPatcherBufferSize, &PatcherBufferMemory)
            || !MapVertexUsse(ShadowDepthVertexUsseSize, &PatcherVertexUsseMemory, &PatcherVertexUsseOffset)
            || !MapFragmentUsse(ShadowDepthFragmentUsseSize, &PatcherFragmentUsseMemory, &PatcherFragmentUsseOffset)) {
            Reset();
            return false;
        }

        SceGxmShaderPatcherParams parameters;
        std::memset(&parameters, 0, sizeof(parameters));
        parameters.hostAllocCallback = &AllocateHostMemory;
        parameters.hostFreeCallback = &FreeHostMemory;
        parameters.bufferMem = PatcherBufferMemory;
        parameters.bufferMemSize = ShadowDepthPatcherBufferSize;
        parameters.vertexUsseMem = PatcherVertexUsseMemory;
        parameters.vertexUsseMemSize = ShadowDepthVertexUsseSize;
        parameters.vertexUsseOffset = PatcherVertexUsseOffset;
        parameters.fragmentUsseMem = PatcherFragmentUsseMemory;
        parameters.fragmentUsseMemSize = ShadowDepthFragmentUsseSize;
        parameters.fragmentUsseOffset = PatcherFragmentUsseOffset;
        if (sceGxmShaderPatcherCreate(&parameters, &ShaderPatcher) < 0
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, vertexProgram, &VertexProgramId) < 0
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, fragmentProgram, &FragmentProgramId) < 0) {
            Reset();
            return false;
        }

        LightViewProjectionParameter = sceGxmProgramFindParameterByName(vertexProgram, "HelengineLightViewProjection");
        const SceGxmProgramParameter* positionParameter = sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_POSITION, 0u);
        if (LightViewProjectionParameter == nullptr || positionParameter == nullptr) {
            Reset();
            return false;
        }

        SceGxmVertexAttribute attribute;
        std::memset(&attribute, 0, sizeof(attribute));
        attribute.streamIndex = 0u;
        attribute.format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attribute.componentCount = 3u;
        attribute.regIndex = static_cast<std::uint16_t>(sceGxmProgramParameterGetResourceIndex(positionParameter));

        SceGxmVertexStream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.stride = static_cast<std::uint16_t>(sizeof(float) * 3u);
        stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_32BIT;
        if (sceGxmShaderPatcherCreateVertexProgram(ShaderPatcher, VertexProgramId, &attribute, 1u, &stream, 1u, &VertexProgram) < 0
            || sceGxmShaderPatcherCreateFragmentProgram(ShaderPatcher, FragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, nullptr, vertexProgram, &FragmentProgram) < 0) {
            Reset();
            return false;
        }

        return IsReady();
    }

    /// Releases all patched programs, artifact allocations, and mapped patcher memory.
    void PsVitaGxmShadowDepthProgram::Reset() {
        if (ShaderPatcher != nullptr && VertexProgram != nullptr) sceGxmShaderPatcherReleaseVertexProgram(ShaderPatcher, VertexProgram);
        if (ShaderPatcher != nullptr && FragmentProgram != nullptr) sceGxmShaderPatcherReleaseFragmentProgram(ShaderPatcher, FragmentProgram);
        if (ShaderPatcher != nullptr && VertexProgramId != nullptr) sceGxmShaderPatcherUnregisterProgram(ShaderPatcher, VertexProgramId);
        if (ShaderPatcher != nullptr && FragmentProgramId != nullptr) sceGxmShaderPatcherUnregisterProgram(ShaderPatcher, FragmentProgramId);
        if (ShaderPatcher != nullptr) sceGxmShaderPatcherDestroy(ShaderPatcher);
        if (PatcherBufferMemory != nullptr) { sceGxmUnmapMemory(PatcherBufferMemory); std::free(PatcherBufferMemory); }
        if (PatcherVertexUsseMemory != nullptr) { sceGxmUnmapVertexUsseMemory(PatcherVertexUsseMemory); std::free(PatcherVertexUsseMemory); }
        if (PatcherFragmentUsseMemory != nullptr) { sceGxmUnmapFragmentUsseMemory(PatcherFragmentUsseMemory); std::free(PatcherFragmentUsseMemory); }
        shaders::PsVitaShaderArtifactReader::Release(VertexProgramData);
        shaders::PsVitaShaderArtifactReader::Release(FragmentProgramData);
        Context = nullptr;
        ShaderPatcher = nullptr;
        VertexProgramId = nullptr;
        FragmentProgramId = nullptr;
        VertexProgram = nullptr;
        FragmentProgram = nullptr;
        VertexProgramData = nullptr;
        FragmentProgramData = nullptr;
        PatcherBufferMemory = nullptr;
        PatcherVertexUsseMemory = nullptr;
        PatcherFragmentUsseMemory = nullptr;
        PatcherVertexUsseOffset = 0u;
        PatcherFragmentUsseOffset = 0u;
        LightViewProjectionParameter = nullptr;
    }

    /// Gets whether the complete depth-only program contract is ready for drawing.
    bool PsVitaGxmShadowDepthProgram::IsReady() const {
        return Context != nullptr && ShaderPatcher != nullptr && VertexProgram != nullptr && FragmentProgram != nullptr && LightViewProjectionParameter != nullptr;
    }

    /// Gets Vita2D's active GXM context used by the open offscreen pass.
    SceGxmContext* PsVitaGxmShadowDepthProgram::GetContext() const { return Context; }

    /// Gets the patched vertex program.
    SceGxmVertexProgram* PsVitaGxmShadowDepthProgram::GetVertexProgram() const { return VertexProgram; }

    /// Gets the patched fragment program.
    SceGxmFragmentProgram* PsVitaGxmShadowDepthProgram::GetFragmentProgram() const { return FragmentProgram; }

    /// Gets the light-space transform uniform reflected from the vertex program.
    const SceGxmProgramParameter* PsVitaGxmShadowDepthProgram::GetLightViewProjectionParameter() const { return LightViewProjectionParameter; }

}

#endif
