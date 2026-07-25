#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <psp2/gxm.h>

namespace helengine::psvita::rendering {
    /// Uploads one complete forward-Lambert uniform set into Vita GXM default uniform buffers.
    class PsVitaForwardLambertUniformBinder final {
    public:
        /// Writes transforms, material color, and directional-light values for one draw.
        static void Bind(
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
            const float* ambient);

        /// Writes the complete Standard Shader receiver uniform set through one vertex and one fragment default uniform reservation.
        static void BindShadowed(
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
            const float* shadowBias);
    };
}

#endif
