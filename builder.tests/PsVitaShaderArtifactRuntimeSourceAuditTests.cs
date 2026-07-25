using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the native Vita artifact loader and material reader contract before runtime shader compilation is removed.
/// </summary>
public sealed class PsVitaShaderArtifactRuntimeSourceAuditTests {
    /// <summary>
    /// Verifies that runtime artifact loading validates complete blobs and keeps GXM patching as a runtime operation.
    /// </summary>
    [Fact]
    public void Source_whenLoadingShaderArtifacts_validatesProgramsAndPatchesRuntimeState() {
        string readerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactReader.cpp");
        string materialReaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaCompiledShaderMaterialReader.cpp");
        string bundleReaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderBundleReader.cpp");
        string readerSource = File.Exists(readerPath) ? File.ReadAllText(readerPath) : string.Empty;
        string materialReaderSource = File.ReadAllText(materialReaderPath);

        Assert.True(File.Exists(readerPath), "Expected one native Vita shader artifact reader source file.");
        Assert.Contains("sceGxmProgramCheck", readerSource, StringComparison.Ordinal);
        Assert.Contains("programData", readerSource, StringComparison.Ordinal);
        Assert.Contains("programSize", readerSource, StringComparison.Ordinal);
        Assert.True(File.Exists(bundleReaderPath), "Expected one native Vita shader bundle reader source file.");
        Assert.Contains("PsVitaShaderBundleReader::Find", File.ReadAllText(bundleReaderPath), StringComparison.Ordinal);
        Assert.DoesNotContain("VertexArtifactHash", materialReaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("FragmentArtifactHash", materialReaderSource, StringComparison.Ordinal);
        Assert.Contains("ParameterContractVersion", materialReaderSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the production solid-color program no longer invokes runtime shader compilation.
    /// </summary>
    [Fact]
    public void Source_whenUsingCookedShaderArtifacts_doesNotCompileShadersAtRuntime() {
        string programPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "rendering", "PsVitaGxmSolidColorProgram.cpp");
        string programSource = File.ReadAllText(programPath);

        Assert.DoesNotContain("sceShaccCgCompileProgram", programSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceShaccCgInitializeCompileOptions", programSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceKernelLoadStartModule", programSource, StringComparison.Ordinal);
    }
}
