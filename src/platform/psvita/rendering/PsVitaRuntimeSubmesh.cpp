#include "platform/psvita/rendering/PsVitaRuntimeSubmesh.hpp"

#include <utility>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one PS Vita runtime submesh with copied triangle indices and Vita-owned draw-range metadata.
    PsVitaRuntimeSubmesh::PsVitaRuntimeSubmesh(
        const std::string& materialSlotName,
        int32_t indexStart,
        int32_t indexCount,
        std::vector<std::uint32_t> triangleIndices)
        : MaterialSlotName(materialSlotName)
        , IndexStart(indexStart)
        , IndexCount(indexCount)
        , TriangleIndices(std::move(triangleIndices)) {
    }

    /// Gets the material slot name associated with this runtime submesh.
    const std::string& PsVitaRuntimeSubmesh::GetMaterialSlotName() const {
        return MaterialSlotName;
    }

    /// Gets the first index in the original cooked range represented by this runtime submesh.
    int32_t PsVitaRuntimeSubmesh::GetIndexStart() const {
        return IndexStart;
    }

    /// Gets the number of indices in the original cooked range represented by this runtime submesh.
    int32_t PsVitaRuntimeSubmesh::GetIndexCount() const {
        return IndexCount;
    }

    /// Gets the copied triangle index list owned by this runtime submesh.
    const std::vector<std::uint32_t>& PsVitaRuntimeSubmesh::GetTriangleIndices() const {
        return TriangleIndices;
    }
}

#endif
