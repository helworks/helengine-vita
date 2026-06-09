#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

namespace helengine::psvita::rendering {
    /// Stores one PS Vita-owned triangle list and draw range for a runtime submesh so the mesh path can avoid stale shared runtime model types.
    class PsVitaRuntimeSubmesh final {
    public:
        /// Creates one PS Vita runtime submesh with copied triangle indices and Vita-owned draw-range metadata.
        PsVitaRuntimeSubmesh(
            const std::string& materialSlotName,
            int32_t indexStart,
            int32_t indexCount,
            std::vector<std::uint32_t> triangleIndices);

        /// Gets the material slot name associated with this runtime submesh.
        const std::string& GetMaterialSlotName() const;

        /// Gets the first index in the original cooked range represented by this runtime submesh.
        int32_t GetIndexStart() const;

        /// Gets the number of indices in the original cooked range represented by this runtime submesh.
        int32_t GetIndexCount() const;

        /// Gets the copied triangle index list owned by this runtime submesh.
        const std::vector<std::uint32_t>& GetTriangleIndices() const;

    private:
        /// Stores the material slot name associated with this runtime submesh.
        std::string MaterialSlotName;

        /// Stores the first index in the original cooked range represented by this runtime submesh.
        int32_t IndexStart;

        /// Stores the number of indices in the original cooked range represented by this runtime submesh.
        int32_t IndexCount;

        /// Stores the copied triangle indices used by the PS Vita white-mesh pass.
        std::vector<std::uint32_t> TriangleIndices;
    };
}

#endif
