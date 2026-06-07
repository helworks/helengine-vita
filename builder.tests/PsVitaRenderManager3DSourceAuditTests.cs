using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita 3D renderer source so cooked mesh assets can build Vita runtime models and reach one concrete native draw path.
/// </summary>
public sealed class PsVitaRenderManager3DSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita 3D renderer owns concrete runtime model bridge files and exposes them through the native build.
    /// </summary>
    [Fact]
    public void Source_whenBuildingRuntimeModels_containsConcreteVitaRuntimeModelTypesAndBuildWiring() {
        string packedModelReaderHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaPackedModelReader.hpp");
        string packedModelReaderSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaPackedModelReader.cpp");
        string runtimeModelHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeModel.hpp");
        string runtimeModelSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeModel.cpp");
        string runtimeSubmeshHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeSubmesh.hpp");
        string runtimeSubmeshSourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRuntimeSubmesh.cpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.True(File.Exists(packedModelReaderHeaderPath), "Expected one Vita packed model reader header.");
        Assert.True(File.Exists(packedModelReaderSourcePath), "Expected one Vita packed model reader source file.");
        Assert.True(File.Exists(runtimeModelHeaderPath), "Expected one Vita runtime model header.");
        Assert.True(File.Exists(runtimeModelSourcePath), "Expected one Vita runtime model source file.");
        Assert.True(File.Exists(runtimeSubmeshHeaderPath), "Expected one Vita runtime submesh header.");
        Assert.True(File.Exists(runtimeSubmeshSourcePath), "Expected one Vita runtime submesh source file.");
        Assert.Contains("PsVitaPackedModelReader.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeModel.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeSubmesh.cpp", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 3D renderer stops returning null placeholders and builds one concrete Vita runtime model from model assets.
    /// </summary>
    [Fact]
    public void Source_whenBuildingRuntimeModels_validatesInputAndReturnsConcreteVitaRuntimeModel() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");

        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"platform/psvita/rendering/PsVitaRuntimeModel.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("if (data == nullptr)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("throw new ArgumentNullException", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PsVitaRuntimeModel", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("return nullptr;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 3D renderer can rebuild one cooked model payload by deserializing the packaged model asset and forwarding it through the raw Vita model path.
    /// </summary>
    [Fact]
    public void Source_whenResolvingCookedPlatformOwnedModels_reusesRawModelBuilderPath() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/psvita/rendering/PsVitaPackedModelReader.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::RuntimeModel* packedRuntimeModel = rendering::PsVitaPackedModelReader::TryRead(cookedAssetPath);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (packedRuntimeModel != nullptr)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"AssetSerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"Asset.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/native_cast.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"system/io/file.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::FileStream* stream = nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::Asset* asset = nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("stream = ::File::OpenRead(cookedAssetPath);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("asset = ::AssetSerializer::Deserialize(stream);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::ModelAsset* cookedModelAsset = he_cpp_try_cast<ModelAsset>(asset);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::RuntimeModel* runtimeModel = BuildModelFromRaw(cookedModelAsset);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 3D renderer can rebuild one cooked material payload by deserializing the packaged platform material asset and returning one concrete runtime material instance.
    /// </summary>
    [Fact]
    public void Source_whenResolvingCookedPlatformOwnedMaterials_buildsConcreteRuntimeMaterial() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("::RuntimeMaterial* BuildMaterialFromCooked(::PlatformMaterialAsset* materialAsset) override;", headerSource, StringComparison.Ordinal);
        Assert.Contains("#include \"EngineBinaryHeaderSerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"EditorAssetBinarySerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"PlatformMaterialAsset.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"ShaderMaterialAsset.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"ShaderMaterialAssetBinarySerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"RuntimeMaterial.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::EngineBinaryHeader* header = nullptr;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("header = ::EngineBinaryHeaderSerializer::Read(stream);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("if (header->FormatId == ShaderMaterialAssetBinarySerializer::FormatId)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::ShaderMaterialAsset* cookedShaderMaterialAsset = ::ShaderMaterialAssetBinarySerializer::Deserialize(stream, header);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("asset = ::EditorAssetBinarySerializer::Deserialize(stream, header);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::PlatformMaterialAsset* cookedMaterialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("auto* runtimeMaterial = new ::RuntimeMaterial();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("runtimeMaterial->set_Id(materialAsset->get_Id());", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("does not support opaque platform-owned cooked material creation", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the PS Vita 3D renderer traverses camera-owned mesh drawables and submits them through one explicit white-triangle GPU path.
    /// </summary>
    [Fact]
    public void Source_whenDrawingMeshComponents_submitsWhiteTrianglesThroughTheGpuPath() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");

        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("void DrawCamera(::ICamera* camera);", headerSource, StringComparison.Ordinal);
        Assert.Contains("void DrawRuntimeModel", headerSource, StringComparison.Ordinal);
        Assert.Contains("MeshComponent", sourceCode, StringComparison.Ordinal);
        Assert.Contains("DrawCamera(camera);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("DrawRuntimeModel", sourceCode, StringComparison.Ordinal);
        Assert.Contains("SubmitSolidWhiteMeshTriangles", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Vita camera path uses the same default forward basis as the engine's normal 3D renderers.
    /// </summary>
    [Fact]
    public void Source_whenBuildingCameraViewProjection_usesEngineNegativeZForwardBasis() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, 1.0f), cameraOrientation);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Vita renderer keeps the same view-projection and world-view-projection composition order used by working engine renderers.
    /// </summary>
    [Fact]
    public void Source_whenProjectingVitaMeshes_matchesEngineMatrixCompositionOrder() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaRenderManager3D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("float4x4::Multiply__ref0_ref1_out2(view, projection, viewProjection);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("float4x4::Multiply__ref0_ref1_out2(world, ActiveViewProjection, worldViewProjection);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("CameraProjectionUtils::CreatePerspectiveProjection(", sourceCode, StringComparison.Ordinal);
    }
}
