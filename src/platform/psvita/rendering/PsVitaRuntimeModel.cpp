#include "platform/psvita/rendering/PsVitaRuntimeModel.hpp"

#include <utility>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one PS Vita runtime model with copied positions for the white-mesh renderer.
    PsVitaRuntimeModel::PsVitaRuntimeModel(std::vector<::float3> positions)
        : Positions(std::move(positions)) {
    }

    /// Gets the copied model-space positions owned by this runtime model.
    const std::vector<::float3>& PsVitaRuntimeModel::GetPositions() const {
        return Positions;
    }
}

#endif
