#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>
#include <vector>

#include <psp2/gxm.h>

namespace helengine::psvita::rendering {
    /// Owns one artifact-backed GXM program pair that writes encoded light-space depth for shadow casters.
    class PsVitaGxmShadowDepthProgram final {
    public:
        /// Creates one uninitialized depth-only GXM program.
        PsVitaGxmShadowDepthProgram();

        /// Loads, patches, and reflects one compiled ShadowDepth artifact pair.
        bool Initialize(const std::vector<std::uint8_t>& vertexArtifactBytes, const std::vector<std::uint8_t>& fragmentArtifactBytes);

        /// Releases all patched programs, artifact allocations, and mapped patcher memory.
        void Reset();

        /// Gets whether the complete depth-only program contract is ready for drawing.
        bool IsReady() const;

        /// Gets Vita2D's active GXM context used by the open offscreen pass.
        SceGxmContext* GetContext() const;

        /// Gets the patched vertex program.
        SceGxmVertexProgram* GetVertexProgram() const;

        /// Gets the patched fragment program.
        SceGxmFragmentProgram* GetFragmentProgram() const;

        /// Gets the light-space transform uniform reflected from the vertex program.
        const SceGxmProgramParameter* GetLightViewProjectionParameter() const;

    private:
        /// Stores Vita2D's public active GXM context.
        SceGxmContext* Context;

        /// Stores the shader patcher that owns linked native programs.
        SceGxmShaderPatcher* ShaderPatcher;

        /// Stores the registered vertex artifact identity.
        SceGxmShaderPatcherId VertexProgramId;

        /// Stores the registered fragment artifact identity.
        SceGxmShaderPatcherId FragmentProgramId;

        /// Stores the linked vertex program.
        SceGxmVertexProgram* VertexProgram;

        /// Stores the linked fragment program.
        SceGxmFragmentProgram* FragmentProgram;

        /// Stores loaded vertex artifact bytes released after program teardown.
        void* VertexProgramData;

        /// Stores loaded fragment artifact bytes released after program teardown.
        void* FragmentProgramData;

        /// Stores generic GXM memory backing the shader patcher.
        void* PatcherBufferMemory;

        /// Stores vertex-USSE memory backing the shader patcher.
        void* PatcherVertexUsseMemory;

        /// Stores fragment-USSE memory backing the shader patcher.
        void* PatcherFragmentUsseMemory;

        /// Stores the mapped vertex-USSE offset.
        unsigned int PatcherVertexUsseOffset;

        /// Stores the mapped fragment-USSE offset.
        unsigned int PatcherFragmentUsseOffset;

        /// Stores the reflected light-space transform parameter.
        const SceGxmProgramParameter* LightViewProjectionParameter;

    };
}

#endif
