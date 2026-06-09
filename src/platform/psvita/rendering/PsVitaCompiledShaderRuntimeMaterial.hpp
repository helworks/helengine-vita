#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <string>

#include "RuntimeMaterial.hpp"

namespace helengine::psvita::rendering {
    /// Stores one PS Vita-specific runtime material payload for the temporary shared solid-color shader bridge.
    class PsVitaCompiledShaderRuntimeMaterial final : public ::RuntimeMaterial {
    public:
        /// Gets the shared shader asset identifier referenced by this runtime material.
        const std::string& GetShaderAssetId() const;

        /// Sets the shared shader asset identifier referenced by this runtime material.
        void SetShaderAssetId(const std::string& shaderAssetId);

        /// Gets the vertex-program name referenced by this runtime material.
        const std::string& GetVertexProgramName() const;

        /// Sets the vertex-program name referenced by this runtime material.
        void SetVertexProgramName(const std::string& vertexProgramName);

        /// Gets the pixel-program name referenced by this runtime material.
        const std::string& GetPixelProgramName() const;

        /// Sets the pixel-program name referenced by this runtime material.
        void SetPixelProgramName(const std::string& pixelProgramName);

        /// Gets the shader variant name referenced by this runtime material.
        const std::string& GetVariantName() const;

        /// Sets the shader variant name referenced by this runtime material.
        void SetVariantName(const std::string& variantName);

        /// Gets the packed ABGR base color referenced by this runtime material.
        std::uint32_t GetBaseColorAbgr() const;

        /// Sets the packed ABGR base color referenced by this runtime material.
        void SetBaseColorAbgr(std::uint32_t baseColorAbgr);

    private:
        /// Stores the shared shader asset identifier referenced by this runtime material.
        std::string ShaderAssetIdValue;

        /// Stores the vertex-program name referenced by this runtime material.
        std::string VertexProgramNameValue;

        /// Stores the pixel-program name referenced by this runtime material.
        std::string PixelProgramNameValue;

        /// Stores the shader variant name referenced by this runtime material.
        std::string VariantNameValue;

        /// Stores the packed ABGR base color referenced by this runtime material.
        std::uint32_t BaseColorAbgrValue = 0xFFFFFFFFu;
    };
}

#endif
