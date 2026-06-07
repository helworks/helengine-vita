#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <vector>

#include "RuntimeModel.hpp"
#include "helengine_float3.hpp"

namespace helengine::psvita::rendering {
    /// Stores one PS Vita-owned runtime model copy so mesh components can render after the source model asset has been released by the scene loader.
    class PsVitaRuntimeModel final : public ::RuntimeModel {
    public:
        /// Creates one PS Vita runtime model with copied positions for the white-mesh renderer.
        explicit PsVitaRuntimeModel(std::vector<::float3> positions);

        /// Gets the copied model-space positions owned by this runtime model.
        const std::vector<::float3>& GetPositions() const;

    private:
        /// Stores the copied model-space positions used by the PS Vita white-mesh renderer.
        std::vector<::float3> Positions;
    };
}

#endif
