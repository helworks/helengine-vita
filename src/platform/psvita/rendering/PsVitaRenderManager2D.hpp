#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <vector>

class ICamera;
class IDrawable2D;

#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaQueuedQuad.hpp"
#include "platform/psvita/rendering/PsVitaTextureCache.hpp"
#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"

namespace helengine::psvita {
    /// Provides the temporary PS Vita 2D renderer bridge so the runtime can initialize before native rendering is implemented.
    class PsVitaRenderManager2D final : public ::RenderManager2D, public ::IRenderVisitor2D {
    public:
        /// Builds a runtime texture from one packaged cooked texture asset.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath) override;

        /// Builds a runtime texture from raw data.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;

        /// Assigns the native PS Vita GXM renderer that will receive flushed quad batches.
        void SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer);

        /// Draws one camera's ordered 2D queue into the queued-quad submission path.
        void DrawCamera(::ICamera* camera);

        /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
        void Draw() override;

        /// Releases one PS Vita runtime texture previously created by this renderer.
        void ReleaseTexture(::RuntimeTexture* texture) override;

        /// Releases one PS Vita font asset and its native-owned object graph.
        void ReleaseFont(::FontAsset* font) override;

        /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
        void FlushReleasedTextures() override;

        /// Draws a rounded rectangle request.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;

        /// Draws a sprite request.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;

        /// Draws a text request.
        void DrawText(::ITextDrawable2D* text) override;

        /// Visits one ordered drawable and forwards it through its generated-core draw entry point.
        void Visit(::IDrawable2D* drawable) override;

    private:
        /// Stores the native PS Vita GXM renderer that receives queued sprite batches.
        rendering::PsVitaGxmRenderer* GxmRenderer = nullptr;

        /// Stores the queued sprite quads built during the current frame.
        std::vector<rendering::PsVitaQueuedQuad> QueuedQuads;

        /// Stores the runtime texture cache used by the temporary PS Vita 2D renderer.
        rendering::PsVitaTextureCache TextureCache;
    };
}

#endif
