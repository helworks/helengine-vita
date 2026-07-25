#include "platform/psvita/rendering/PsVitaForwardLambertUniformBinder.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <stdexcept>

namespace helengine::psvita::rendering {
    /// Writes transforms, material color, and directional-light values for one draw.
    void PsVitaForwardLambertUniformBinder::Bind(
        SceGxmContext* context,
        const SceGxmProgramParameter* worldViewProjectionParameter,
        const SceGxmProgramParameter* normalTransformParameter,
        const SceGxmProgramParameter* baseColorParameter,
        const SceGxmProgramParameter* lightDirectionParameter,
        const SceGxmProgramParameter* lightColorParameter,
        const SceGxmProgramParameter* ambientParameter,
        const float* worldViewProjection,
        const float* normalTransform,
        const float* baseColor,
        const float* lightDirection,
        const float* lightColor,
        const float* ambient) {
        if (context == nullptr || worldViewProjectionParameter == nullptr || normalTransformParameter == nullptr
            || baseColorParameter == nullptr || lightDirectionParameter == nullptr || lightColorParameter == nullptr
            || ambientParameter == nullptr || worldViewProjection == nullptr || normalTransform == nullptr
            || baseColor == nullptr || lightDirection == nullptr || lightColor == nullptr || ambient == nullptr) {
            throw std::runtime_error("PS Vita forward-Lambert binding requires complete GXM parameters and uniform values.");
        }

        void* vertexUniformBuffer = nullptr;
        if (sceGxmReserveVertexDefaultUniformBuffer(context, &vertexUniformBuffer) < 0 || vertexUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita forward-Lambert binding could not reserve a vertex uniform buffer.");
        }

        void* fragmentUniformBuffer = nullptr;
        if (sceGxmReserveFragmentDefaultUniformBuffer(context, &fragmentUniformBuffer) < 0 || fragmentUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita forward-Lambert binding could not reserve a fragment uniform buffer.");
        }

        sceGxmSetUniformDataF(vertexUniformBuffer, worldViewProjectionParameter, 0u, 16u, worldViewProjection);
        sceGxmSetUniformDataF(vertexUniformBuffer, normalTransformParameter, 0u, 16u, normalTransform);
        sceGxmSetUniformDataF(fragmentUniformBuffer, baseColorParameter, 0u, 4u, baseColor);
        sceGxmSetUniformDataF(fragmentUniformBuffer, lightDirectionParameter, 0u, 4u, lightDirection);
        sceGxmSetUniformDataF(fragmentUniformBuffer, lightColorParameter, 0u, 4u, lightColor);
        sceGxmSetUniformDataF(fragmentUniformBuffer, ambientParameter, 0u, 4u, ambient);
    }

    /// Writes Standard Shader receiver uniforms without replacing either default uniform-buffer binding mid-draw.
    void PsVitaForwardLambertUniformBinder::BindShadowed(
        SceGxmContext* context,
        const SceGxmProgramParameter* worldViewProjectionParameter,
        const SceGxmProgramParameter* normalTransformParameter,
        const SceGxmProgramParameter* baseColorParameter,
        const SceGxmProgramParameter* lightDirectionParameter,
        const SceGxmProgramParameter* lightColorParameter,
        const SceGxmProgramParameter* ambientParameter,
        const SceGxmProgramParameter* lightViewProjectionParameter,
        const SceGxmProgramParameter* shadowBiasParameter,
        const float* worldViewProjection,
        const float* normalTransform,
        const float* baseColor,
        const float* lightDirection,
        const float* lightColor,
        const float* ambient,
        const float* lightViewProjection,
        const float* shadowBias) {
        if (context == nullptr || worldViewProjectionParameter == nullptr || normalTransformParameter == nullptr
            || baseColorParameter == nullptr || lightDirectionParameter == nullptr || lightColorParameter == nullptr
            || ambientParameter == nullptr || lightViewProjectionParameter == nullptr || shadowBiasParameter == nullptr
            || worldViewProjection == nullptr || normalTransform == nullptr || baseColor == nullptr || lightDirection == nullptr
            || lightColor == nullptr || ambient == nullptr || lightViewProjection == nullptr || shadowBias == nullptr) {
            throw std::runtime_error("PS Vita shadowed Standard binding requires complete GXM parameters and uniform values.");
        }

        void* vertexUniformBuffer = nullptr;
        if (sceGxmReserveVertexDefaultUniformBuffer(context, &vertexUniformBuffer) < 0 || vertexUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita shadowed Standard binding could not reserve a vertex uniform buffer.");
        }

        void* fragmentUniformBuffer = nullptr;
        if (sceGxmReserveFragmentDefaultUniformBuffer(context, &fragmentUniformBuffer) < 0 || fragmentUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita shadowed Standard binding could not reserve a fragment uniform buffer.");
        }

        sceGxmSetUniformDataF(vertexUniformBuffer, worldViewProjectionParameter, 0u, 16u, worldViewProjection);
        sceGxmSetUniformDataF(vertexUniformBuffer, normalTransformParameter, 0u, 16u, normalTransform);
        sceGxmSetUniformDataF(vertexUniformBuffer, lightViewProjectionParameter, 0u, 16u, lightViewProjection);
        sceGxmSetUniformDataF(fragmentUniformBuffer, baseColorParameter, 0u, 4u, baseColor);
        sceGxmSetUniformDataF(fragmentUniformBuffer, lightDirectionParameter, 0u, 4u, lightDirection);
        sceGxmSetUniformDataF(fragmentUniformBuffer, lightColorParameter, 0u, 4u, lightColor);
        sceGxmSetUniformDataF(fragmentUniformBuffer, ambientParameter, 0u, 4u, ambient);
        sceGxmSetUniformDataF(fragmentUniformBuffer, shadowBiasParameter, 0u, 4u, shadowBias);
    }
}

#endif
