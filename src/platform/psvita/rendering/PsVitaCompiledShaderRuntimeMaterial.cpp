#include "platform/psvita/rendering/PsVitaCompiledShaderRuntimeMaterial.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Gets the shared shader asset identifier referenced by this runtime material.
    const std::string& PsVitaCompiledShaderRuntimeMaterial::GetShaderAssetId() const {
        return ShaderAssetIdValue;
    }

    /// Sets the shared shader asset identifier referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetShaderAssetId(const std::string& shaderAssetId) {
        ShaderAssetIdValue = shaderAssetId;
    }

    /// Gets the vertex-program name referenced by this runtime material.
    const std::string& PsVitaCompiledShaderRuntimeMaterial::GetVertexProgramName() const {
        return VertexProgramNameValue;
    }

    /// Sets the vertex-program name referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetVertexProgramName(const std::string& vertexProgramName) {
        VertexProgramNameValue = vertexProgramName;
    }

    /// Gets the pixel-program name referenced by this runtime material.
    const std::string& PsVitaCompiledShaderRuntimeMaterial::GetPixelProgramName() const {
        return PixelProgramNameValue;
    }

    /// Sets the pixel-program name referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetPixelProgramName(const std::string& pixelProgramName) {
        PixelProgramNameValue = pixelProgramName;
    }

    /// Gets the shader variant name referenced by this runtime material.
    const std::string& PsVitaCompiledShaderRuntimeMaterial::GetVariantName() const {
        return VariantNameValue;
    }

    /// Sets the shader variant name referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetVariantName(const std::string& variantName) {
        VariantNameValue = variantName;
    }

    /// Gets the packed ABGR base color referenced by this runtime material.
    std::uint32_t PsVitaCompiledShaderRuntimeMaterial::GetBaseColorAbgr() const {
        return BaseColorAbgrValue;
    }

    /// Sets the packed ABGR base color referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetBaseColorAbgr(std::uint32_t baseColorAbgr) {
        BaseColorAbgrValue = baseColorAbgr;
    }
}

#endif
