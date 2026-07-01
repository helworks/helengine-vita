#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

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
#include "TextureAsset.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    namespace {
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";
        constexpr int RoundedCornerSegmentCount = 8;

        void AppendRenderTrace(const char* message) {
            if (message == nullptr) {
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
    ::RuntimeTexture* PsVitaRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath) {
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

    /// Assigns the native PS Vita GXM renderer that will receive flushed quad batches.
    void PsVitaRenderManager2D::SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer) {
        GxmRenderer = gxmRenderer;
    }

    /// Draws one camera's ordered 2D queue into the queued-quad submission path.
    void PsVitaRenderManager2D::DrawCamera(::ICamera* camera) {
        if (camera == nullptr) {
            throw new ArgumentNullException("camera");
        }

        ::IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
        if (renderQueue == nullptr) {
            return;
        }

        renderQueue->VisitOrdered(this);
    }

    /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
    void PsVitaRenderManager2D::Draw() {
        if (QueuedSolidColorTriangles.empty() && QueuedQuads.empty()) {
            return;
        }

        std::stable_sort(QueuedSolidColorTriangles.begin(), QueuedSolidColorTriangles.end(), [](const rendering::PsVitaSolidColorVertex& left, const rendering::PsVitaSolidColorVertex& right) {
            return left.RenderOrder < right.RenderOrder;
        });
        std::sort(QueuedQuads.begin(), QueuedQuads.end(), [](const rendering::PsVitaQueuedQuad& left, const rendering::PsVitaQueuedQuad& right) {
            if (left.RenderOrder != right.RenderOrder) {
                return left.RenderOrder < right.RenderOrder;
            }

            return left.Texture < right.Texture;
        });

        if (GxmRenderer != nullptr) {
            GxmRenderer->SubmitSolidColorTriangles(QueuedSolidColorTriangles);
            GxmRenderer->SubmitQuads(QueuedQuads);
        }

        QueuedSolidColorTriangles.clear();
        QueuedQuads.clear();
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
        std::uint8_t renderOrder = shape->get_RenderOrder2D();
        int32_t borderThickness = std::max<int32_t>(0, static_cast<int32_t>(std::lround(shape->get_BorderThickness())));
        double clampedRadius = std::max(
            0.0,
            std::min(
                static_cast<double>(shape->get_Radius()),
                std::min(static_cast<double>(size.X), static_cast<double>(size.Y)) * 0.5));

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
            position.X + static_cast<float>(borderThickness),
            position.Y + static_cast<float>(borderThickness),
            position.Z);
        double innerRadius = std::max(0.0, clampedRadius - static_cast<double>(borderThickness));
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
        if (texture == nullptr || !texture->HasPixels()) {
            return;
        }

        ::int2 spriteSizeValue = sprite->get_Size();
        const int32_t spriteWidthPixels = spriteSizeValue.X;
        const int32_t spriteHeightPixels = spriteSizeValue.Y;
        double spriteWidth = spriteWidthPixels > 0
            ? static_cast<double>(spriteWidthPixels)
            : static_cast<double>(texture->get_Width());
        double spriteHeight = spriteHeightPixels > 0
            ? static_cast<double>(spriteHeightPixels)
            : static_cast<double>(texture->get_Height());
        if (spriteWidth <= 0.0 || spriteHeight <= 0.0) {
            return;
        }

        ::Entity* parent = sprite->get_Parent();
        float3 position = parent->get_Position();
        float3 scale = parent->get_Scale();
        float4 sourceRect = sprite->get_SourceRect();
        byte4 color = sprite->get_Color();
        spriteWidth *= static_cast<double>(scale.X);
        spriteHeight *= static_cast<double>(scale.Y);
        float3 rotatedRight = float4::RotateVector(float3::get_UnitX(), parent->get_Orientation());
        double rotation = std::atan2(
            static_cast<double>(rotatedRight.Y),
            static_cast<double>(rotatedRight.X));
        double halfWidth = spriteWidth * 0.5;
        double halfHeight = spriteHeight * 0.5;
        double centerX = static_cast<double>(position.X) + halfWidth;
        double centerY = static_cast<double>(position.Y) + halfHeight;
        double sine = std::sin(rotation);
        double cosine = std::cos(rotation);
        std::uint32_t packedColor = (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);

        rendering::PsVitaQueuedQuad queuedQuad{};
        queuedQuad.Texture = texture;
        queuedQuad.RenderOrder = sprite->get_RenderOrder2D();

        double topLeftOffsetX = -halfWidth;
        double topLeftOffsetY = -halfHeight;
        double topRightOffsetX = halfWidth;
        double topRightOffsetY = -halfHeight;
        double bottomLeftOffsetX = -halfWidth;
        double bottomLeftOffsetY = halfHeight;
        double bottomRightOffsetX = halfWidth;
        double bottomRightOffsetY = halfHeight;

        queuedQuad.Vertices[0].PositionX = static_cast<float>(centerX + (topLeftOffsetX * cosine) - (topLeftOffsetY * sine));
        queuedQuad.Vertices[0].PositionY = static_cast<float>(centerY + (topLeftOffsetX * sine) + (topLeftOffsetY * cosine));
        queuedQuad.Vertices[0].TextureU = sourceRect.X;
        queuedQuad.Vertices[0].TextureV = sourceRect.Y;
        queuedQuad.Vertices[0].ColorAbgr = packedColor;

        queuedQuad.Vertices[1].PositionX = static_cast<float>(centerX + (topRightOffsetX * cosine) - (topRightOffsetY * sine));
        queuedQuad.Vertices[1].PositionY = static_cast<float>(centerY + (topRightOffsetX * sine) + (topRightOffsetY * cosine));
        queuedQuad.Vertices[1].TextureU = sourceRect.X + sourceRect.Z;
        queuedQuad.Vertices[1].TextureV = sourceRect.Y;
        queuedQuad.Vertices[1].ColorAbgr = packedColor;

        queuedQuad.Vertices[2].PositionX = static_cast<float>(centerX + (bottomLeftOffsetX * cosine) - (bottomLeftOffsetY * sine));
        queuedQuad.Vertices[2].PositionY = static_cast<float>(centerY + (bottomLeftOffsetX * sine) + (bottomLeftOffsetY * cosine));
        queuedQuad.Vertices[2].TextureU = sourceRect.X;
        queuedQuad.Vertices[2].TextureV = sourceRect.Y + sourceRect.W;
        queuedQuad.Vertices[2].ColorAbgr = packedColor;

        queuedQuad.Vertices[3].PositionX = static_cast<float>(centerX + (bottomRightOffsetX * cosine) - (bottomRightOffsetY * sine));
        queuedQuad.Vertices[3].PositionY = static_cast<float>(centerY + (bottomRightOffsetX * sine) + (bottomRightOffsetY * cosine));
        queuedQuad.Vertices[3].TextureU = sourceRect.X + sourceRect.Z;
        queuedQuad.Vertices[3].TextureV = sourceRect.Y + sourceRect.W;
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
        if (texture == nullptr || !texture->HasPixels()) {
            return;
        }

        std::string content = text->get_Text();
        const double fontScale = 1.0;
        if (text->get_WrapText()) {
            int32_t maxWidth = 1;
            ::int2 textSizeValue = text->get_Size();
            if (textSizeValue.X > 0) {
                maxWidth = std::max(static_cast<int32_t>(1), static_cast<int32_t>(std::round(static_cast<double>(textSizeValue.X) / fontScale)));
            }

            content = TextLayoutUtils::WrapText(content, font, maxWidth);
        }

        std::vector<std::string> lines;
        std::string currentLine;
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

        std::vector<double> lineWidths;
        lineWidths.reserve(lines.size());
        for (const std::string& line : lines) {
            lineWidths.push_back(TextLayoutAlignmentUtils::MeasureVisibleLineWidth(
                line,
                font,
                fontScale,
                static_cast<double>(font->get_AtlasWidth())));
        }

        float3 position = text->get_Parent()->get_Position();
        double baseX = static_cast<double>(position.X);
        double baseY = static_cast<double>(position.Y);
        double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0);
        ::int2 layoutSizeValue = text->get_Size();
        byte4 color = text->get_Color();
        std::uint32_t packedColor = (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);

        for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const std::string& line = lines[lineIndex];
            double offsetX = 0.0;
            double lineOffsetX = TextLayoutAlignmentUtils::ResolveHorizontalOffset(
                text->get_Alignment(),
                layoutSizeValue.X,
                lineWidths[lineIndex]);

            double lineOffsetY = static_cast<double>(lineIndex) * lineHeight;
            for (char character : line) {
                if (character == ' ') {
                    offsetX += static_cast<double>(font->get_FontInfo()->get_SpaceWidth()) * fontScale;
                    continue;
                }

                FontChar glyph;
                if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                    continue;
                }

                double glyphWidth = std::max(1.0, std::round(static_cast<double>(glyph.SourceRect.Z) * static_cast<double>(font->get_AtlasWidth()) * fontScale));
                double glyphHeight = std::max(1.0, std::round(static_cast<double>(glyph.SourceRect.W) * static_cast<double>(font->get_AtlasHeight()) * fontScale));
                double glyphX = baseX + lineOffsetX + offsetX;
                double glyphY = baseY + lineOffsetY + (static_cast<double>(glyph.OffsetY) * fontScale);

                rendering::PsVitaQueuedQuad queuedQuad{};
                queuedQuad.Texture = texture;
                queuedQuad.RenderOrder = text->get_RenderOrder2D();

                queuedQuad.Vertices[0].PositionX = static_cast<float>(glyphX);
                queuedQuad.Vertices[0].PositionY = static_cast<float>(glyphY);
                queuedQuad.Vertices[0].TextureU = glyph.SourceRect.X;
                queuedQuad.Vertices[0].TextureV = glyph.SourceRect.Y;
                queuedQuad.Vertices[0].ColorAbgr = packedColor;

                queuedQuad.Vertices[1].PositionX = static_cast<float>(glyphX + glyphWidth);
                queuedQuad.Vertices[1].PositionY = static_cast<float>(glyphY);
                queuedQuad.Vertices[1].TextureU = glyph.SourceRect.X + glyph.SourceRect.Z;
                queuedQuad.Vertices[1].TextureV = glyph.SourceRect.Y;
                queuedQuad.Vertices[1].ColorAbgr = packedColor;

                queuedQuad.Vertices[2].PositionX = static_cast<float>(glyphX);
                queuedQuad.Vertices[2].PositionY = static_cast<float>(glyphY + glyphHeight);
                queuedQuad.Vertices[2].TextureU = glyph.SourceRect.X;
                queuedQuad.Vertices[2].TextureV = glyph.SourceRect.Y + glyph.SourceRect.W;
                queuedQuad.Vertices[2].ColorAbgr = packedColor;

                queuedQuad.Vertices[3].PositionX = static_cast<float>(glyphX + glyphWidth);
                queuedQuad.Vertices[3].PositionY = static_cast<float>(glyphY + glyphHeight);
                queuedQuad.Vertices[3].TextureU = glyph.SourceRect.X + glyph.SourceRect.Z;
                queuedQuad.Vertices[3].TextureV = glyph.SourceRect.Y + glyph.SourceRect.W;
                queuedQuad.Vertices[3].ColorAbgr = packedColor;

                QueuedQuads.push_back(queuedQuad);

                double advanceWidth = glyph.AdvanceWidth > 0.0f
                    ? static_cast<double>(glyph.AdvanceWidth) * fontScale
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
        if (parent == nullptr || !parent->get_Enabled()) {
            return;
        }

        drawable->Draw();
    }

    /// Appends one queued solid-color triangle to the current frame submission list.
    void PsVitaRenderManager2D::AppendSolidTriangle(
        float x0,
        float y0,
        float x1,
        float y1,
        float x2,
        float y2,
        std::uint32_t colorAbgr,
        std::uint8_t renderOrder) {
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            x0,
            y0,
            colorAbgr,
            renderOrder
        });
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            x1,
            y1,
            colorAbgr,
            renderOrder
        });
        QueuedSolidColorTriangles.push_back(rendering::PsVitaSolidColorVertex {
            x2,
            y2,
            colorAbgr,
            renderOrder
        });
    }

    /// Appends one queued solid-color quad as two triangles to the current frame submission list.
    void PsVitaRenderManager2D::AppendSolidQuad(
        float left,
        float top,
        float right,
        float bottom,
        std::uint32_t colorAbgr,
        std::uint8_t renderOrder) {
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
        double radius,
        std::uint32_t colorAbgr,
        std::uint8_t renderOrder) {
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        float left = position.X;
        float top = position.Y;
        float right = position.X + static_cast<float>(size.X);
        float bottom = position.Y + static_cast<float>(size.Y);
        double clampedRadius = std::max(
            0.0,
            std::min(
                radius,
                std::min(static_cast<double>(size.X), static_cast<double>(size.Y)) * 0.5));
        if (clampedRadius <= 0.0) {
            AppendSolidQuad(left, top, right, bottom, colorAbgr, renderOrder);
            return;
        }

        float radiusPixels = static_cast<float>(clampedRadius);
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
        float centerX,
        float centerY,
        double radius,
        double startAngleRadians,
        double endAngleRadians,
        std::uint32_t colorAbgr,
        std::uint8_t renderOrder) {
        if (radius <= 0.0) {
            return;
        }

        double angleStep = (endAngleRadians - startAngleRadians) / static_cast<double>(RoundedCornerSegmentCount);
        for (int segmentIndex = 0; segmentIndex < RoundedCornerSegmentCount; ++segmentIndex) {
            double angle0 = startAngleRadians + (angleStep * static_cast<double>(segmentIndex));
            double angle1 = startAngleRadians + (angleStep * static_cast<double>(segmentIndex + 1));
            AppendSolidTriangle(
                centerX,
                centerY,
                static_cast<float>(centerX + (std::cos(angle0) * radius)),
                static_cast<float>(centerY + (std::sin(angle0) * radius)),
                static_cast<float>(centerX + (std::cos(angle1) * radius)),
                static_cast<float>(centerY + (std::sin(angle1) * radius)),
                colorAbgr,
                renderOrder);
        }
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
