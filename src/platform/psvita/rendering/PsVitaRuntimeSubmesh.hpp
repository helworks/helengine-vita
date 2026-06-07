#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

#include "RuntimeSubmesh.hpp"

namespace helengine::psvita::rendering {
    /// Stores one PS Vita-owned triangle list for a runtime submesh so the white-mesh pass can submit indexed geometry without depending on generated asset lifetime.
    class PsVitaRuntimeSubmesh final : public ::RuntimeSubmesh {
    public:
        /// Creates one PS Vita runtime submesh with copied triangle indices and shared runtime draw-range metadata.
        PsVitaRuntimeSubmesh(
            const std::string& materialSlotName,
            int32_t indexStart,
            int32_t indexCount,
            std::vector<std::uint32_t> triangleIndices);

        /// Gets the copied triangle index list owned by this runtime submesh.
        const std::vector<std::uint32_t>& GetTriangleIndices() const;

    private:
        /// Stores the copied triangle indices used by the PS Vita white-mesh pass.
        std::vector<std::uint32_t> TriangleIndices;
    };
}

#endif
