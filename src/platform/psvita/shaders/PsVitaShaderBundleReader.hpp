#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

namespace helengine::psvita::shaders {
    /// Represents one material-addressable vertex and fragment artifact pair stored in the Vita runtime shader bundle.
    struct PsVitaShaderBundleEntry final {
        std::string ShaderAssetId;
        std::string VertexProgramName;
        std::string PixelProgramName;
        std::string VariantName;
        std::vector<std::uint8_t> VertexArtifactBytes;
        std::vector<std::uint8_t> FragmentArtifactBytes;
    };

    /// Loads the one cooked Vita shader bundle and resolves program pairs by the persistent material shader key.
    class PsVitaShaderBundleReader final {
    public:
        bool Load(const std::string& path);
        const PsVitaShaderBundleEntry* Find(const std::string& shaderAssetId, const std::string& vertexProgramName, const std::string& pixelProgramName, const std::string& variantName) const;

    private:
        std::vector<PsVitaShaderBundleEntry> Entries;
    };
}

#endif
