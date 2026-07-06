#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstddef>
#include <cstdint>

#include <psp2/gxm.h>
#include <psp2/types.h>

namespace helengine::psvita::rendering {
    /// <summary>
    /// Owns the runtime-compiled PS Vita solid-color shader objects that the first programmable mesh path binds directly.
    /// </summary>
    class PsVitaGxmSolidColorProgram final {
    public:
        /// <summary>
        /// Creates one empty runtime-program wrapper that will compile and patch its shader state during renderer initialization.
        /// </summary>
        PsVitaGxmSolidColorProgram();

        /// <summary>
        /// Compiles, patches, and binds the PS Vita solid-color shader programs needed by runtime mesh draws.
        /// </summary>
        bool Initialize();

        /// <summary>
        /// Releases the compiled shader programs, patcher allocations, and resolved uniform bindings owned by this runtime-program wrapper.
        /// </summary>
        void Reset();

        /// <summary>
        /// Gets whether the runtime-compiled GXM context, shader programs, and uniform bindings are all available.
        /// </summary>
        bool IsReady() const;

        /// <summary>
        /// Gets the active GXM context used for direct programmable draw calls.
        /// </summary>
        SceGxmContext* GetContext() const;

        /// <summary>
        /// Gets the runtime-compiled solid-color vertex program.
        /// </summary>
        SceGxmVertexProgram* GetVertexProgram() const;

        /// <summary>
        /// Gets the runtime-compiled solid-color fragment program.
        /// </summary>
        SceGxmFragmentProgram* GetFragmentProgram() const;

        /// <summary>
        /// Gets the world-view-projection uniform parameter used by the runtime-compiled solid-color vertex program.
        /// </summary>
        const SceGxmProgramParameter* GetWorldViewProjectionParameter() const;

        /// <summary>
        /// Gets the base-color uniform parameter used by the runtime-compiled solid-color fragment program.
        /// </summary>
        const SceGxmProgramParameter* GetBaseColorParameter() const;

    private:
        /// <summary>
        /// Stores the active GXM context used for direct programmable draw calls.
        /// </summary>
        SceGxmContext* Context;

        /// <summary>
        /// Stores the shader patcher that owns the runtime-compiled solid-color programs.
        /// </summary>
        SceGxmShaderPatcher* ShaderPatcher;

        /// <summary>
        /// Stores the patcher registration handle for the runtime-compiled solid-color vertex program header.
        /// </summary>
        SceGxmShaderPatcherId VertexProgramId;

        /// <summary>
        /// Stores the patcher registration handle for the runtime-compiled solid-color fragment program header.
        /// </summary>
        SceGxmShaderPatcherId FragmentProgramId;

        /// <summary>
        /// Stores the runtime-compiled solid-color vertex program.
        /// </summary>
        SceGxmVertexProgram* VertexProgram;

        /// <summary>
        /// Stores the runtime-compiled solid-color fragment program.
        /// </summary>
        SceGxmFragmentProgram* FragmentProgram;

        /// <summary>
        /// Stores the world-view-projection uniform parameter used by the runtime-compiled solid-color vertex program.
        /// </summary>
        const SceGxmProgramParameter* WorldViewProjectionParameter;

        /// <summary>
        /// Stores the base-color uniform parameter used by the runtime-compiled solid-color fragment program.
        /// </summary>
        const SceGxmProgramParameter* BaseColorParameter;

        /// <summary>
        /// Stores one persistent copy of the runtime-compiled solid-color vertex program header.
        /// </summary>
        void* VertexProgramData;

        /// <summary>
        /// Stores the size in bytes of the runtime-compiled solid-color vertex program header copy.
        /// </summary>
        std::size_t VertexProgramDataSize;

        /// <summary>
        /// Stores one persistent copy of the runtime-compiled solid-color fragment program header.
        /// </summary>
        void* FragmentProgramData;

        /// <summary>
        /// Stores the size in bytes of the runtime-compiled solid-color fragment program header copy.
        /// </summary>
        std::size_t FragmentProgramDataSize;

        /// <summary>
        /// Stores the mapped shader patcher backing buffer used for runtime program creation.
        /// </summary>
        void* PatcherBufferMemory;

        /// <summary>
        /// Stores the size in bytes of the mapped shader patcher backing buffer.
        /// </summary>
        std::size_t PatcherBufferMemorySize;

        /// <summary>
        /// Stores the mapped vertex USSE memory reserved for the runtime shader patcher.
        /// </summary>
        void* PatcherVertexUsseMemory;

        /// <summary>
        /// Stores the size in bytes of the mapped vertex USSE memory reserved for the runtime shader patcher.
        /// </summary>
        std::size_t PatcherVertexUsseMemorySize;

        /// <summary>
        /// Stores the vertex USSE offset returned by the GXM runtime for the shader patcher allocation.
        /// </summary>
        unsigned int PatcherVertexUsseOffset;

        /// <summary>
        /// Stores the mapped fragment USSE memory reserved for the runtime shader patcher.
        /// </summary>
        void* PatcherFragmentUsseMemory;

        /// <summary>
        /// Stores the size in bytes of the mapped fragment USSE memory reserved for the runtime shader patcher.
        /// </summary>
        std::size_t PatcherFragmentUsseMemorySize;

        /// <summary>
        /// Stores the fragment USSE offset returned by the GXM runtime for the shader patcher allocation.
        /// </summary>
        unsigned int PatcherFragmentUsseOffset;

        /// <summary>
        /// Stores the loaded shader-compiler module identifier for the current process lifetime.
        /// </summary>
        SceUID ShaderCompilerModuleId;

        /// <summary>
        /// Stores whether one prior initialization attempt has already failed and should no longer be retried during the current process lifetime.
        /// </summary>
        bool InitializationFailed;
    };
}

#endif
