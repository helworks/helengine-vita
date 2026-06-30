#include "platform/psvita/rendering/PsVitaRuntimeModel.hpp"

#include <utility>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one PS Vita runtime model with copied positions and normals for the Lambert fallback renderer.
    PsVitaRuntimeModel::PsVitaRuntimeModel(std::vector<::float3> positions, std::vector<::float3> normals)
        : Positions(std::move(positions))
        , Normals(std::move(normals))
        , Submeshes(nullptr) {
    }

    /// Gets the copied model-space positions owned by this runtime model.
    const std::vector<::float3>& PsVitaRuntimeModel::GetPositions() const {
        return Positions;
    }

    /// Gets the copied model-space normals owned by this runtime model.
    const std::vector<::float3>& PsVitaRuntimeModel::GetNormals() const {
        return Normals;
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
