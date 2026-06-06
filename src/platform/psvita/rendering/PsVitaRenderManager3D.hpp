#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "RenderManager3D.hpp"

namespace helengine::psvita {
    /// Provides the temporary PS Vita 3D renderer bridge so the runtime can initialize before native model rendering is implemented.
    class PsVitaRenderManager3D final : public ::RenderManager3D {
    public:
        /// Creates the temporary PS Vita 3D renderer with the Vita display size.
        PsVitaRenderManager3D();

        /// Dispatches camera-owned 2D queues into the Vita 2D renderer until native 3D rendering is implemented.
        void Draw() override;

        /// Builds a runtime model from raw data.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;
    };
}

#endif
