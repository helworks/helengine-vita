#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"

#include <stdexcept>

#include "FontAsset.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    /// Builds a runtime texture from raw data.
    ::RuntimeTexture* PsVitaRenderManager2D::BuildTextureFromRaw(::TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
    void PsVitaRenderManager2D::Draw() {
    }

    /// Releases one PS Vita runtime texture previously created by this renderer.
    void PsVitaRenderManager2D::ReleaseTexture(::RuntimeTexture* texture) {
        if (texture == nullptr) {
            throw std::invalid_argument("PS Vita 2D runtime texture release requires one texture instance.");
        }

        rendering::PsVitaRuntimeTexture* psVitaTexture = dynamic_cast<rendering::PsVitaRuntimeTexture*>(texture);
        if (psVitaTexture == nullptr) {
            throw std::runtime_error("PS Vita 2D texture release requires PS Vita runtime textures.");
        }

        TextureCache.ReleaseTexture(psVitaTexture);
    }

    /// Releases one PS Vita font asset and its native-owned object graph.
    void PsVitaRenderManager2D::ReleaseFont(::FontAsset* font) {
        if (font == nullptr) {
            throw std::invalid_argument("PS Vita 2D font release requires one font instance.");
        }

        ::RuntimeTexture* texture = font->get_Texture();
        if (texture != nullptr && !texture->get_IsDisposed()) {
            ReleaseTexture(texture);
            texture->Dispose();
            delete texture;
        }

        font->Dispose();
        delete font;
    }

    /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
    void PsVitaRenderManager2D::FlushReleasedTextures() {
        TextureCache.FlushReleasedTextures();
    }

    /// Draws a rounded rectangle request.
    void PsVitaRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
    }

    /// Draws a sprite request.
    void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
    }

    /// Draws a text request.
    void PsVitaRenderManager2D::DrawText(::ITextDrawable2D* text) {
    }
}

#endif
