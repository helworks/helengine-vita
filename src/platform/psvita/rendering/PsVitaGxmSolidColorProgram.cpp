#include "platform/psvita/rendering/PsVitaGxmSolidColorProgram.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// <summary>
    /// Creates one empty wrapper that will remain unbound until the real compiled PS Vita shader pipeline is available.
    /// </summary>
    PsVitaGxmSolidColorProgram::PsVitaGxmSolidColorProgram()
        : Context(nullptr)
        , VertexProgram(nullptr)
        , FragmentProgram(nullptr)
        , WorldViewProjectionParameter(nullptr) {
    }

    /// <summary>
    /// Clears the wrapper state so callers can detect that the compiled PS Vita solid-color program has not been bound yet.
    /// </summary>
    void PsVitaGxmSolidColorProgram::Initialize() {
        Context = nullptr;
        VertexProgram = nullptr;
        FragmentProgram = nullptr;
        WorldViewProjectionParameter = nullptr;
    }

    /// <summary>
    /// Clears the wrapper state so future draw calls can continue reporting that the programmable mesh path is unavailable.
    /// </summary>
    void PsVitaGxmSolidColorProgram::Reset() {
        Context = nullptr;
        VertexProgram = nullptr;
        FragmentProgram = nullptr;
        WorldViewProjectionParameter = nullptr;
    }

    /// <summary>
    /// Gets whether one compiled PS Vita solid-color program has been fully bound.
    /// </summary>
    bool PsVitaGxmSolidColorProgram::IsReady() const {
        return Context != nullptr
            && VertexProgram != nullptr
            && FragmentProgram != nullptr
            && WorldViewProjectionParameter != nullptr;
    }

    /// <summary>
    /// Gets the currently bound GXM context used for direct programmable draw calls.
    /// </summary>
    SceGxmContext* PsVitaGxmSolidColorProgram::GetContext() const {
        return Context;
    }

    /// <summary>
    /// Gets the currently bound solid-color vertex program.
    /// </summary>
    SceGxmVertexProgram* PsVitaGxmSolidColorProgram::GetVertexProgram() const {
        return VertexProgram;
    }

    /// <summary>
    /// Gets the currently bound solid-color fragment program.
    /// </summary>
    SceGxmFragmentProgram* PsVitaGxmSolidColorProgram::GetFragmentProgram() const {
        return FragmentProgram;
    }

    /// <summary>
    /// Gets the currently bound world-view-projection parameter used by the solid-color vertex program.
    /// </summary>
    const SceGxmProgramParameter* PsVitaGxmSolidColorProgram::GetWorldViewProjectionParameter() const {
        return WorldViewProjectionParameter;
    }
}

#endif
