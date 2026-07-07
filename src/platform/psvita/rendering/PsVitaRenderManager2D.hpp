#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <vector>

class ICamera;
class IDrawable2D;

#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"
#include "platform/psvita/rendering/PsVitaQueuedQuad.hpp"
#include "platform/psvita/rendering/PsVitaSolidColorVertex.hpp"
#include "platform/psvita/rendering/PsVitaTextureCache.hpp"
#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"
#include "byte4.hpp"
#include "float3.hpp"
#include "int2.hpp"

namespace helengine::psvita {
    /// Provides the temporary PS Vita 2D renderer bridge so the runtime can initialize before native rendering is implemented.
    class PsVitaRenderManager2D final : public ::RenderManager2D, public ::IRenderVisitor2D {
    public:
        /// Builds a runtime texture from one packaged cooked texture asset.
        ::RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds a runtime texture from raw data.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;

        /// Assigns the native PS Vita GXM renderer that will receive flushed quad batches.
        void SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer);

        /// Draws one camera's ordered 2D queue into the queued-quad submission path.
        void DrawCamera(::ICamera* camera);

        /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
        void Draw() override;

        /// Releases one PS Vita runtime texture previously created by this renderer.
        void ReleaseTexture(::RuntimeTexture* texture);

        /// Releases one PS Vita font asset and its native-owned object graph.
        void ReleaseFont(::FontAsset* font);

        /// Marks that one presented PS Vita frame completed so deferred texture and font release work can advance at the post-present safe point.
        void NotifyFramePresented();

        /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
        void FlushReleasedTextures();

        /// Draws a rounded rectangle request.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;

        /// Draws a sprite request.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;

        /// Draws a text request.
        void DrawText(::ITextDrawable2D* text) override;

        /// Visits one ordered drawable and forwards it through its generated-core draw entry point.
        void Visit(::IDrawable2D* drawable) override;

    private:
        /// Appends one queued solid-color triangle to the current frame submission list.
        void AppendSolidTriangle(
            float x0,
            float y0,
            float x1,
            float y1,
            float x2,
            float y2,
            std::uint32_t colorAbgr,
            std::uint8_t renderOrder);

        /// Appends one queued solid-color quad as two triangles to the current frame submission list.
        void AppendSolidQuad(
            float left,
            float top,
            float right,
            float bottom,
            std::uint32_t colorAbgr,
            std::uint8_t renderOrder);

        /// Appends one filled rounded rectangle using solid-color triangles for the current menu pass.
        void AppendFilledRoundedRect(
            const float3& position,
            const ::int2& size,
            double radius,
            std::uint32_t colorAbgr,
            std::uint8_t renderOrder);

        /// Appends one rounded-corner triangle fan using the supplied angle range.
        void AppendRoundedCornerFan(
            float centerX,
            float centerY,
            double radius,
            double startAngleRadians,
            double endAngleRadians,
            std::uint32_t colorAbgr,
            std::uint8_t renderOrder);

        /// Packs one engine byte color into the ABGR layout expected by Vita2D.
        static std::uint32_t PackColorAbgr(const byte4& color);

        /// Stores the native PS Vita GXM renderer that receives queued sprite batches.
        rendering::PsVitaGxmRenderer* GxmRenderer = nullptr;

        /// Stores the queued solid-color rounded-rectangle triangles built during the current frame.
        std::vector<rendering::PsVitaSolidColorVertex> QueuedSolidColorTriangles;

        /// Stores the queued sprite quads built during the current frame.
        std::vector<rendering::PsVitaQueuedQuad> QueuedQuads;

        /// Stores font assets whose disposal must wait until deferred texture releases reach the explicit flush boundary.
        std::vector<::FontAsset*> ReleasedFonts;

        /// Stores whether one presented PS Vita frame completed since the last deferred-release flush attempt.
        bool PresentedFramePendingFlush = false;

        /// Stores the runtime texture cache used by the temporary PS Vita 2D renderer.
        rendering::PsVitaTextureCache TextureCache;
    };
}

#endif
