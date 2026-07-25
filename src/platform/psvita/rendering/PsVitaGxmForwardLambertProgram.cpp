#include "platform/psvita/rendering/PsVitaGxmForwardLambertProgram.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <vita2d.h>

#include "platform/psvita/shaders/PsVitaShaderArtifactReader.hpp"

namespace {
    /// Stores the shared runtime trace path used by the PS Vita renderer diagnostics.
    constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";

    /// Controls persisted diagnostics for forward-Lambert program initialization.
    constexpr bool EnablePsVitaBootTraceLogging = false;

    /// Stores the patcher backing memory size for one forward-Lambert program.
    constexpr std::size_t PatcherBufferSize = 128u * 1024u;

    /// Stores the vertex USSE memory size for one forward-Lambert program.
    constexpr std::size_t VertexUsseSize = 32u * 1024u;

    /// Stores the fragment USSE memory size for one forward-Lambert program.
    constexpr std::size_t FragmentUsseSize = 32u * 1024u;

    /// Appends one forward-Lambert initialization diagnostic to the persisted boot trace.
    void AppendForwardLambertProgramTrace(const char* message) {
        if (!EnablePsVitaBootTraceLogging) {
            return;
        }

        std::FILE* file = std::fopen(BootTracePath, "a");
        if (file == nullptr) {
            return;
        }

        std::fputs(message, file);
        std::fputc('\n', file);
        std::fclose(file);
    }

    /// Allocates host memory owned by the GXM patcher.
    void* AllocateHostMemory(void* userData, unsigned int sizeBytes) {
        (void)userData;
        return std::malloc(sizeBytes);
    }

    /// Releases host memory owned by the GXM patcher.
    void FreeHostMemory(void* userData, void* memory) {
        (void)userData;
        std::free(memory);
    }

    /// Maps the generic patcher backing buffer.
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

    /// Maps one vertex USSE buffer.
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

    /// Maps one fragment USSE buffer.
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
    /// Creates one uninitialized forward-Lambert program wrapper.
    PsVitaGxmForwardLambertProgram::PsVitaGxmForwardLambertProgram()
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
        , WorldViewProjectionParameter(nullptr)
        , NormalTransformParameter(nullptr)
        , BaseColorParameter(nullptr)
        , LightDirectionParameter(nullptr)
        , LightColorParameter(nullptr)
        , AmbientParameter(nullptr)
        , DiffuseTextureParameter(nullptr)
        , LightViewProjectionParameter(nullptr)
        , ShadowBiasParameter(nullptr)
        , ShadowTextureParameter(nullptr)
        , Textured(false)
        , Shadowed(false) {
    }

    /// Loads artifacts, creates the patcher state, and resolves the Lambert parameter contract.
    bool PsVitaGxmForwardLambertProgram::Initialize(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes) {
        return InitializeInternal(vertexArtifactBytes, fragmentArtifactBytes, false, false);
    }

    /// Loads artifacts for the textured Forward Standard Shader profile and resolves its diffuse sampler contract.
    bool PsVitaGxmForwardLambertProgram::InitializeTextured(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes) {
        return InitializeInternal(vertexArtifactBytes, fragmentArtifactBytes, true, false);
    }

    /// Loads artifacts for the textured Standard Shader profile that receives one directional shadow map.
    bool PsVitaGxmForwardLambertProgram::InitializeTexturedShadowed(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes) {
        return InitializeInternal(vertexArtifactBytes, fragmentArtifactBytes, true, true);
    }

    /// Initializes either the untextured Lambert or textured Forward Standard artifact contract.
    bool PsVitaGxmForwardLambertProgram::InitializeInternal(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes, bool textured, bool shadowed) {
        if (IsReady()) {
            return Textured == textured && Shadowed == shadowed;
        }

        Reset();
        std::size_t vertexProgramSize = 0u;
        std::size_t fragmentProgramSize = 0u;
        SceGxmContext* activeContext = vita2d_get_context();
        if (activeContext == nullptr) {
            AppendForwardLambertProgramTrace("ForwardLambert: initialization failed because Vita2D did not provide a GXM context.");
            Reset();
            return false;
        }
        if (!shaders::PsVitaShaderArtifactReader::TryReadBytes(vertexArtifactBytes, "VP", &VertexProgramData, &vertexProgramSize)) {
            AppendForwardLambertProgramTrace("ForwardLambert: vertex artifact failed validation or loading.");
            Reset();
            return false;
        }
        if (!shaders::PsVitaShaderArtifactReader::TryReadBytes(fragmentArtifactBytes, "FP", &FragmentProgramData, &fragmentProgramSize)) {
            AppendForwardLambertProgramTrace("ForwardLambert: fragment artifact failed validation or loading.");
            Reset();
            return false;
        }

        if (sceGxmProgramCheck(static_cast<const SceGxmProgram*>(VertexProgramData)) < 0) {
            AppendForwardLambertProgramTrace("ForwardLambert: vertex GXP program check failed.");
            Reset();
            return false;
        }
        if (sceGxmProgramCheck(static_cast<const SceGxmProgram*>(FragmentProgramData)) < 0) {
            AppendForwardLambertProgramTrace("ForwardLambert: fragment GXP program check failed.");
            Reset();
            return false;
        }
        if (!MapBuffer(PatcherBufferSize, &PatcherBufferMemory)
            || !MapVertexUsse(VertexUsseSize, &PatcherVertexUsseMemory, &PatcherVertexUsseOffset)
            || !MapFragmentUsse(FragmentUsseSize, &PatcherFragmentUsseMemory, &PatcherFragmentUsseOffset)) {
            AppendForwardLambertProgramTrace("ForwardLambert: shader patcher memory mapping failed.");
            Reset();
            return false;
        }

        SceGxmShaderPatcherParams params;
        std::memset(&params, 0, sizeof(params));
        params.hostAllocCallback = &AllocateHostMemory;
        params.hostFreeCallback = &FreeHostMemory;
        params.bufferMem = PatcherBufferMemory;
        params.bufferMemSize = PatcherBufferSize;
        params.vertexUsseMem = PatcherVertexUsseMemory;
        params.vertexUsseMemSize = VertexUsseSize;
        params.vertexUsseOffset = PatcherVertexUsseOffset;
        params.fragmentUsseMem = PatcherFragmentUsseMemory;
        params.fragmentUsseMemSize = FragmentUsseSize;
        params.fragmentUsseOffset = PatcherFragmentUsseOffset;
        if (sceGxmShaderPatcherCreate(&params, &ShaderPatcher) < 0
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, static_cast<const SceGxmProgram*>(VertexProgramData), &VertexProgramId) < 0
            || sceGxmShaderPatcherRegisterProgram(ShaderPatcher, static_cast<const SceGxmProgram*>(FragmentProgramData), &FragmentProgramId) < 0) {
            AppendForwardLambertProgramTrace("ForwardLambert: shader patcher creation or GXP registration failed.");
            Reset();
            return false;
        }

        const SceGxmProgram* vertexProgram = static_cast<const SceGxmProgram*>(VertexProgramData);
        const SceGxmProgram* fragmentProgram = static_cast<const SceGxmProgram*>(FragmentProgramData);
        WorldViewProjectionParameter = sceGxmProgramFindParameterByName(vertexProgram, "HelengineWorldViewProjection");
        NormalTransformParameter = sceGxmProgramFindParameterByName(vertexProgram, "HelengineNormalTransform");
        BaseColorParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineBaseColor");
        LightDirectionParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineLightDirection");
        LightColorParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineLightColor");
        AmbientParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineAmbient");
        if (WorldViewProjectionParameter == nullptr || NormalTransformParameter == nullptr || BaseColorParameter == nullptr
            || LightDirectionParameter == nullptr || LightColorParameter == nullptr || AmbientParameter == nullptr) {
            AppendForwardLambertProgramTrace("ForwardLambert: one or more required uniform parameters were not reflected by the GXP programs.");
            Reset();
            return false;
        }

        if (textured) {
            DiffuseTextureParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineDiffuseTexture");
            if (DiffuseTextureParameter == nullptr) {
                AppendForwardLambertProgramTrace("ForwardLambert: textured program did not reflect the required diffuse sampler.");
                Reset();
                return false;
            }
        }

        if (shadowed) {
            LightViewProjectionParameter = sceGxmProgramFindParameterByName(vertexProgram, "HelengineLightViewProjection");
            ShadowBiasParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineShadowBias");
            ShadowTextureParameter = sceGxmProgramFindParameterByName(fragmentProgram, "HelengineShadowTexture");
            if (LightViewProjectionParameter == nullptr || ShadowBiasParameter == nullptr || ShadowTextureParameter == nullptr) {
                AppendForwardLambertProgramTrace("ForwardLambert: shadowed program did not reflect its required directional shadow parameters.");
                Reset();
                return false;
            }
        }

        const SceGxmProgramParameter* positionParameter = sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_POSITION, 0u);
        const SceGxmProgramParameter* normalParameter = sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_NORMAL, 0u);
        const SceGxmProgramParameter* texCoordParameter = textured
            ? sceGxmProgramFindParameterBySemantic(vertexProgram, SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 0u)
            : nullptr;
        if (positionParameter == nullptr || normalParameter == nullptr || (textured && texCoordParameter == nullptr)) {
            AppendForwardLambertProgramTrace("ForwardLambert: position or normal vertex attribute was not reflected by the vertex GXP program.");
            Reset();
            return false;
        }

        SceGxmVertexAttribute attributes[3];
        std::memset(attributes, 0, sizeof(attributes));
        attributes[0].streamIndex = 0u;
        attributes[0].offset = 0u;
        attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attributes[0].componentCount = 3u;
        attributes[0].regIndex = static_cast<std::uint16_t>(sceGxmProgramParameterGetResourceIndex(positionParameter));
        attributes[1] = attributes[0];
        attributes[1].offset = static_cast<std::uint16_t>(sizeof(float) * 3u);
        attributes[1].regIndex = static_cast<std::uint16_t>(sceGxmProgramParameterGetResourceIndex(normalParameter));
        if (textured) {
            attributes[2] = attributes[0];
            attributes[2].offset = static_cast<std::uint16_t>(sizeof(float) * 6u);
            attributes[2].componentCount = 2u;
            attributes[2].regIndex = static_cast<std::uint16_t>(sceGxmProgramParameterGetResourceIndex(texCoordParameter));
        }

        SceGxmVertexStream stream;
        std::memset(&stream, 0, sizeof(stream));
        stream.stride = static_cast<std::uint16_t>(textured ? sizeof(float) * 8u : sizeof(float) * 8u);
        stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_32BIT;
        if (sceGxmShaderPatcherCreateVertexProgram(ShaderPatcher, VertexProgramId, attributes, textured ? 3u : 2u, &stream, 1u, &VertexProgram) < 0
            || sceGxmShaderPatcherCreateFragmentProgram(ShaderPatcher, FragmentProgramId, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, nullptr, vertexProgram, &FragmentProgram) < 0) {
            AppendForwardLambertProgramTrace("ForwardLambert: shader patcher could not create the linked vertex or fragment program.");
            Reset();
            return false;
        }

        Context = activeContext;
        Textured = textured;
        Shadowed = shadowed;
        if (!IsReady()) {
            AppendForwardLambertProgramTrace("ForwardLambert: linked programs were created but the final runtime contract is incomplete.");
            Reset();
            return false;
        }

        AppendForwardLambertProgramTrace("ForwardLambert: artifact-backed GXM program initialized successfully.");
        return true;
    }

    /// Releases shader programs, patcher state, and artifact memory.
    void PsVitaGxmForwardLambertProgram::Reset() {
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
        WorldViewProjectionParameter = nullptr;
        NormalTransformParameter = nullptr;
        BaseColorParameter = nullptr;
        LightDirectionParameter = nullptr;
        LightColorParameter = nullptr;
        AmbientParameter = nullptr;
        DiffuseTextureParameter = nullptr;
        LightViewProjectionParameter = nullptr;
        ShadowBiasParameter = nullptr;
        ShadowTextureParameter = nullptr;
        Textured = false;
        Shadowed = false;
    }

    /// Gets whether the complete forward-Lambert program is ready for drawing.
    bool PsVitaGxmForwardLambertProgram::IsReady() const {
        return Context != nullptr && ShaderPatcher != nullptr && VertexProgram != nullptr && FragmentProgram != nullptr
            && WorldViewProjectionParameter != nullptr && NormalTransformParameter != nullptr && BaseColorParameter != nullptr
            && LightDirectionParameter != nullptr && LightColorParameter != nullptr && AmbientParameter != nullptr
            && (!Textured || DiffuseTextureParameter != nullptr)
            && (!Shadowed || (LightViewProjectionParameter != nullptr && ShadowBiasParameter != nullptr && ShadowTextureParameter != nullptr));
    }

    /// Gets the active GXM context.
    SceGxmContext* PsVitaGxmForwardLambertProgram::GetContext() const { return Context; }

    /// Gets the patched vertex program.
    SceGxmVertexProgram* PsVitaGxmForwardLambertProgram::GetVertexProgram() const { return VertexProgram; }

    /// Gets the patched fragment program.
    SceGxmFragmentProgram* PsVitaGxmForwardLambertProgram::GetFragmentProgram() const { return FragmentProgram; }

    /// Gets the world-view-projection parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetWorldViewProjectionParameter() const { return WorldViewProjectionParameter; }

    /// Gets the normal-transform parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetNormalTransformParameter() const { return NormalTransformParameter; }

    /// Gets the base-color parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetBaseColorParameter() const { return BaseColorParameter; }

    /// Gets the light-direction parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetLightDirectionParameter() const { return LightDirectionParameter; }

    /// Gets the light-color parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetLightColorParameter() const { return LightColorParameter; }

    /// Gets the ambient parameter.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetAmbientParameter() const { return AmbientParameter; }

    /// Gets the diffuse sampler parameter for the textured Forward Standard Shader profile.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetDiffuseTextureParameter() const { return DiffuseTextureParameter; }

    /// Gets the light view-projection parameter for the shadowed Standard Shader profile.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetLightViewProjectionParameter() const { return LightViewProjectionParameter; }

    /// Gets the shadow-bias parameter for the shadowed Standard Shader profile.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetShadowBiasParameter() const { return ShadowBiasParameter; }

    /// Gets the shadow-map sampler parameter for the shadowed Standard Shader profile.
    const SceGxmProgramParameter* PsVitaGxmForwardLambertProgram::GetShadowTextureParameter() const { return ShadowTextureParameter; }
}

#endif
