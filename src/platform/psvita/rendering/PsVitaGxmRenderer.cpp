#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>

#include <vita2d.h>

#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    /// Creates one uninitialized PS Vita GXM renderer foundation.
    PsVitaGxmRenderer::PsVitaGxmRenderer()
        : Initialized(false)
        , FrameBegun(false)
        , ActiveClearColorAbgr(0u)
        , SubmittedQuadCount(0u) {
    }

    /// Initializes the native renderer state needed before the first submitted 2D frame.
    bool PsVitaGxmRenderer::Initialize() {
        if (Initialized) {
            return true;
        }

        if (vita2d_init() < 0) {
            return false;
        }

        Initialized = true;
        FrameBegun = false;
        SubmittedQuadCount = 0u;
        return true;
    }

    /// Shuts down the native renderer state and releases any owned frame resources.
    void PsVitaGxmRenderer::Shutdown() {
        if (!Initialized) {
            return;
        }

        if (FrameBegun) {
            vita2d_end_drawing();
            FrameBegun = false;
        }

        vita2d_wait_rendering_done();
        vita2d_fini();
        Initialized = false;
        FrameBegun = false;
        SubmittedQuadCount = 0u;
        ActiveClearColorAbgr = 0u;
    }

    /// Gets whether the renderer has completed its initialization path.
    bool PsVitaGxmRenderer::IsInitialized() const {
        return Initialized;
    }

    /// Begins one new frame and records the requested clear color for later native submission.
    void PsVitaGxmRenderer::BeginFrame(std::uint32_t clearColorAbgr) {
        if (!Initialized) {
            throw std::runtime_error("PS Vita GPU renderer must be initialized before beginning a frame.");
        }

        ActiveClearColorAbgr = clearColorAbgr;
        SubmittedQuadCount = 0u;
        vita2d_set_clear_color(ActiveClearColorAbgr);
        vita2d_start_drawing();
        vita2d_clear_screen();
        FrameBegun = true;
    }

    /// Records one batch of textured quads for later native submission.
    void PsVitaGxmRenderer::SubmitQuads(const std::vector<PsVitaQueuedQuad>& queuedQuads) {
        if (!Initialized || !FrameBegun) {
            return;
        }

        for (const PsVitaQueuedQuad& queuedQuad : queuedQuads) {
            SubmitQuad(queuedQuad);
        }

        SubmittedQuadCount = queuedQuads.size();
    }

    /// Presents the current frame through the PS Vita display path.
    void PsVitaGxmRenderer::PresentFrame() {
        if (!Initialized || !FrameBegun) {
            return;
        }

        vita2d_end_drawing();
        vita2d_swap_buffers();
        FrameBegun = false;
    }

    /// Gets the number of quads most recently recorded for the current frame.
    std::size_t PsVitaGxmRenderer::GetSubmittedQuadCount() const {
        return SubmittedQuadCount;
    }

    /// Lazily uploads one runtime texture into a native PS Vita texture allocation before the first draw that references it.
    void PsVitaGxmRenderer::EnsureUploaded(PsVitaRuntimeTexture* runtimeTexture) {
        if (runtimeTexture == nullptr) {
            throw std::invalid_argument("PS Vita GPU upload requires one runtime texture.");
        }

        if (runtimeTexture->HasGpuTexture()) {
            return;
        }

        std::uint32_t textureWidth = runtimeTexture->GetTextureWidthPixels();
        std::uint32_t textureHeight = runtimeTexture->GetTextureHeightPixels();
        if (textureWidth == 0u || textureHeight == 0u || !runtimeTexture->HasPixels()) {
            throw std::runtime_error("PS Vita GPU upload requires non-empty runtime texture pixels and dimensions.");
        }

        vita2d_texture* nativeTexture = vita2d_create_empty_texture_format(textureWidth, textureHeight, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
        if (nativeTexture == nullptr) {
            throw std::runtime_error("PS Vita GPU upload failed to allocate one native texture.");
        }

        std::uint32_t* nativePixels = static_cast<std::uint32_t*>(vita2d_texture_get_datap(nativeTexture));
        if (nativePixels == nullptr) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita GPU upload failed to acquire the native texture pixel buffer.");
        }

        std::size_t stridePixels = static_cast<std::size_t>(vita2d_texture_get_stride(nativeTexture)) / sizeof(std::uint32_t);
        if (stridePixels < textureWidth) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita GPU upload received one native texture stride smaller than the authored width.");
        }

        const std::uint32_t* sourcePixels = runtimeTexture->GetPixelsAbgr8888();
        for (std::uint32_t rowIndex = 0u; rowIndex < textureHeight; ++rowIndex) {
            std::uint32_t* destinationRow = nativePixels + (static_cast<std::size_t>(rowIndex) * stridePixels);
            const std::uint32_t* sourceRow = sourcePixels + (static_cast<std::size_t>(rowIndex) * textureWidth);
            std::memcpy(destinationRow, sourceRow, static_cast<std::size_t>(textureWidth) * sizeof(std::uint32_t));
        }

        vita2d_texture_set_filters(nativeTexture, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

        std::unique_ptr<PsVitaGpuTexture> gpuTexture = std::make_unique<PsVitaGpuTexture>();
        gpuTexture->SetNativeTexture(nativeTexture, textureWidth, textureHeight);
        runtimeTexture->SetGpuTexture(std::move(gpuTexture));
    }

    /// Submits one queued quad through the GPU-backed textured-triangle path.
    void PsVitaGxmRenderer::SubmitQuad(const PsVitaQueuedQuad& queuedQuad) {
        if (queuedQuad.Texture == nullptr) {
            return;
        }

        EnsureUploaded(queuedQuad.Texture);
        PsVitaGpuTexture* gpuTexture = queuedQuad.Texture->GetGpuTexture();
        if (gpuTexture == nullptr || !gpuTexture->IsUploaded() || gpuTexture->GetNativeTexture() == nullptr) {
            return;
        }

        unsigned int colorAbgr = queuedQuad.Vertices[0].ColorAbgr;

        vita2d_texture_vertex* triangleVertices = static_cast<vita2d_texture_vertex*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(vita2d_texture_vertex) * 6u),
            8u));
        if (triangleVertices == nullptr) {
            throw std::runtime_error("PS Vita quad submission failed to allocate transient GPU-visible vertex memory.");
        }

        std::memset(triangleVertices, 0, sizeof(vita2d_texture_vertex) * 6u);
        triangleVertices[0].x = queuedQuad.Vertices[0].PositionX;
        triangleVertices[0].y = queuedQuad.Vertices[0].PositionY;
        triangleVertices[0].z = 0.5f;
        triangleVertices[0].u = queuedQuad.Vertices[0].TextureU;
        triangleVertices[0].v = queuedQuad.Vertices[0].TextureV;

        triangleVertices[1].x = queuedQuad.Vertices[1].PositionX;
        triangleVertices[1].y = queuedQuad.Vertices[1].PositionY;
        triangleVertices[1].z = 0.5f;
        triangleVertices[1].u = queuedQuad.Vertices[1].TextureU;
        triangleVertices[1].v = queuedQuad.Vertices[1].TextureV;

        triangleVertices[2].x = queuedQuad.Vertices[2].PositionX;
        triangleVertices[2].y = queuedQuad.Vertices[2].PositionY;
        triangleVertices[2].z = 0.5f;
        triangleVertices[2].u = queuedQuad.Vertices[2].TextureU;
        triangleVertices[2].v = queuedQuad.Vertices[2].TextureV;

        triangleVertices[3] = triangleVertices[2];

        triangleVertices[4].x = queuedQuad.Vertices[1].PositionX;
        triangleVertices[4].y = queuedQuad.Vertices[1].PositionY;
        triangleVertices[4].z = 0.5f;
        triangleVertices[4].u = queuedQuad.Vertices[1].TextureU;
        triangleVertices[4].v = queuedQuad.Vertices[1].TextureV;

        triangleVertices[5].x = queuedQuad.Vertices[3].PositionX;
        triangleVertices[5].y = queuedQuad.Vertices[3].PositionY;
        triangleVertices[5].z = 0.5f;
        triangleVertices[5].u = queuedQuad.Vertices[3].TextureU;
        triangleVertices[5].v = queuedQuad.Vertices[3].TextureV;

        vita2d_draw_array_textured(
            gpuTexture->GetNativeTexture(),
            SCE_GXM_PRIMITIVE_TRIANGLES,
            triangleVertices,
            6u,
            colorAbgr);
    }
}

#endif
