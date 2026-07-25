#include "platform/psvita/rendering/PsVitaGxmShadowMap.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <stdexcept>

#include <psp2/gxm.h>

namespace {
    /// Stores the fixed square resolution used by the first directional shadow tier.
    constexpr unsigned int ShadowMapResolutionPixels = 256u;

    /// Stores the color format used to preserve depth encoded by the depth-only artifact.
    constexpr SceGxmTextureFormat ShadowMapTextureFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
}

namespace helengine::psvita::rendering {
    /// Creates one uninitialized offscreen shadow-map owner.
    PsVitaGxmShadowMap::PsVitaGxmShadowMap()
        : Texture(nullptr)
        , DepthPassActive(false) {
    }

    /// Allocates the fixed-resolution native render target required before drawing shadow casters.
    bool PsVitaGxmShadowMap::Initialize() {
        if (Texture != nullptr) {
            return true;
        }

        Texture = vita2d_create_empty_texture_rendertarget(
            ShadowMapResolutionPixels,
            ShadowMapResolutionPixels,
            ShadowMapTextureFormat);
        return Texture != nullptr;
    }

    /// Releases the native render target after any active shadow pass has finished.
    void PsVitaGxmShadowMap::Reset() {
        if (DepthPassActive) {
            vita2d_end_drawing();
            DepthPassActive = false;
        }

        if (Texture != nullptr) {
            vita2d_free_texture(Texture);
            Texture = nullptr;
        }
    }

    /// Begins rendering into the offscreen Vita2D target.
    void PsVitaGxmShadowMap::BeginDepthPass() {
        if (Texture == nullptr) {
            throw std::runtime_error("PS Vita shadow map must be initialized before beginning its depth pass.");
        }
        if (DepthPassActive) {
            throw std::runtime_error("PS Vita shadow map depth pass is already active.");
        }

        vita2d_start_drawing_advanced(Texture, 0u);
        vita2d_clear_screen();
        DepthPassActive = true;
    }

    /// Ends the offscreen Vita2D target pass so the normal frame target can resume.
    void PsVitaGxmShadowMap::EndDepthPass() {
        if (!DepthPassActive) {
            throw std::runtime_error("PS Vita shadow map depth pass cannot end because it is not active.");
        }

        vita2d_end_drawing();
        DepthPassActive = false;
    }

    /// Gets the texture containing the latest completed shadow depth pass.
    vita2d_texture* PsVitaGxmShadowMap::GetTexture() const {
        return Texture;
    }

    /// Gets whether a depth pass currently owns Vita2D drawing.
    bool PsVitaGxmShadowMap::IsDepthPassActive() const {
        return DepthPassActive;
    }
}

#endif
