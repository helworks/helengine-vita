#include "platform/psvita/rendering/PsVitaRuntimeSubmesh.hpp"

#include <utility>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one PS Vita runtime submesh with copied triangle indices and shared runtime draw-range metadata.
    PsVitaRuntimeSubmesh::PsVitaRuntimeSubmesh(
        const std::string& materialSlotName,
        int32_t indexStart,
        int32_t indexCount,
        std::vector<std::uint32_t> triangleIndices)
        : TriangleIndices(std::move(triangleIndices)) {
        set_MaterialSlotName(materialSlotName);
        set_IndexStart(indexStart);
        set_IndexCount(indexCount);
    }

    /// Gets the copied triangle index list owned by this runtime submesh.
    const std::vector<std::uint32_t>& PsVitaRuntimeSubmesh::GetTriangleIndices() const {
        return TriangleIndices;
    }
}

#endif
