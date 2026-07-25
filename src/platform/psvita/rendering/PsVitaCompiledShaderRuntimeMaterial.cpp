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

    /// Gets the runtime parameter contract version required by this material.
    std::uint32_t PsVitaCompiledShaderRuntimeMaterial::GetParameterContractVersion() const {
        return ParameterContractVersionValue;
    }

    /// Sets the runtime parameter contract version required by this material.
    void PsVitaCompiledShaderRuntimeMaterial::SetParameterContractVersion(std::uint32_t parameterContractVersion) {
        ParameterContractVersionValue = parameterContractVersion;
    }

    /// Gets the packed ABGR base color referenced by this runtime material.
    std::uint32_t PsVitaCompiledShaderRuntimeMaterial::GetBaseColorAbgr() const {
        return BaseColorAbgrValue;
    }

    /// Sets the packed ABGR base color referenced by this runtime material.
    void PsVitaCompiledShaderRuntimeMaterial::SetBaseColorAbgr(std::uint32_t baseColorAbgr) {
        BaseColorAbgrValue = baseColorAbgr;
    }

    /// Gets whether this material requires one diffuse texture for its selected shader profile.
    bool PsVitaCompiledShaderRuntimeMaterial::GetRequiresDiffuseTexture() const {
        return RequiresDiffuseTextureValue;
    }

    /// Sets whether this material requires one diffuse texture for its selected shader profile.
    void PsVitaCompiledShaderRuntimeMaterial::SetRequiresDiffuseTexture(bool requiresDiffuseTexture) {
        RequiresDiffuseTextureValue = requiresDiffuseTexture;
    }

    /// Gets the cooked diffuse texture asset identity required by this material.
    const std::string& PsVitaCompiledShaderRuntimeMaterial::GetDiffuseTextureAssetId() const {
        return DiffuseTextureAssetIdValue;
    }

    /// Sets the cooked diffuse texture asset identity required by this material.
    void PsVitaCompiledShaderRuntimeMaterial::SetDiffuseTextureAssetId(const std::string& diffuseTextureAssetId) {
        DiffuseTextureAssetIdValue = diffuseTextureAssetId;
    }

    /// Gets whether this material contributes geometry to directional shadow depth passes.
    bool PsVitaCompiledShaderRuntimeMaterial::GetCastsShadows() const {
        return CastsShadowsValue;
    }

    /// Sets whether this material contributes geometry to directional shadow depth passes.
    void PsVitaCompiledShaderRuntimeMaterial::SetCastsShadows(bool castsShadows) {
        CastsShadowsValue = castsShadows;
    }

    /// Gets whether this material receives directional shadow attenuation in forward passes.
    bool PsVitaCompiledShaderRuntimeMaterial::GetReceivesShadows() const {
        return ReceivesShadowsValue;
    }

    /// Sets whether this material receives directional shadow attenuation in forward passes.
    void PsVitaCompiledShaderRuntimeMaterial::SetReceivesShadows(bool receivesShadows) {
        ReceivesShadowsValue = receivesShadows;
    }
}

#endif
