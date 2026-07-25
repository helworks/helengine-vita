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

        /// Gets the runtime parameter contract version required by this material.
        std::uint32_t GetParameterContractVersion() const;

        /// Sets the runtime parameter contract version required by this material.
        void SetParameterContractVersion(std::uint32_t parameterContractVersion);

        /// Gets the packed ABGR base color referenced by this runtime material.
        std::uint32_t GetBaseColorAbgr() const;

        /// Sets the packed ABGR base color referenced by this runtime material.
        void SetBaseColorAbgr(std::uint32_t baseColorAbgr);

        /// Gets whether this material requires one diffuse texture for its selected shader profile.
        bool GetRequiresDiffuseTexture() const;

        /// Sets whether this material requires one diffuse texture for its selected shader profile.
        void SetRequiresDiffuseTexture(bool requiresDiffuseTexture);

        /// Gets the cooked diffuse texture asset identity required by this material.
        const std::string& GetDiffuseTextureAssetId() const;

        /// Sets the cooked diffuse texture asset identity required by this material.
        void SetDiffuseTextureAssetId(const std::string& diffuseTextureAssetId);

        /// Gets whether this material contributes geometry to directional shadow depth passes.
        bool GetCastsShadows() const;

        /// Sets whether this material contributes geometry to directional shadow depth passes.
        void SetCastsShadows(bool castsShadows);

        /// Gets whether this material receives directional shadow attenuation in forward passes.
        bool GetReceivesShadows() const;

        /// Sets whether this material receives directional shadow attenuation in forward passes.
        void SetReceivesShadows(bool receivesShadows);

    private:
        /// Stores the shared shader asset identifier referenced by this runtime material.
        std::string ShaderAssetIdValue;

        /// Stores the vertex-program name referenced by this runtime material.
        std::string VertexProgramNameValue;

        /// Stores the pixel-program name referenced by this runtime material.
        std::string PixelProgramNameValue;

        /// Stores the shader variant name referenced by this runtime material.
        std::string VariantNameValue;

        /// Stores the runtime parameter contract version required by this material.
        std::uint32_t ParameterContractVersionValue = 0u;

        /// Stores the packed ABGR base color referenced by this runtime material.
        std::uint32_t BaseColorAbgrValue = 0xFFFFFFFFu;

        /// Stores whether this material requires one diffuse texture for its selected shader profile.
        bool RequiresDiffuseTextureValue = false;

        /// Stores the cooked diffuse texture asset identity required by this material.
        std::string DiffuseTextureAssetIdValue;

        /// Stores whether this material contributes geometry to directional shadow depth passes.
        bool CastsShadowsValue = true;

        /// Stores whether this material receives directional shadow attenuation in forward passes.
        bool ReceivesShadowsValue = true;
    };
}

#endif
