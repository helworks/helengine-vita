#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

#include "Entity.hpp"
#include "FontChar.hpp"
#include "FontAsset.hpp"
#include "FontInfo.hpp"
#include "TextAlignment.hpp"
#include "TextLayoutUtils.hpp"
#include "byte4.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "int2.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita {
    /// Builds a runtime texture from raw data.
    ::RuntimeTexture* PsVitaRenderManager2D::BuildTextureFromRaw(::TextureAsset* data) {
        return TextureCache.BuildTextureFromRaw(data);
    }

    /// Assigns the native PS Vita GXM renderer that will receive flushed quad batches.
    void PsVitaRenderManager2D::SetGxmRenderer(rendering::PsVitaGxmRenderer* gxmRenderer) {
        GxmRenderer = gxmRenderer;
    }

    /// Flushes one frame of queued PS Vita 2D commands through the native renderer foundation.
    void PsVitaRenderManager2D::Draw() {
        if (QueuedQuads.empty()) {
            return;
        }

        std::sort(QueuedQuads.begin(), QueuedQuads.end(), [](const rendering::PsVitaQueuedQuad& left, const rendering::PsVitaQueuedQuad& right) {
            if (left.RenderOrder != right.RenderOrder) {
                return left.RenderOrder < right.RenderOrder;
            }

            return left.Texture < right.Texture;
        });

        if (GxmRenderer != nullptr) {
            GxmRenderer->SubmitQuads(QueuedQuads);
        }

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
        if (sprite == nullptr) {
            throw std::invalid_argument("PS Vita sprite drawing requires one sprite instance.");
        } else if (sprite->get_Parent() == nullptr) {
            return;
        }

        rendering::PsVitaRuntimeTexture* texture = dynamic_cast<rendering::PsVitaRuntimeTexture*>(sprite->get_Texture());
        if (texture == nullptr || !texture->HasPixels()) {
            return;
        }

        int2 spriteSize = sprite->get_Size();
        double spriteWidth = spriteSize.X > 0
            ? static_cast<double>(spriteSize.X)
            : static_cast<double>(texture->get_Width());
        double spriteHeight = spriteSize.Y > 0
            ? static_cast<double>(spriteSize.Y)
            : static_cast<double>(texture->get_Height());
        if (spriteWidth <= 0.0 || spriteHeight <= 0.0) {
            return;
        }

        float3 position = sprite->get_Parent()->get_Position();
        float4 sourceRect = sprite->get_SourceRect();
        byte4 color = sprite->get_Color();
        double rotation = static_cast<double>(sprite->get_Rotation());
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
        double fontScale = std::max(static_cast<double>(text->get_FontScale()), 0.0001);
        if (text->get_WrapText()) {
            int32_t maxWidth = 1;
            int2 textSize = text->get_Size();
            if (textSize.X > 0) {
                maxWidth = std::max(static_cast<int32_t>(1), static_cast<int32_t>(std::round(static_cast<double>(textSize.X) / fontScale)));
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
            double lineWidth = 0.0;
            for (char character : line) {
                if (character == ' ') {
                    lineWidth += static_cast<double>(font->get_FontInfo()->get_SpaceWidth()) * fontScale;
                    continue;
                }

                FontChar glyph;
                if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                    continue;
                }

                double glyphWidth = std::max(1.0, std::round(static_cast<double>(glyph.SourceRect.Z) * static_cast<double>(font->get_AtlasWidth()) * fontScale));
                double advanceWidth = glyph.AdvanceWidth > 0.0f
                    ? static_cast<double>(glyph.AdvanceWidth) * fontScale
                    : glyphWidth;
                lineWidth += advanceWidth;
            }

            lineWidths.push_back(lineWidth);
        }

        float3 position = text->get_Parent()->get_Position();
        double baseX = static_cast<double>(position.X);
        double baseY = static_cast<double>(position.Y);
        double lineHeight = std::max(static_cast<double>(font->get_LineHeight()) * fontScale, 1.0);
        int2 layoutSize = text->get_Size();
        double layoutWidth = layoutSize.X > 0
            ? static_cast<double>(layoutSize.X)
            : 0.0;
        byte4 color = text->get_Color();
        std::uint32_t packedColor = (static_cast<std::uint32_t>(color.W) << 24)
            | (static_cast<std::uint32_t>(color.Z) << 16)
            | (static_cast<std::uint32_t>(color.Y) << 8)
            | static_cast<std::uint32_t>(color.X);

        for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            const std::string& line = lines[lineIndex];
            double offsetX = 0.0;
            double lineOffsetX = 0.0;
            if (layoutWidth > 0.0) {
                switch (text->get_Alignment()) {
                case TextAlignment::Center:
                    lineOffsetX = std::max(0.0, (layoutWidth - lineWidths[lineIndex]) * 0.5);
                    break;
                case TextAlignment::Right:
                    lineOffsetX = std::max(0.0, layoutWidth - lineWidths[lineIndex]);
                    break;
                default:
                    break;
                }
            }

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
}

#endif
