#include "SpriteQuad2D.hpp"
#include "Batch2DGeometry.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "platform/psvita/rendering/PsVitaDrawOrder.hpp"

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "Entity.hpp"
#include "FontChar.hpp"
#include "FontAsset.hpp"
#include "FontInfo.hpp"
#include "ICamera.hpp"
#include "IDrawable2D.hpp"
#include "IRenderQueue2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "TextLayoutAlignmentUtils.hpp"
#include "TextLayoutUtils.hpp"
#include "byte4.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "int2.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"
#include "platform/psvita/rendering/PsVitaTextEffectPass.hpp"
#include "TextureAsset.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    namespace {
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";
        constexpr bool EnablePsVitaBootTraceLogging = false;
        constexpr int RoundedCornerSegmentCount = 8;

        void AppendRenderTrace(const char* message) {
            if (!EnablePsVitaBootTraceLogging || message == nullptr) {
                return;
            }

            std::FILE* file = std::fopen(BootTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::fputs(message, file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }

    /// Builds a runtime texture from one packaged cooked texture asset.
    ::RuntimeTexture* PsVitaRenderManager2D::BuildTextureFromCooked(HeCppString cookedAssetPath, IContentStreamSource* contentStreamSource) {
        (void)contentStreamSource;
        AppendRenderTrace("RenderManager2D::BuildTextureFromCooked begin");
        if (cookedAssetPath.empty()) {
            throw new ArgumentException("Cooked texture asset path must be provided.", "cookedAssetPath");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            AppendRenderTrace("RenderManager2D::BuildTextureFromCooked opened");
            asset = ::AssetSerializer::Deserialize(stream);
            AppendRenderTrace("RenderManager2D::BuildTextureFromCooked deserialized");
            delete stream;
            stream = nullptr;

            ::TextureAsset* cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);
            if (cookedTextureAsset == nullptr) {
                throw new InvalidOperationException("PS Vita cooked texture payloads must deserialize as TextureAsset.");
            }

            AppendRenderTrace("RenderManager2D::BuildTextureFromCooked typed");
            ::RuntimeTexture* runtimeTexture = BuildTextureFromRaw(cookedTextureAsset);
            delete cookedTextureAsset;
            asset = nullptr;
            AppendRenderTrace("RenderManager2D::BuildTextureFromCooked complete");
            return runtimeTexture;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }
            if (asset != nullptr) {
                delete asset;
            }

            throw;
        }
    }

    /// Builds a runtime texture from raw data.
    ::RuntimeTexture* PsVitaRenderManager2D::BuildTextureFromRaw(::TextureAsset* data) {
        AppendRenderTrace("RenderManager2D::BuildTextureFromRaw begin");
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Updates only live mutable textures, retaining CPU pixels for a later upload when needed.
    void PsVitaRenderManager2D::UpdateTextureRegionCore(::RuntimeTexture* texture, int32_t x, int32_t y,
        int32_t width, int32_t height, ::Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
        auto* nativeTexture = dynamic_cast<rendering::PsVitaRuntimeTexture*>(texture);
        if (nativeTexture == nullptr || rgba8 == nullptr || rgba8->Data == nullptr || rgba8->Length < 0
            || TextureCache.IsPendingRelease(nativeTexture)
            || (GxmRenderer != nullptr && GxmRenderer->IsShadowDepthPassActive())) {
            throw std::invalid_argument("PS Vita texture region update requires live native texture and source pixels.");
        }
        nativeTexture->UpdateRgbaRegion(x, y, width, height, rgba8->Data,
            static_cast<std::size_t>(rgba8->Length), sourceRowPitch,
            GxmRenderer != nullptr && GxmRenderer->IsMainFrameBegun());
    }

    /// Assigns the native PS Vita GXM renderer that will receive flushed quad batches.
    void PsVitaRenderManager2D::SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer) {
        GxmRenderer = gxmRenderer;
    }

    /// Draws one camera's ordered 2D queue into the queued-quad submission path.
    void PsVitaRenderManager2D::DrawCamera(::ICamera* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        ActiveCameraRenderOrder = camera->get_CameraDrawOrder();
        ::IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
        if (renderQueue == nullptr) {
            return;
        }

        renderQueue->VisitOrdered(this);
    }

    /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
    void PsVitaRenderManager2D::Draw() {
        if (QueuedSolidColorTriangles.empty() && QueuedQuads.empty()) {
            DrawRequestOrder.Reset();
            return;
        }

        std::stable_sort(QueuedSolidColorTriangles.begin(), QueuedSolidColorTriangles.end(), [](const rendering::PsVitaSolidColorVertex& left, const rendering::PsVitaSolidColorVertex& right) {
            return left.RenderOrder < right.RenderOrder;
        });
        std::stable_sort(QueuedQuads.begin(), QueuedQuads.end(), [](const rendering::PsVitaQueuedQuad& left, const rendering::PsVitaQueuedQuad& right) {
            if (left.RenderOrder != right.RenderOrder) {
                return left.RenderOrder < right.RenderOrder;
            }

            return false;
        });

        if (GxmRenderer != nullptr) {
            std::size_t solidColorIndex = 0u;
            std::size_t quadIndex = 0u;
            std::vector<rendering::PsVitaSolidColorVertex> solidColorLayer;
            std::vector<rendering::PsVitaQueuedQuad> quadLayer;
            while (solidColorIndex < QueuedSolidColorTriangles.size() || quadIndex < QueuedQuads.size()) {
                std::uint64_t renderOrder = solidColorIndex < QueuedSolidColorTriangles.size()
                    ? QueuedSolidColorTriangles[solidColorIndex].RenderOrder
                    : QueuedQuads[quadIndex].RenderOrder;
                if (quadIndex < QueuedQuads.size() && QueuedQuads[quadIndex].RenderOrder < renderOrder) {
                    renderOrder = QueuedQuads[quadIndex].RenderOrder;
                }
                solidColorLayer.clear();
                while (solidColorIndex < QueuedSolidColorTriangles.size()
                    && QueuedSolidColorTriangles[solidColorIndex].RenderOrder == renderOrder) {
                    solidColorLayer.push_back(QueuedSolidColorTriangles[solidColorIndex]);
                    ++solidColorIndex;
                }
                if (!solidColorLayer.empty()) {
                    GxmRenderer->SubmitSolidColorTriangles(solidColorLayer);
                }

                quadLayer.clear();
                while (quadIndex < QueuedQuads.size()
                    && QueuedQuads[quadIndex].RenderOrder == renderOrder) {
                    quadLayer.push_back(QueuedQuads[quadIndex]);
                    ++quadIndex;
                }
                if (!quadLayer.empty()) {
                    GxmRenderer->SubmitQuads(quadLayer);
                }
            }
        }

        QueuedSolidColorTriangles.clear();
        QueuedQuads.clear();
        DrawRequestOrder.Reset();
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
        if (texture != nullptr) {
            ReleaseTexture(texture);
        }

        ReleasedFonts.push_back(font);
    }

    /// Marks that one presented PS Vita frame completed so deferred texture and font destruction can advance only from the host post-present safe point.
    void PsVitaRenderManager2D::NotifyFramePresented() {
        PresentedFramePendingFlush = true;
        TextureCache.NotifyFramePresented();
    }

    /// Flushes deferred PS Vita runtime texture deletions once the renderer reaches a safe boundary before the next scene begins loading.
    void PsVitaRenderManager2D::FlushReleasedTextures() {
        if (!PresentedFramePendingFlush) {
            return;
        }

        PresentedFramePendingFlush = false;
        TextureCache.FlushReleasedTextures();
        while (!ReleasedFonts.empty()) {
            ::FontAsset* releasedFont = ReleasedFonts.back();
            ReleasedFonts.pop_back();
            if (releasedFont == nullptr) {
                continue;
            }

            releasedFont->Dispose();
            delete releasedFont;
        }
    }

    /// Draws a rounded rectangle request.
    void PsVitaRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            throw std::invalid_argument("PS Vita rounded-rectangle drawing requires one rounded-rectangle instance.");
        } else if (shape->get_Parent() == nullptr) {
            return;
        }

        ::int2 size = shape->get_Size();
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        float3 position = shape->get_Parent()->get_Position();
        std::uint32_t renderOrder = NextDrawRequestOrdinal();
        int32_t borderThickness = std::max<int32_t>(0, static_cast<int32_t>(RealMath::Round(shape->get_BorderThickness(), MidpointRounding::AwayFromZero)));
        RealEvaluation clampedRadius = RealMath::Max(
            RealEvaluation(0),
            RealMath::Min(
                shape->get_Radius(),
                RealEvaluation(RealMath::Min(size.X, size.Y)) / 2));

        if (borderThickness > 0) {
            AppendFilledRoundedRect(
                position,
                size,
                clampedRadius,
                PackColorAbgr(shape->get_BorderColor()),
                renderOrder);
        }

        ::int2 innerSize(size.X - (borderThickness * 2), size.Y - (borderThickness * 2));
        if (innerSize.X <= 0 || innerSize.Y <= 0) {
            if (borderThickness <= 0) {
                AppendFilledRoundedRect(
                    position,
                    size,
                    clampedRadius,
                    PackColorAbgr(shape->get_FillColor()),
                    renderOrder);
            }

            return;
        }

        float3 innerPosition(
            position.X + borderThickness,
            position.Y + borderThickness,
            position.Z);
        RealEvaluation innerRadius = RealMath::Max(RealEvaluation(0), clampedRadius - borderThickness);
        AppendFilledRoundedRect(
            innerPosition,
            innerSize,
            innerRadius,
            PackColorAbgr(shape->get_FillColor()),
            renderOrder);
    }

    /// Draws a sprite request.
    void PsVitaRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
        if (sprite == nullptr) {
            throw std::invalid_argument("PS Vita sprite drawing requires one sprite instance.");
        } else if (sprite->get_Parent() == nullptr) {
            return;
        }

        rendering::PsVitaRuntimeTexture* texture = dynamic_cast<rendering::PsVitaRuntimeTexture*>(sprite->get_Texture());
        if (texture == nullptr || (!texture->HasPixels() && !texture->HasNativePayload())) {
            return;
        }

        const SpriteQuad2D quad = Batch2DGeometry::ResolveSpriteQuad(sprite);
        float4 sourceRect = sprite->get_SourceRect();
        byte4 color = sprite->get_Color();
        std::uint32_t packedColor = (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);

        rendering::PsVitaQueuedQuad queuedQuad{};
        queuedQuad.Texture = texture;
        queuedQuad.RenderOrder = ComposeRenderOrder(ActiveCameraRenderOrder, NextDrawRequestOrdinal());
        queuedQuad.UsesLogicalTextureExtents = true;

        queuedQuad.Vertices[0].PositionX = static_cast<float>(quad.TopLeft.X);
        queuedQuad.Vertices[0].PositionY = static_cast<float>(quad.TopLeft.Y);
        queuedQuad.Vertices[0].TextureU = static_cast<float>(sourceRect.X);
        queuedQuad.Vertices[0].TextureV = static_cast<float>(sourceRect.Y);
        queuedQuad.Vertices[0].ColorAbgr = packedColor;

        queuedQuad.Vertices[1].PositionX = static_cast<float>(quad.TopRight.X);
        queuedQuad.Vertices[1].PositionY = static_cast<float>(quad.TopRight.Y);
        queuedQuad.Vertices[1].TextureU = static_cast<float>(sourceRect.X + sourceRect.Z);
        queuedQuad.Vertices[1].TextureV = static_cast<float>(sourceRect.Y);
        queuedQuad.Vertices[1].ColorAbgr = packedColor;

        queuedQuad.Vertices[2].PositionX = static_cast<float>(quad.BottomLeft.X);
        queuedQuad.Vertices[2].PositionY = static_cast<float>(quad.BottomLeft.Y);
        queuedQuad.Vertices[2].TextureU = static_cast<float>(sourceRect.X);
        queuedQuad.Vertices[2].TextureV = static_cast<float>(sourceRect.Y + sourceRect.W);
        queuedQuad.Vertices[2].ColorAbgr = packedColor;

        queuedQuad.Vertices[3].PositionX = static_cast<float>(quad.BottomRight.X);
        queuedQuad.Vertices[3].PositionY = static_cast<float>(quad.BottomRight.Y);
        queuedQuad.Vertices[3].TextureU = static_cast<float>(sourceRect.X + sourceRect.Z);
        queuedQuad.Vertices[3].TextureV = static_cast<float>(sourceRect.Y + sourceRect.W);
        queuedQuad.Vertices[3].ColorAbgr = packedColor;

        QueuedQuads.push_back(queuedQuad);
    }

    /// Draws a text request.
    void PsVitaRenderManager2D::DrawText(::ITextDrawable2D* text) {
        if (text == nullptr) {
            throw std::invalid_argument("PS Vita text drawing requires one text instance.");
        } else if (text->get_Parent() == nullptr || text->get_Font() == nullptr) {
            return;
        }

        ::FontAsset* font = text->get_Font();
        rendering::PsVitaRuntimeTexture* texture = dynamic_cast<rendering::PsVitaRuntimeTexture*>(font->get_Texture());
        if (texture == nullptr || (!texture->HasPixels() && !texture->HasNativePayload())) {
            return;
        }

        const std::uint32_t renderOrder = NextDrawRequestOrdinal();
        HeCppString content = text->get_Text();
        const RealEvaluation fontScale = 1;
        if (text->get_WrapText()) {
            int32_t maxWidth = 1;
            ::int2 textSizeValue = text->get_Size();
            if (textSizeValue.X > 0) {
                maxWidth = std::max(static_cast<int32_t>(1), static_cast<int32_t>(RealMath::Round(RealEvaluation(textSizeValue.X) / fontScale, MidpointRounding::AwayFromZero)));
            }

            content = TextLayoutUtils::WrapText(content, font, maxWidth);
        }

        std::vector<HeCppString> lines;
        HeCppString currentLine = "";
        for (char character : content) {
            if (character == '\r') {
                continue;
            }

            if (character == '\n') {
                lines.push_back(currentLine);
                currentLine.clear();
                continue;
            }

            currentLine.push_back(character);
        }
        lines.push_back(currentLine);

        std::vector<RealEvaluation> lineWidths;
        lineWidths.reserve(lines.size());
        for (const HeCppString& line : lines) {
            lineWidths.push_back(TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
                line,
                font,
                fontScale,
                font->get_AtlasWidth()));
        }

        float3 position = text->get_Parent()->get_Position();
        RealEvaluation baseX = position.X;
        RealEvaluation baseY = position.Y;
        RealEvaluation lineHeight = RealMath::Max(font->get_LineHeight() * fontScale, RealEvaluation(1));
        ::int2 layoutSizeValue = text->get_Size();
        std::vector<rendering::PsVitaTextEffectPass> effectPasses;
        effectPasses.reserve(6u);
        ::float2 shadowOffset = text->get_ShadowOffset();
        if (shadowOffset.X != 0.0f || shadowOffset.Y != 0.0f) {
            effectPasses.push_back(rendering::PsVitaTextEffectPass { shadowOffset, text->get_ShadowColor() });
        }

        Real outlineScale = text->get_OutlineScale();
        if (outlineScale > 0.0f) {
            ::byte4 outlineColor = text->get_OutlineColor();
            effectPasses.push_back(rendering::PsVitaTextEffectPass { ::float2(-outlineScale, 0.0f), outlineColor });
            effectPasses.push_back(rendering::PsVitaTextEffectPass { ::float2(outlineScale, 0.0f), outlineColor });
            effectPasses.push_back(rendering::PsVitaTextEffectPass { ::float2(0.0f, -outlineScale), outlineColor });
            effectPasses.push_back(rendering::PsVitaTextEffectPass { ::float2(0.0f, outlineScale), outlineColor });
        }

        effectPasses.push_back(rendering::PsVitaTextEffectPass { ::float2(0.0f, 0.0f), text->get_Color() });

        for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const HeCppString& line = lines[lineIndex];
            RealEvaluation offsetX = 0;
            RealEvaluation lineOffsetX = TextLayoutAlignmentUtils::ResolveHorizontalOffset(
                text->get_Alignment(),
                layoutSizeValue.X,
                lineWidths[lineIndex]);

            RealEvaluation lineOffsetY = RealEvaluation(lineIndex) * lineHeight;
            for (char character : line) {
                if (character == ' ') {
                    offsetX += font->get_FontInfo()->get_SpaceWidth() * fontScale;
                    continue;
                }

                FontChar glyph;
                if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                    continue;
                }

                RealEvaluation glyphWidth = RealMath::Max(RealEvaluation(1), RealMath::Round(glyph.SourceRect.Z * font->get_AtlasWidth() * fontScale, MidpointRounding::AwayFromZero));
                RealEvaluation glyphHeight = RealMath::Max(RealEvaluation(1), RealMath::Round(glyph.SourceRect.W * font->get_AtlasHeight() * fontScale, MidpointRounding::AwayFromZero));
                RealEvaluation glyphX = baseX + lineOffsetX + offsetX;
                RealEvaluation glyphY = baseY + lineOffsetY + (glyph.OffsetY * fontScale);

                for (const rendering::PsVitaTextEffectPass& effectPass : effectPasses) {
                    std::uint32_t packedColor = PackColorAbgr(effectPass.Color);
                    rendering::PsVitaQueuedQuad queuedQuad{};
                    queuedQuad.Texture = texture;
                    queuedQuad.RenderOrder = ComposeRenderOrder(ActiveCameraRenderOrder, renderOrder);
                    queuedQuad.UsesLogicalTextureExtents = true;

                    queuedQuad.Vertices[0].PositionX = static_cast<float>(glyphX + RealEvaluation(effectPass.Offset.X));
                    queuedQuad.Vertices[0].PositionY = static_cast<float>(glyphY + RealEvaluation(effectPass.Offset.Y));
                    queuedQuad.Vertices[0].TextureU = static_cast<float>(glyph.SourceRect.X);
                    queuedQuad.Vertices[0].TextureV = static_cast<float>(glyph.SourceRect.Y);
                    queuedQuad.Vertices[0].ColorAbgr = packedColor;

                    queuedQuad.Vertices[1].PositionX = static_cast<float>(glyphX + glyphWidth + RealEvaluation(effectPass.Offset.X));
                    queuedQuad.Vertices[1].PositionY = static_cast<float>(glyphY + RealEvaluation(effectPass.Offset.Y));
                    queuedQuad.Vertices[1].TextureU = static_cast<float>(glyph.SourceRect.X + glyph.SourceRect.Z);
                    queuedQuad.Vertices[1].TextureV = static_cast<float>(glyph.SourceRect.Y);
                    queuedQuad.Vertices[1].ColorAbgr = packedColor;

                    queuedQuad.Vertices[2].PositionX = static_cast<float>(glyphX + RealEvaluation(effectPass.Offset.X));
                    queuedQuad.Vertices[2].PositionY = static_cast<float>(glyphY + glyphHeight + RealEvaluation(effectPass.Offset.Y));
                    queuedQuad.Vertices[2].TextureU = static_cast<float>(glyph.SourceRect.X);
                    queuedQuad.Vertices[2].TextureV = static_cast<float>(glyph.SourceRect.Y + glyph.SourceRect.W);
                    queuedQuad.Vertices[2].ColorAbgr = packedColor;

                    queuedQuad.Vertices[3].PositionX = static_cast<float>(glyphX + glyphWidth + RealEvaluation(effectPass.Offset.X));
                    queuedQuad.Vertices[3].PositionY = static_cast<float>(glyphY + glyphHeight + RealEvaluation(effectPass.Offset.Y));
                    queuedQuad.Vertices[3].TextureU = static_cast<float>(glyph.SourceRect.X + glyph.SourceRect.Z);
                    queuedQuad.Vertices[3].TextureV = static_cast<float>(glyph.SourceRect.Y + glyph.SourceRect.W);
                    queuedQuad.Vertices[3].ColorAbgr = packedColor;

                    QueuedQuads.push_back(queuedQuad);
                }

                RealEvaluation advanceWidth = glyph.AdvanceWidth > 0.0f
                    ? glyph.AdvanceWidth * fontScale
                    : glyphWidth;
                offsetX += advanceWidth;
            }
        }
    }

    /// Visits one ordered drawable and forwards it through its generated-core draw entry point.
    void PsVitaRenderManager2D::Visit(::IDrawable2D* drawable) {
        if (drawable == nullptr) {
            return;
        }

        ::Entity* parent = drawable->get_Parent();
        if (parent == nullptr || !parent->get_IsHierarchyEnabled()) {
            return;
        }

        drawable->Draw();
    }

    /// Appends one queued solid-color triangle to the current frame submission list.
    void PsVitaRenderManager2D::AppendSolidTriangle(
        RealEvaluation x0,
        RealEvaluation y0,
        RealEvaluation x1,
        RealEvaluation y1,
        RealEvaluation x2,
        RealEvaluation y2,
        std::uint32_t colorAbgr,
        std::uint32_t renderOrder) {
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            static_cast<float>(x0),
            static_cast<float>(y0),
            colorAbgr,
            ComposeRenderOrder(ActiveCameraRenderOrder, renderOrder)
        });
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            static_cast<float>(x1),
            static_cast<float>(y1),
            colorAbgr,
            ComposeRenderOrder(ActiveCameraRenderOrder, renderOrder)
        });
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            static_cast<float>(x2),
            static_cast<float>(y2),
            colorAbgr,
            ComposeRenderOrder(ActiveCameraRenderOrder, renderOrder)
        });
    }

    /// Appends one queued solid-color quad as two triangles to the current frame submission list.
    void PsVitaRenderManager2D::AppendSolidQuad(
        RealEvaluation left,
        RealEvaluation top,
        RealEvaluation right,
        RealEvaluation bottom,
        std::uint32_t colorAbgr,
        std::uint32_t renderOrder) {
        if (right <= left || bottom <= top) {
            return;
        }

        AppendSolidTriangle(left, top, right, top, left, bottom, colorAbgr, renderOrder);
        AppendSolidTriangle(left, bottom, right, top, right, bottom, colorAbgr, renderOrder);
    }

    /// Appends one filled rounded rectangle using solid-color triangles for the current menu pass.
    void PsVitaRenderManager2D::AppendFilledRoundedRect(
        const float3& position,
        const ::int2& size,
        RealEvaluation radius,
        std::uint32_t colorAbgr,
        std::uint32_t renderOrder) {
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        RealEvaluation left = position.X;
        RealEvaluation top = position.Y;
        RealEvaluation right = position.X + size.X;
        RealEvaluation bottom = position.Y + size.Y;
        RealEvaluation clampedRadius = RealMath::Max(
            RealEvaluation(0),
            RealMath::Min(
                radius,
                RealEvaluation(RealMath::Min(size.X, size.Y)) / 2));
        if (clampedRadius <= 0.0) {
            AppendSolidQuad(left, top, right, bottom, colorAbgr, renderOrder);
            return;
        }

        RealEvaluation radiusPixels = clampedRadius;
        AppendSolidQuad(left + radiusPixels, top, right - radiusPixels, bottom, colorAbgr, renderOrder);
        AppendSolidQuad(left, top + radiusPixels, left + radiusPixels, bottom - radiusPixels, colorAbgr, renderOrder);
        AppendSolidQuad(right - radiusPixels, top + radiusPixels, right, bottom - radiusPixels, colorAbgr, renderOrder);

        AppendRoundedCornerFan(
            left + radiusPixels,
            top + radiusPixels,
            clampedRadius,
            3.14159265358979323846,
            4.71238898038468985769,
            colorAbgr,
            renderOrder);
        AppendRoundedCornerFan(
            right - radiusPixels,
            top + radiusPixels,
            clampedRadius,
            4.71238898038468985769,
            6.28318530717958647692,
            colorAbgr,
            renderOrder);
        AppendRoundedCornerFan(
            right - radiusPixels,
            bottom - radiusPixels,
            clampedRadius,
            0.0,
            1.57079632679489661923,
            colorAbgr,
            renderOrder);
        AppendRoundedCornerFan(
            left + radiusPixels,
            bottom - radiusPixels,
            clampedRadius,
            1.57079632679489661923,
            3.14159265358979323846,
            colorAbgr,
            renderOrder);
    }

    /// Appends one rounded-corner triangle fan using the supplied angle range.
    void PsVitaRenderManager2D::AppendRoundedCornerFan(
        RealEvaluation centerX,
        RealEvaluation centerY,
        RealEvaluation radius,
        RealEvaluation startAngleRadians,
        RealEvaluation endAngleRadians,
        std::uint32_t colorAbgr,
        std::uint32_t renderOrder) {
        if (radius <= 0.0) {
            return;
        }

        RealEvaluation angleStep = (endAngleRadians - startAngleRadians) / RoundedCornerSegmentCount;
        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; ++segmentIndex) {
            RealEvaluation angle0 = startAngleRadians + (angleStep * segmentIndex);
            RealEvaluation angle1 = startAngleRadians + (angleStep * (segmentIndex + 1));
            AppendSolidTriangle(
                centerX,
                centerY,
                centerX + (RealMath::Cos(angle0) * radius),
                centerY + (RealMath::Sin(angle0) * radius),
                centerX + (RealMath::Cos(angle1) * radius),
                centerY + (RealMath::Sin(angle1) * radius),
                colorAbgr,
                renderOrder);
        }
    }

    /// Combines one camera draw order and one local drawable order into a stable global queue key.
    std::uint64_t PsVitaRenderManager2D::ComposeRenderOrder(std::uint16_t cameraRenderOrder, std::uint32_t renderOrder) {
        return rendering::ComposePsVitaDrawOrder(cameraRenderOrder, renderOrder);
    }

    std::uint32_t PsVitaRenderManager2D::NextDrawRequestOrdinal() {
        return DrawRequestOrder.Next();
    }

    /// Packs one engine byte color into the ABGR layout expected by Vita2D.
    std::uint32_t PsVitaRenderManager2D::PackColorAbgr(const byte4& color) {
        return (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);
    }

}

#endif
