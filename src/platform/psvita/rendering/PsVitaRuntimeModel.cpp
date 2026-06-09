#include "platform/psvita/rendering/PsVitaRuntimeModel.hpp"

#include <utility>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one PS Vita runtime model with copied positions for the white-mesh renderer.
    PsVitaRuntimeModel::PsVitaRuntimeModel(std::vector<::float3> positions)
        : Positions(std::move(positions))
        , Submeshes(nullptr) {
    }

    /// Gets the copied model-space positions owned by this runtime model.
    const std::vector<::float3>& PsVitaRuntimeModel::GetPositions() const {
        return Positions;
    }

    /// Gets the Vita-owned runtime submesh collection used by the mesh path.
    Array<PsVitaRuntimeSubmesh*>* PsVitaRuntimeModel::get_Submeshes() const {
        return Submeshes;
    }

    /// Stores the Vita-owned runtime submesh collection used by the mesh path.
    void PsVitaRuntimeModel::SetSubmeshes(Array<PsVitaRuntimeSubmesh*>* submeshes) {
        Submeshes = submeshes;
    }
}

#endif
