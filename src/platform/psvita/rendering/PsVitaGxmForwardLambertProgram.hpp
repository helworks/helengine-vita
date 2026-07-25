#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <string>
#include <vector>

#include <psp2/gxm.h>

namespace helengine::psvita::rendering {
    /// Owns one artifact-backed GXM forward-Lambert vertex/fragment program pair.
    class PsVitaGxmForwardLambertProgram final {
    public:
        /// Creates one uninitialized forward-Lambert program wrapper.
        PsVitaGxmForwardLambertProgram();

        /// Loads artifacts, creates the patcher state, and resolves the Lambert parameter contract.
        bool Initialize(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes);

        /// Loads artifacts for the textured Forward Standard Shader profile and resolves its diffuse sampler contract.
        bool InitializeTextured(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes);

        /// Loads artifacts for the textured Standard Shader profile that receives one directional shadow map.
        bool InitializeTexturedShadowed(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes);

        /// Releases shader programs, patcher state, and artifact memory.
        void Reset();

        /// Gets whether the complete forward-Lambert program is ready for drawing.
        bool IsReady() const;

        /// Gets the GXM context owned by the active Vita renderer.
        SceGxmContext* GetContext() const;

        /// Gets the patched vertex program.
        SceGxmVertexProgram* GetVertexProgram() const;

        /// Gets the patched fragment program.
        SceGxmFragmentProgram* GetFragmentProgram() const;

        /// Gets the reflected world-view-projection parameter.
        const SceGxmProgramParameter* GetWorldViewProjectionParameter() const;

        /// Gets the reflected normal-transform parameter.
        const SceGxmProgramParameter* GetNormalTransformParameter() const;

        /// Gets the reflected base-color parameter.
        const SceGxmProgramParameter* GetBaseColorParameter() const;

        /// Gets the reflected light-direction parameter.
        const SceGxmProgramParameter* GetLightDirectionParameter() const;

        /// Gets the reflected light-color parameter.
        const SceGxmProgramParameter* GetLightColorParameter() const;

        /// Gets the reflected ambient parameter.
        const SceGxmProgramParameter* GetAmbientParameter() const;

        /// Gets the reflected diffuse sampler parameter for the textured profile, or null for untextured Lambert.
        const SceGxmProgramParameter* GetDiffuseTextureParameter() const;

        /// Gets the reflected light view-projection parameter for the shadowed Standard Shader profile.
        const SceGxmProgramParameter* GetLightViewProjectionParameter() const;

        /// Gets the reflected shadow-bias parameter for the shadowed Standard Shader profile.
        const SceGxmProgramParameter* GetShadowBiasParameter() const;

        /// Gets the reflected shadow-map sampler parameter for the shadowed Standard Shader profile.
        const SceGxmProgramParameter* GetShadowTextureParameter() const;

    private:
        /// Stores the active GXM context.
        SceGxmContext* Context;

        /// Stores the GXM shader patcher.
        SceGxmShaderPatcher* ShaderPatcher;

        /// Stores the registered vertex program identity.
        SceGxmShaderPatcherId VertexProgramId;

        /// Stores the registered fragment program identity.
        SceGxmShaderPatcherId FragmentProgramId;

        /// Stores the patched vertex program.
        SceGxmVertexProgram* VertexProgram;

        /// Stores the patched fragment program.
        SceGxmFragmentProgram* FragmentProgram;

        /// Stores the loaded vertex program header bytes.
        void* VertexProgramData;

        /// Stores the loaded fragment program header bytes.
        void* FragmentProgramData;

        /// Stores the patcher backing allocation.
        void* PatcherBufferMemory;

        /// Stores the patcher vertex USSE allocation.
        void* PatcherVertexUsseMemory;

        /// Stores the patcher fragment USSE allocation.
        void* PatcherFragmentUsseMemory;

        /// Stores the patcher vertex USSE offset.
        unsigned int PatcherVertexUsseOffset;

        /// Stores the patcher fragment USSE offset.
        unsigned int PatcherFragmentUsseOffset;

        /// Stores the world-view-projection parameter.
        const SceGxmProgramParameter* WorldViewProjectionParameter;

        /// Stores the normal-transform parameter.
        const SceGxmProgramParameter* NormalTransformParameter;

        /// Stores the base-color parameter.
        const SceGxmProgramParameter* BaseColorParameter;

        /// Stores the light-direction parameter.
        const SceGxmProgramParameter* LightDirectionParameter;

        /// Stores the light-color parameter.
        const SceGxmProgramParameter* LightColorParameter;

        /// Stores the ambient parameter.
        const SceGxmProgramParameter* AmbientParameter;

        /// Stores the reflected diffuse sampler parameter for the textured profile.
        const SceGxmProgramParameter* DiffuseTextureParameter;

        /// Stores the reflected light view-projection parameter for shadowed Standard Shader draws.
        const SceGxmProgramParameter* LightViewProjectionParameter;

        /// Stores the reflected shadow-bias parameter for shadowed Standard Shader draws.
        const SceGxmProgramParameter* ShadowBiasParameter;

        /// Stores the reflected shadow-map sampler parameter for shadowed Standard Shader draws.
        const SceGxmProgramParameter* ShadowTextureParameter;

        /// Stores whether the active artifact pair requires UV and diffuse-texture binding.
        bool Textured;

        /// Stores whether the active artifact pair requires directional shadow bindings.
        bool Shadowed;

        /// Initializes either the untextured Lambert or textured Forward Standard artifact contract.
        bool InitializeInternal(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes, bool textured, bool shadowed);
    };
}

#endif
