#include "platform/psvita/rendering/PsVitaGxmRenderer.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <psp2/gxm.h>
#include <vita2d.h>

#include "platform/psvita/rendering/PsVitaGpuTexture.hpp"
#include "platform/psvita/rendering/PsVitaRuntimeTexture.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

namespace helengine::psvita::rendering {
    namespace {
        /// Keeps the legacy solid-color compatibility path disabled while the artifact-backed path is selected explicitly.
        constexpr bool EnableRuntimeCompiledSolidColorProgram = false;
        constexpr bool EnablePsVitaBootTraceLogging = false;
        constexpr const char* BootTracePath = "ux0:/data/helengine_psvita_boot.log";
        /// Minimum square dimension accepted by the GXM linear texture binding used for the Standard Shader fallback.
        constexpr std::uint32_t StandardFallbackTextureDimension = 8u;

        /// Appends one PS Vita renderer diagnostics line to the persisted boot trace.
        void AppendRendererTrace(const std::string& message) {
            if (!EnablePsVitaBootTraceLogging) {
                return;
            }

            std::FILE* file = std::fopen(BootTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::fputs(message.c_str(), file);
            std::fputc('\n', file);
            std::fclose(file);
        }
    }

    /// Creates one uninitialized PS Vita GXM renderer foundation.
    PsVitaGxmRenderer::PsVitaGxmRenderer()
        : Initialized(false)
        , FrameBegun(false)
        , ActiveClearColorAbgr(0u)
        , SubmittedQuadCount(0u)
        , SolidColorProgram()
        , ForwardLambertProgram()
        , ForwardStandardProgram()
        , ShadowDepthProgram()
        , ShadowMap()
        , ShadowSubmissionMemory()
        , ShaderBundle()
        , ShaderBundleLoaded(false)
        , ForwardLambertProgramKey()
        , ForwardStandardProgramKey()
        , ShadowDepthProgramKey()
        , StandardWhiteTexture(nullptr) {
    }

    /// Initializes the native renderer state needed before the first submitted 2D frame.
    bool PsVitaGxmRenderer::Initialize() {
        if (Initialized) {
            return true;
        }

        if (vita2d_init() < 0) {
            return false;
        }

        if (EnableRuntimeCompiledSolidColorProgram && !SolidColorProgram.Initialize()) {
            vita2d_fini();
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
        ReleaseCompletedShadowSubmissionMemory();
        if (StandardWhiteTexture != nullptr) {
            vita2d_free_texture(StandardWhiteTexture);
            StandardWhiteTexture = nullptr;
        }
        vita2d_fini();
        SolidColorProgram.Reset();
        ForwardLambertProgram.Reset();
        ForwardStandardProgram.Reset();
        ShadowDepthProgram.Reset();
        ShadowMap.Reset();
        ShaderBundleLoaded = false;
        ForwardLambertProgramKey.clear();
        ForwardStandardProgramKey.clear();
        ShadowDepthProgramKey.clear();
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

    /// Begins the Vita2D-owned offscreen shadow depth pass before the main frame is opened.
    bool PsVitaGxmRenderer::BeginShadowDepthPass() {
        if (!Initialized || FrameBegun || ShadowMap.IsDepthPassActive()) {
            return false;
        }
        ReleaseCompletedShadowSubmissionMemory();
        if (!ShadowMap.Initialize()) {
            return false;
        }

        vita2d_set_clear_color(0xFFFFFFFFu);
        ShadowMap.BeginDepthPass();
        return true;
    }

    /// Ends the active offscreen shadow depth pass before main-frame rendering resumes.
    void PsVitaGxmRenderer::EndShadowDepthPass() {
        if (!ShadowMap.IsDepthPassActive()) {
            throw std::runtime_error("PS Vita shadow depth pass cannot end because it is not active.");
        }

        ShadowMap.EndDepthPass();
    }

    /// Draws one indexed mesh through the artifact-backed ShadowDepth program during the active shadow pass.
    bool PsVitaGxmRenderer::DrawShadowDepthMesh(
        const ::float4x4& lightViewProjection,
        const ::float3* positions,
        int32_t vertexCount,
        const std::uint32_t* indices,
        int32_t indexCount,
        const std::string& shaderAssetId,
        const std::string& vertexProgramName,
        const std::string& pixelProgramName) {
        if (!Initialized || !ShadowMap.IsDepthPassActive() || positions == nullptr || indices == nullptr
            || vertexCount <= 0 || indexCount <= 0 || shaderAssetId.empty() || vertexProgramName.empty() || pixelProgramName.empty()) {
            return false;
        }
        if (!ShaderBundleLoaded && !(ShaderBundleLoaded = ShaderBundle.Load("app0:/cooked/shaders/psvita/shaders.psvb"))) {
            throw std::runtime_error("PS Vita ShadowDepth Shader bundle could not be loaded.");
        }

        const shaders::PsVitaShaderBundleEntry* entry = ShaderBundle.Find(shaderAssetId, vertexProgramName, pixelProgramName, "ShadowDepth");
        if (entry == nullptr) {
            throw std::runtime_error("PS Vita ShadowDepth Shader bundle does not contain the required canonical variant.");
        }
        const std::string key = shaderAssetId + "\n" + vertexProgramName + "\n" + pixelProgramName + "\nShadowDepth";
        if (ShadowDepthProgramKey != key) {
            ShadowDepthProgram.Reset();
            ShadowDepthProgramKey.clear();
        }
        if (!ShadowDepthProgram.IsReady() && !ShadowDepthProgram.Initialize(entry->VertexArtifactBytes, entry->FragmentArtifactBytes)) {
            throw std::runtime_error("PS Vita ShadowDepth Shader program creation failed from the compiled artifacts.");
        }
        ShadowDepthProgramKey = key;

        const std::size_t positionBytes = sizeof(::float3) * static_cast<std::size_t>(vertexCount);
        const std::size_t indexOffset = (positionBytes + 7u) & ~static_cast<std::size_t>(7u);
        const std::size_t indexBytes = sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount);
        if (indexOffset > std::numeric_limits<std::size_t>::max() - indexBytes) {
            throw std::runtime_error("PS Vita ShadowDepth Shader mesh submission exceeds the supported GPU allocation size.");
        }

        std::unique_ptr<PsVitaGxmShadowSubmissionMemory> submissionMemory = std::make_unique<PsVitaGxmShadowSubmissionMemory>();
        if (!submissionMemory->Allocate(indexOffset + indexBytes)) {
            throw std::runtime_error(
                "PS Vita ShadowDepth Shader failed to allocate transient mesh memory: "
                + submissionMemory->GetLastDiagnostic());
        }
        ::float3* gpuPositions = static_cast<::float3*>(submissionMemory->GetData());
        std::uint32_t* gpuIndices = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(submissionMemory->GetData()) + indexOffset);
        std::memcpy(gpuPositions, positions, sizeof(::float3) * static_cast<std::size_t>(vertexCount));
        std::memcpy(gpuIndices, indices, sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount));

        SceGxmContext* context = ShadowDepthProgram.GetContext();
        if (context == nullptr) {
            throw std::runtime_error("PS Vita ShadowDepth Shader requires one active GXM context.");
        }
        const float matrixValues[16] = {
            lightViewProjection.M11, lightViewProjection.M12, lightViewProjection.M13, lightViewProjection.M14,
            lightViewProjection.M21, lightViewProjection.M22, lightViewProjection.M23, lightViewProjection.M24,
            lightViewProjection.M31, lightViewProjection.M32, lightViewProjection.M33, lightViewProjection.M34,
            lightViewProjection.M41, lightViewProjection.M42, lightViewProjection.M43, lightViewProjection.M44
        };
        sceGxmSetVertexProgram(context, ShadowDepthProgram.GetVertexProgram());
        sceGxmSetFragmentProgram(context, ShadowDepthProgram.GetFragmentProgram());
        void* vertexUniformBuffer = nullptr;
        if (sceGxmReserveVertexDefaultUniformBuffer(context, &vertexUniformBuffer) < 0 || vertexUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita ShadowDepth Shader could not reserve its vertex uniform buffer.");
        }
        sceGxmSetUniformDataF(vertexUniformBuffer, ShadowDepthProgram.GetLightViewProjectionParameter(), 0u, 16u, matrixValues);
        if (sceGxmSetVertexStream(context, 0u, gpuPositions) < 0
            || sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U32, gpuIndices, static_cast<unsigned int>(indexCount)) < 0) {
            throw std::runtime_error("PS Vita ShadowDepth Shader failed to draw the indexed caster mesh.");
        }

        ShadowSubmissionMemory.push_back(std::move(submissionMemory));
        return true;
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

    /// Records one batch of solid-color triangles for later native submission.
    void PsVitaGxmRenderer::SubmitSolidColorTriangles(const std::vector<PsVitaSolidColorVertex>& vertices) {
        if (!Initialized || !FrameBegun || vertices.empty()) {
            return;
        }

        vita2d_color_vertex* drawVertices = static_cast<vita2d_color_vertex*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(vita2d_color_vertex) * vertices.size()),
            8u));
        if (drawVertices == nullptr) {
            throw std::runtime_error("PS Vita rounded-rect submission failed to allocate transient GPU-visible vertex memory.");
        }

        for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
            drawVertices[vertexIndex].x = vertices[vertexIndex].PositionX;
            drawVertices[vertexIndex].y = vertices[vertexIndex].PositionY;
            drawVertices[vertexIndex].z = 0.5f;
            drawVertices[vertexIndex].color = vertices[vertexIndex].ColorAbgr;
        }

        vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, drawVertices, static_cast<unsigned int>(vertices.size()));
    }

    /// Records one batch of already projected 3D mesh triangles as solid white GPU geometry.
    void PsVitaGxmRenderer::SubmitSolidWhiteMeshTriangles(const std::vector<::float3>& vertices) {
        if (!Initialized || !FrameBegun || vertices.empty()) {
            return;
        }

        vita2d_color_vertex* drawVertices = static_cast<vita2d_color_vertex*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(vita2d_color_vertex) * vertices.size()),
            8u));
        if (drawVertices == nullptr) {
            throw std::runtime_error("PS Vita white-mesh submission failed to allocate transient GPU-visible vertex memory.");
        }

        for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
            drawVertices[vertexIndex].x = vertices[vertexIndex].X;
            drawVertices[vertexIndex].y = vertices[vertexIndex].Y;
            drawVertices[vertexIndex].z = vertices[vertexIndex].Z;
            drawVertices[vertexIndex].color = 0xFFFFFFFFu;
        }

        vita2d_draw_array(SCE_GXM_PRIMITIVE_TRIANGLES, drawVertices, static_cast<unsigned int>(vertices.size()));
    }

    /// Draws one indexed runtime mesh through the first programmable solid-color GXM path.
    bool PsVitaGxmRenderer::DrawSolidColorMesh(
        const ::float4x4& worldViewProjection,
        const ::float3* positions,
        int32_t positionCount,
        const std::uint32_t* indices,
        int32_t indexCount,
        std::uint32_t colorAbgr) {
        if (!Initialized
            || !FrameBegun
            || positions == nullptr
            || indices == nullptr
            || positionCount <= 0
            || indexCount <= 0) {
            return false;
        }
        if (!EnableRuntimeCompiledSolidColorProgram) {
            return false;
        }

        if (!SolidColorProgram.IsReady() && !SolidColorProgram.Initialize()) {
            return false;
        }

        ::float3* gpuVisiblePositions = static_cast<::float3*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(::float3) * static_cast<std::size_t>(positionCount)),
            8u));
        if (gpuVisiblePositions == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to allocate transient GPU-visible vertex memory.");
        }

        std::uint32_t* gpuVisibleIndices = static_cast<std::uint32_t*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount)),
            8u));
        if (gpuVisibleIndices == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to allocate transient GPU-visible index memory.");
        }

        std::memcpy(gpuVisiblePositions, positions, sizeof(::float3) * static_cast<std::size_t>(positionCount));
        std::memcpy(gpuVisibleIndices, indices, sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount));

        SceGxmContext* context = SolidColorProgram.GetContext();
        if (context == nullptr) {
            return false;
        }

        sceGxmSetVertexProgram(context, SolidColorProgram.GetVertexProgram());
        sceGxmSetFragmentProgram(context, SolidColorProgram.GetFragmentProgram());
        UploadSolidColorWorldViewProjection(context, SolidColorProgram.GetWorldViewProjectionParameter(), worldViewProjection);
        UploadSolidColorBaseColor(context, SolidColorProgram.GetBaseColorParameter(), colorAbgr);

        if (sceGxmSetVertexStream(context, 0u, gpuVisiblePositions) < 0) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to bind the runtime-compiled vertex stream.");
        }

        if (sceGxmDraw(
            context,
            SCE_GXM_PRIMITIVE_TRIANGLES,
            SCE_GXM_INDEX_FORMAT_U32,
            gpuVisibleIndices,
            static_cast<unsigned int>(indexCount)) < 0) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to draw the runtime-compiled indexed mesh.");
        }

        return true;
    }

    /// Draws one indexed runtime mesh through the artifact-backed GPU forward-Lambert path.
    bool PsVitaGxmRenderer::DrawForwardLambertMesh(
        const ::float4x4& worldViewProjection,
        const ::float4x4& normalTransform,
        const ::float3* positions,
        const ::float3* normals,
        int32_t vertexCount,
        const std::uint32_t* indices,
        int32_t indexCount,
        std::uint32_t colorAbgr,
        const std::string& shaderAssetId,
        const std::string& vertexProgramName,
        const std::string& pixelProgramName,
        const std::string& variantName,
        const ::float3& lightDirection,
        const ::float3& lightColor,
        const ::float3& ambientColor) {
        if (!Initialized || !FrameBegun || positions == nullptr || normals == nullptr || indices == nullptr
            || vertexCount <= 0 || indexCount <= 0) {
            return false;
        }

        if (shaderAssetId.empty() || vertexProgramName.empty() || pixelProgramName.empty() || variantName.empty()) {
            return false;
        }
        if (!ShaderBundleLoaded && !(ShaderBundleLoaded = ShaderBundle.Load("app0:/cooked/shaders/psvita/shaders.psvb"))) return false;
        const shaders::PsVitaShaderBundleEntry* entry = ShaderBundle.Find(shaderAssetId, vertexProgramName, pixelProgramName, variantName);
        if (entry == nullptr) return false;
        std::string key = shaderAssetId + "\n" + vertexProgramName + "\n" + pixelProgramName + "\n" + variantName;
        if (ForwardLambertProgramKey != key) { ForwardLambertProgram.Reset(); ForwardLambertProgramKey.clear(); }
        if (!ForwardLambertProgram.IsReady() && !ForwardLambertProgram.Initialize(entry->VertexArtifactBytes, entry->FragmentArtifactBytes)) {
            return false;
        }
        ForwardLambertProgramKey = key;

        PsVitaForwardLambertVertex* gpuVertices = static_cast<PsVitaForwardLambertVertex*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(PsVitaForwardLambertVertex) * static_cast<std::size_t>(vertexCount)), 8u));
        std::uint32_t* gpuIndices = static_cast<std::uint32_t*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount)), 8u));
        if (gpuVertices == nullptr || gpuIndices == nullptr) {
            throw std::runtime_error("PS Vita forward-Lambert submission failed to allocate transient GPU-visible mesh memory.");
        }

        for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            gpuVertices[vertexIndex].Position = positions[vertexIndex];
            gpuVertices[vertexIndex].Normal = normals[vertexIndex];
            gpuVertices[vertexIndex].TexCoord = ::float2(0.0f, 0.0f);
        }
        std::memcpy(gpuIndices, indices, sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount));

        SceGxmContext* context = ForwardLambertProgram.GetContext();
        if (context == nullptr) {
            return false;
        }

        const float worldViewProjectionValues[16] = {
            worldViewProjection.M11, worldViewProjection.M12, worldViewProjection.M13, worldViewProjection.M14,
            worldViewProjection.M21, worldViewProjection.M22, worldViewProjection.M23, worldViewProjection.M24,
            worldViewProjection.M31, worldViewProjection.M32, worldViewProjection.M33, worldViewProjection.M34,
            worldViewProjection.M41, worldViewProjection.M42, worldViewProjection.M43, worldViewProjection.M44
        };
        const float normalTransformValues[16] = {
            normalTransform.M11, normalTransform.M12, normalTransform.M13, normalTransform.M14,
            normalTransform.M21, normalTransform.M22, normalTransform.M23, normalTransform.M24,
            normalTransform.M31, normalTransform.M32, normalTransform.M33, normalTransform.M34,
            normalTransform.M41, normalTransform.M42, normalTransform.M43, normalTransform.M44
        };
        const float baseColorValues[4] = {
            static_cast<float>(colorAbgr & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 8u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 16u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 24u) & 0xFFu) / 255.0f
        };
        const float lightDirectionValues[4] = { lightDirection.X, lightDirection.Y, lightDirection.Z, 0.0f };
        const float lightColorValues[4] = { lightColor.X, lightColor.Y, lightColor.Z, 1.0f };
        const float ambientValues[4] = { ambientColor.X, ambientColor.Y, ambientColor.Z, 1.0f };

        sceGxmSetVertexProgram(context, ForwardLambertProgram.GetVertexProgram());
        sceGxmSetFragmentProgram(context, ForwardLambertProgram.GetFragmentProgram());
        PsVitaForwardLambertUniformBinder::Bind(
            context,
            ForwardLambertProgram.GetWorldViewProjectionParameter(),
            ForwardLambertProgram.GetNormalTransformParameter(),
            ForwardLambertProgram.GetBaseColorParameter(),
            ForwardLambertProgram.GetLightDirectionParameter(),
            ForwardLambertProgram.GetLightColorParameter(),
            ForwardLambertProgram.GetAmbientParameter(),
            worldViewProjectionValues,
            normalTransformValues,
            baseColorValues,
            lightDirectionValues,
            lightColorValues,
            ambientValues);
        if (sceGxmSetVertexStream(context, 0u, gpuVertices) < 0
            || sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U32, gpuIndices, static_cast<unsigned int>(indexCount)) < 0) {
            throw std::runtime_error("PS Vita forward-Lambert submission failed to draw the indexed mesh.");
        }

        return true;
    }

    /// Draws one indexed runtime mesh through the textured artifact-backed Forward Standard Shader profile.
    bool PsVitaGxmRenderer::DrawForwardStandardMesh(
        const ::float4x4& worldViewProjection,
        const ::float4x4& normalTransform,
        const ::float4x4& lightViewProjection,
        const ::float3* positions,
        const ::float3* normals,
        const ::float2* texCoords,
        int32_t vertexCount,
        const std::uint32_t* indices,
        int32_t indexCount,
        std::uint32_t colorAbgr,
        const std::string& shaderAssetId,
        const std::string& vertexProgramName,
        const std::string& pixelProgramName,
        const std::string& variantName,
        const ::float3& lightDirection,
        const ::float3& lightColor,
        const ::float3& ambientColor,
        PsVitaRuntimeTexture* diffuseTexture) {
        if (!Initialized || !FrameBegun || positions == nullptr || normals == nullptr || texCoords == nullptr || indices == nullptr
            || vertexCount <= 0 || indexCount <= 0 || shaderAssetId.empty() || vertexProgramName.empty() || pixelProgramName.empty() || variantName.empty()) {
            throw std::runtime_error("PS Vita Forward Standard Shader received an invalid mesh draw request.");
        }
        if (!ShaderBundleLoaded && !(ShaderBundleLoaded = ShaderBundle.Load("app0:/cooked/shaders/psvita/shaders.psvb"))) {
            throw std::runtime_error("PS Vita Forward Standard Shader bundle could not be loaded.");
        }
        const shaders::PsVitaShaderBundleEntry* entry = ShaderBundle.Find(shaderAssetId, vertexProgramName, pixelProgramName, variantName);
        if (entry == nullptr) {
            throw std::runtime_error("PS Vita Forward Standard Shader bundle does not contain the requested material program.");
        }
        std::string key = shaderAssetId + "\n" + vertexProgramName + "\n" + pixelProgramName + "\n" + variantName;
        if (ForwardStandardProgramKey != key) { ForwardStandardProgram.Reset(); ForwardStandardProgramKey.clear(); }
        const bool shadowed = variantName == "ForwardStandardShadowed";
        if (!ForwardStandardProgram.IsReady() && !(shadowed
            ? ForwardStandardProgram.InitializeTexturedShadowed(entry->VertexArtifactBytes, entry->FragmentArtifactBytes)
            : ForwardStandardProgram.InitializeTextured(entry->VertexArtifactBytes, entry->FragmentArtifactBytes))) {
            throw std::runtime_error("PS Vita Forward Standard Shader program creation failed from the requested compiled artifacts.");
        }
        ForwardStandardProgramKey = key;

        vita2d_texture* nativeTexture = nullptr;
        std::uint32_t textureWidth = 0u;
        std::uint32_t textureHeight = 0u;
        if (diffuseTexture == nullptr) {
            nativeTexture = GetOrCreateStandardWhiteTexture();
            textureWidth = StandardFallbackTextureDimension;
            textureHeight = StandardFallbackTextureDimension;
        } else {
            EnsureUploaded(diffuseTexture);
            PsVitaGpuTexture* gpuTexture = diffuseTexture->GetGpuTexture();
            if (gpuTexture == nullptr || !gpuTexture->IsUploaded() || gpuTexture->GetNativeTexture() == nullptr) {
                throw std::runtime_error("PS Vita Forward Standard Shader could not prepare the authored diffuse texture.");
            }

            nativeTexture = gpuTexture->GetNativeTexture();
            textureWidth = gpuTexture->GetWidth();
            textureHeight = gpuTexture->GetHeight();
        }
        SceGxmTexture diffuseGxmTexture;
        int textureInitializationResult = sceGxmTextureInitLinear(
            &diffuseGxmTexture,
            vita2d_texture_get_datap(nativeTexture),
            vita2d_texture_get_format(nativeTexture),
            textureWidth,
            textureHeight,
            0u);
        if (textureInitializationResult < 0) {
            throw std::runtime_error("PS Vita Forward Standard Shader failed to initialize its fragment texture binding. sceGxmTextureInitLinear="
                + std::to_string(textureInitializationResult));
        }

        int minimumFilterResult = sceGxmTextureSetMinFilter(&diffuseGxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
        if (minimumFilterResult < 0) {
            throw std::runtime_error("PS Vita Forward Standard Shader failed to configure its fragment texture minimum filter. sceGxmTextureSetMinFilter="
                + std::to_string(minimumFilterResult));
        }

        int magnificationFilterResult = sceGxmTextureSetMagFilter(&diffuseGxmTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
        if (magnificationFilterResult < 0) {
            throw std::runtime_error("PS Vita Forward Standard Shader failed to configure its fragment texture magnification filter. sceGxmTextureSetMagFilter="
                + std::to_string(magnificationFilterResult));
        }

        int horizontalAddressModeResult = sceGxmTextureSetUAddrMode(&diffuseGxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
        if (horizontalAddressModeResult < 0) {
            throw std::runtime_error("PS Vita Forward Standard Shader failed to configure horizontal diffuse texture wrapping. sceGxmTextureSetUAddrMode="
                + std::to_string(horizontalAddressModeResult));
        }

        int verticalAddressModeResult = sceGxmTextureSetVAddrMode(&diffuseGxmTexture, SCE_GXM_TEXTURE_ADDR_REPEAT);
        if (verticalAddressModeResult < 0) {
            throw std::runtime_error("PS Vita Forward Standard Shader failed to configure vertical diffuse texture wrapping. sceGxmTextureSetVAddrMode="
                + std::to_string(verticalAddressModeResult));
        }

        PsVitaForwardLambertVertex* gpuVertices = static_cast<PsVitaForwardLambertVertex*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(PsVitaForwardLambertVertex) * static_cast<std::size_t>(vertexCount)), 8u));
        std::uint32_t* gpuIndices = static_cast<std::uint32_t*>(vita2d_pool_memalign(
            static_cast<unsigned int>(sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount)), 8u));
        if (gpuVertices == nullptr || gpuIndices == nullptr) {
            throw std::runtime_error("PS Vita Forward Standard submission failed to allocate transient GPU-visible mesh memory.");
        }
        for (int32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
            gpuVertices[vertexIndex].Position = positions[vertexIndex];
            gpuVertices[vertexIndex].Normal = normals[vertexIndex];
            gpuVertices[vertexIndex].TexCoord = texCoords[vertexIndex];
        }
        std::memcpy(gpuIndices, indices, sizeof(std::uint32_t) * static_cast<std::size_t>(indexCount));

        SceGxmContext* context = ForwardStandardProgram.GetContext();
        if (context == nullptr) {
            throw std::runtime_error("PS Vita Forward Standard Shader program did not provide one active GXM context.");
        }
        const float worldViewProjectionValues[16] = {
            worldViewProjection.M11, worldViewProjection.M12, worldViewProjection.M13, worldViewProjection.M14,
            worldViewProjection.M21, worldViewProjection.M22, worldViewProjection.M23, worldViewProjection.M24,
            worldViewProjection.M31, worldViewProjection.M32, worldViewProjection.M33, worldViewProjection.M34,
            worldViewProjection.M41, worldViewProjection.M42, worldViewProjection.M43, worldViewProjection.M44
        };
        const float normalTransformValues[16] = {
            normalTransform.M11, normalTransform.M12, normalTransform.M13, normalTransform.M14,
            normalTransform.M21, normalTransform.M22, normalTransform.M23, normalTransform.M24,
            normalTransform.M31, normalTransform.M32, normalTransform.M33, normalTransform.M34,
            normalTransform.M41, normalTransform.M42, normalTransform.M43, normalTransform.M44
        };
        const float lightViewProjectionValues[16] = {
            lightViewProjection.M11, lightViewProjection.M12, lightViewProjection.M13, lightViewProjection.M14,
            lightViewProjection.M21, lightViewProjection.M22, lightViewProjection.M23, lightViewProjection.M24,
            lightViewProjection.M31, lightViewProjection.M32, lightViewProjection.M33, lightViewProjection.M34,
            lightViewProjection.M41, lightViewProjection.M42, lightViewProjection.M43, lightViewProjection.M44
        };
        const float baseColorValues[4] = {
            static_cast<float>(colorAbgr & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 8u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 16u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 24u) & 0xFFu) / 255.0f
        };
        const float lightDirectionValues[4] = { lightDirection.X, lightDirection.Y, lightDirection.Z, 0.0f };
        const float lightColorValues[4] = { lightColor.X, lightColor.Y, lightColor.Z, 1.0f };
        const float ambientValues[4] = { ambientColor.X, ambientColor.Y, ambientColor.Z, 1.0f };
        const float shadowBiasValues[4] = { 0.003f, 0.0f, 0.0f, 0.0f };

        sceGxmSetVertexProgram(context, ForwardStandardProgram.GetVertexProgram());
        sceGxmSetFragmentProgram(context, ForwardStandardProgram.GetFragmentProgram());
        if (shadowed) {
            PsVitaForwardLambertUniformBinder::BindShadowed(
                context,
                ForwardStandardProgram.GetWorldViewProjectionParameter(),
                ForwardStandardProgram.GetNormalTransformParameter(),
                ForwardStandardProgram.GetBaseColorParameter(),
                ForwardStandardProgram.GetLightDirectionParameter(),
                ForwardStandardProgram.GetLightColorParameter(),
                ForwardStandardProgram.GetAmbientParameter(),
                ForwardStandardProgram.GetLightViewProjectionParameter(),
                ForwardStandardProgram.GetShadowBiasParameter(),
                worldViewProjectionValues,
                normalTransformValues,
                baseColorValues,
                lightDirectionValues,
                lightColorValues,
                ambientValues,
                lightViewProjectionValues,
                shadowBiasValues);
            vita2d_texture* shadowTexture = ShadowMap.GetTexture();
            if (shadowTexture == nullptr) {
                throw std::runtime_error("PS Vita Forward Standard Shader requires one completed directional shadow map.");
            }

            SceGxmTexture shadowGxmTexture;
            if (sceGxmTextureInitLinear(&shadowGxmTexture, vita2d_texture_get_datap(shadowTexture), vita2d_texture_get_format(shadowTexture), vita2d_texture_get_width(shadowTexture), vita2d_texture_get_height(shadowTexture), 0u) < 0
                || sceGxmTextureSetMinFilter(&shadowGxmTexture, SCE_GXM_TEXTURE_FILTER_POINT) < 0
                || sceGxmTextureSetMagFilter(&shadowGxmTexture, SCE_GXM_TEXTURE_FILTER_POINT) < 0
                || sceGxmSetFragmentTexture(context, sceGxmProgramParameterGetResourceIndex(ForwardStandardProgram.GetShadowTextureParameter()), &shadowGxmTexture) < 0) {
                throw std::runtime_error("PS Vita Forward Standard Shader failed to bind its directional shadow texture.");
            }
        } else {
            PsVitaForwardLambertUniformBinder::Bind(
                context,
                ForwardStandardProgram.GetWorldViewProjectionParameter(),
                ForwardStandardProgram.GetNormalTransformParameter(),
                ForwardStandardProgram.GetBaseColorParameter(),
                ForwardStandardProgram.GetLightDirectionParameter(),
                ForwardStandardProgram.GetLightColorParameter(),
                ForwardStandardProgram.GetAmbientParameter(),
                worldViewProjectionValues,
                normalTransformValues,
                baseColorValues,
                lightDirectionValues,
                lightColorValues,
                ambientValues);
        }
        unsigned int diffuseTextureIndex = sceGxmProgramParameterGetResourceIndex(ForwardStandardProgram.GetDiffuseTextureParameter());
        if (sceGxmSetFragmentTexture(context, diffuseTextureIndex, &diffuseGxmTexture) < 0
            || sceGxmSetVertexStream(context, 0u, gpuVertices) < 0
            || sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U32, gpuIndices, static_cast<unsigned int>(indexCount)) < 0) {
            throw std::runtime_error("PS Vita Forward Standard submission failed to draw the indexed textured mesh.");
        }

        return true;
    }

    /// Returns the persistent opaque white texture used when one Standard material has no authored diffuse texture asset.
    vita2d_texture* PsVitaGxmRenderer::GetOrCreateStandardWhiteTexture() {
        if (StandardWhiteTexture != nullptr) {
            return StandardWhiteTexture;
        }

        vita2d_texture* nativeTexture = vita2d_create_empty_texture_format(
            StandardFallbackTextureDimension,
            StandardFallbackTextureDimension,
            SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
        if (nativeTexture == nullptr) {
            throw std::runtime_error("PS Vita Standard Shader fallback failed to allocate its white texture.");
        }

        std::uint32_t* nativePixels = static_cast<std::uint32_t*>(vita2d_texture_get_datap(nativeTexture));
        if (nativePixels == nullptr) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita Standard Shader fallback failed to acquire its white texture pixels.");
        }

        std::size_t stridePixels = static_cast<std::size_t>(vita2d_texture_get_stride(nativeTexture)) / sizeof(std::uint32_t);
        if (stridePixels < StandardFallbackTextureDimension) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita Standard Shader fallback received a native texture stride smaller than its required dimension.");
        }

        for (std::uint32_t rowIndex = 0u; rowIndex < StandardFallbackTextureDimension; ++rowIndex) {
            std::uint32_t* rowPixels = nativePixels + (static_cast<std::size_t>(rowIndex) * stridePixels);
            for (std::uint32_t columnIndex = 0u; columnIndex < StandardFallbackTextureDimension; ++columnIndex) {
                rowPixels[columnIndex] = 0xFFFFFFFFu;
            }
        }
        StandardWhiteTexture = nativeTexture;
        return StandardWhiteTexture;
    }

    /// Waits for completed rendering before releasing shadow-caster allocations retained across the offscreen/main-frame boundary.
    void PsVitaGxmRenderer::ReleaseCompletedShadowSubmissionMemory() {
        if (ShadowSubmissionMemory.empty()) {
            return;
        }

        vita2d_wait_rendering_done();
        ShadowSubmissionMemory.clear();
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

    /// Returns the padded native Vita texture dimension used to avoid odd-sized runtime uploads that destabilize Vita3K's texture destroy path.
    std::uint32_t PsVitaGxmRenderer::CalculatePaddedTextureDimension(std::uint32_t textureDimension) {
        if (textureDimension == 0u) {
            throw std::runtime_error("PS Vita GPU upload requires positive texture dimensions.");
        }

        std::uint32_t paddedDimension = 1u;
        while (paddedDimension < textureDimension) {
            paddedDimension <<= 1u;
        }

        return paddedDimension;
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
        std::uint32_t nativeTextureWidth = CalculatePaddedTextureDimension(textureWidth);
        std::uint32_t nativeTextureHeight = CalculatePaddedTextureDimension(textureHeight);
        if (textureWidth == 0u || textureHeight == 0u || !runtimeTexture->HasPixels()) {
            throw std::runtime_error("PS Vita GPU upload requires non-empty runtime texture pixels and dimensions.");
        }

        vita2d_texture* nativeTexture = vita2d_create_empty_texture_format(nativeTextureWidth, nativeTextureHeight, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8);
        if (nativeTexture == nullptr) {
            throw std::runtime_error("PS Vita GPU upload failed to allocate one native texture.");
        }

        std::uint32_t* nativePixels = static_cast<std::uint32_t*>(vita2d_texture_get_datap(nativeTexture));
        if (nativePixels == nullptr) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita GPU upload failed to acquire the native texture pixel buffer.");
        }

        std::size_t stridePixels = static_cast<std::size_t>(vita2d_texture_get_stride(nativeTexture)) / sizeof(std::uint32_t);
        if (stridePixels < nativeTextureWidth) {
            vita2d_free_texture(nativeTexture);
            throw std::runtime_error("PS Vita GPU upload received one native texture stride smaller than the authored width.");
        }

        std::memset(
            nativePixels,
            0,
            stridePixels * static_cast<std::size_t>(nativeTextureHeight) * sizeof(std::uint32_t));
        const std::uint32_t* sourcePixels = runtimeTexture->GetPixelsAbgr8888();
        for (std::uint32_t rowIndex = 0u; rowIndex < textureHeight; ++rowIndex) {
            std::uint32_t* destinationRow = nativePixels + (static_cast<std::size_t>(rowIndex) * stridePixels);
            const std::uint32_t* sourceRow = sourcePixels + (static_cast<std::size_t>(rowIndex) * textureWidth);
            std::memcpy(destinationRow, sourceRow, static_cast<std::size_t>(textureWidth) * sizeof(std::uint32_t));
        }

        vita2d_texture_set_filters(nativeTexture, SCE_GXM_TEXTURE_FILTER_LINEAR, SCE_GXM_TEXTURE_FILTER_LINEAR);

        std::unique_ptr<PsVitaGpuTexture> gpuTexture = std::make_unique<PsVitaGpuTexture>();
        gpuTexture->SetNativeTexture(nativeTexture, nativeTextureWidth, nativeTextureHeight);
        runtimeTexture->SetGpuTexture(std::move(gpuTexture));
        AppendRendererTrace("GxmRenderer: EnsureUploaded runtimeTexture="
            + std::to_string(reinterpret_cast<std::uintptr_t>(runtimeTexture))
            + " id="
            + runtimeTexture->get_Id()
            + " nativeTexture="
            + std::to_string(reinterpret_cast<std::uintptr_t>(nativeTexture))
            + " logicalWidth="
            + std::to_string(textureWidth)
            + " logicalHeight="
            + std::to_string(textureHeight)
            + " nativeWidth="
            + std::to_string(nativeTextureWidth)
            + " nativeHeight="
            + std::to_string(nativeTextureHeight)
            + " stridePixels="
            + std::to_string(stridePixels));
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
        float textureUScale = 1.0f;
        float textureVScale = 1.0f;
        if (queuedQuad.UsesLogicalTextureExtents) {
            textureUScale = static_cast<float>(queuedQuad.Texture->GetTextureWidthPixels())
                / static_cast<float>(gpuTexture->GetWidth());
            textureVScale = static_cast<float>(queuedQuad.Texture->GetTextureHeightPixels())
                / static_cast<float>(gpuTexture->GetHeight());
        }

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
        triangleVertices[0].u = queuedQuad.Vertices[0].TextureU * textureUScale;
        triangleVertices[0].v = queuedQuad.Vertices[0].TextureV * textureVScale;

        triangleVertices[1].x = queuedQuad.Vertices[1].PositionX;
        triangleVertices[1].y = queuedQuad.Vertices[1].PositionY;
        triangleVertices[1].z = 0.5f;
        triangleVertices[1].u = queuedQuad.Vertices[1].TextureU * textureUScale;
        triangleVertices[1].v = queuedQuad.Vertices[1].TextureV * textureVScale;

        triangleVertices[2].x = queuedQuad.Vertices[2].PositionX;
        triangleVertices[2].y = queuedQuad.Vertices[2].PositionY;
        triangleVertices[2].z = 0.5f;
        triangleVertices[2].u = queuedQuad.Vertices[2].TextureU * textureUScale;
        triangleVertices[2].v = queuedQuad.Vertices[2].TextureV * textureVScale;

        triangleVertices[3] = triangleVertices[2];

        triangleVertices[4].x = queuedQuad.Vertices[1].PositionX;
        triangleVertices[4].y = queuedQuad.Vertices[1].PositionY;
        triangleVertices[4].z = 0.5f;
        triangleVertices[4].u = queuedQuad.Vertices[1].TextureU * textureUScale;
        triangleVertices[4].v = queuedQuad.Vertices[1].TextureV * textureVScale;

        triangleVertices[5].x = queuedQuad.Vertices[3].PositionX;
        triangleVertices[5].y = queuedQuad.Vertices[3].PositionY;
        triangleVertices[5].z = 0.5f;
        triangleVertices[5].u = queuedQuad.Vertices[3].TextureU * textureUScale;
        triangleVertices[5].v = queuedQuad.Vertices[3].TextureV * textureVScale;

        vita2d_draw_array_textured(
            gpuTexture->GetNativeTexture(),
            SCE_GXM_PRIMITIVE_TRIANGLES,
            triangleVertices,
            6u,
            colorAbgr);
    }

    /// Submits one projected 3D triangle through the existing native textured-triangle path.
    void PsVitaGxmRenderer::SubmitTexturedTriangle(const PsVitaQueuedQuad& triangle) {
        if (!Initialized || !FrameBegun) {
            return;
        }

        const PsVitaTexturedQuadVertex& vertex0 = triangle.Vertices[0];
        const PsVitaTexturedQuadVertex& vertex1 = triangle.Vertices[1];
        const PsVitaTexturedQuadVertex& vertex2 = triangle.Vertices[2];
        const PsVitaTexturedQuadVertex midpoint01 = InterpolateTexturedVertex(vertex0, vertex1);
        const PsVitaTexturedQuadVertex midpoint12 = InterpolateTexturedVertex(vertex1, vertex2);
        const PsVitaTexturedQuadVertex midpoint20 = InterpolateTexturedVertex(vertex2, vertex0);

        SubmitQuad(PsVitaQueuedQuad { triangle.Texture, triangle.RenderOrder, { vertex0, midpoint01, midpoint20, midpoint20 } });
        SubmitQuad(PsVitaQueuedQuad { triangle.Texture, triangle.RenderOrder, { midpoint01, vertex1, midpoint12, midpoint12 } });
        SubmitQuad(PsVitaQueuedQuad { triangle.Texture, triangle.RenderOrder, { midpoint20, midpoint12, vertex2, vertex2 } });
        SubmitQuad(PsVitaQueuedQuad { triangle.Texture, triangle.RenderOrder, { midpoint01, midpoint12, midpoint20, midpoint20 } });
    }

    /// Interpolates one textured vertex for the scale-stable triangle subdivision path.
    PsVitaTexturedQuadVertex PsVitaGxmRenderer::InterpolateTexturedVertex(const PsVitaTexturedQuadVertex& left, const PsVitaTexturedQuadVertex& right) {
        return PsVitaTexturedQuadVertex {
            (left.PositionX + right.PositionX) * 0.5f,
            (left.PositionY + right.PositionY) * 0.5f,
            (left.TextureU + right.TextureU) * 0.5f,
            (left.TextureV + right.TextureV) * 0.5f,
            left.ColorAbgr
        };
    }

    /// Uploads one world-view-projection matrix into the runtime-compiled solid-color vertex shader uniform buffer.
    void PsVitaGxmRenderer::UploadSolidColorWorldViewProjection(
        SceGxmContext* context,
        const SceGxmProgramParameter* parameter,
        const ::float4x4& worldViewProjection) {
        if (context == nullptr || parameter == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission requires one valid GXM shader context and matrix parameter.");
        }

        void* vertexDefaultUniformBuffer = nullptr;
        if (sceGxmReserveVertexDefaultUniformBuffer(context, &vertexDefaultUniformBuffer) < 0 || vertexDefaultUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to reserve the default vertex uniform buffer.");
        }

        const float matrixValues[16] = {
            worldViewProjection.M11, worldViewProjection.M12, worldViewProjection.M13, worldViewProjection.M14,
            worldViewProjection.M21, worldViewProjection.M22, worldViewProjection.M23, worldViewProjection.M24,
            worldViewProjection.M31, worldViewProjection.M32, worldViewProjection.M33, worldViewProjection.M34,
            worldViewProjection.M41, worldViewProjection.M42, worldViewProjection.M43, worldViewProjection.M44
        };
        sceGxmSetUniformDataF(vertexDefaultUniformBuffer, parameter, 0u, 16u, matrixValues);
    }

    /// Uploads one packed ABGR solid color into the runtime-compiled solid-color fragment shader uniform buffer.
    void PsVitaGxmRenderer::UploadSolidColorBaseColor(
        SceGxmContext* context,
        const SceGxmProgramParameter* parameter,
        std::uint32_t colorAbgr) {
        if (context == nullptr || parameter == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission requires one valid GXM shader context and base-color parameter.");
        }

        void* fragmentDefaultUniformBuffer = nullptr;
        if (sceGxmReserveFragmentDefaultUniformBuffer(context, &fragmentDefaultUniformBuffer) < 0 || fragmentDefaultUniformBuffer == nullptr) {
            throw std::runtime_error("PS Vita solid-color mesh submission failed to reserve the default fragment uniform buffer.");
        }

        const float colorValues[4] = {
            static_cast<float>(colorAbgr & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 8u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 16u) & 0xFFu) / 255.0f,
            static_cast<float>((colorAbgr >> 24u) & 0xFFu) / 255.0f
        };
        sceGxmSetUniformDataF(fragmentDefaultUniformBuffer, parameter, 0u, 4u, colorValues);
    }
}

#endif
