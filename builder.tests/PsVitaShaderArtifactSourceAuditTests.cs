using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the Vita-side shader artifact exporter so compiler output remains a complete versioned program payload.
/// </summary>
public sealed class PsVitaShaderArtifactSourceAuditTests {
    /// <summary>
    /// Verifies that the exporter invokes the Vita compiler and writes the complete program payload contract.
    /// </summary>
    [Fact]
    public void Source_whenExportingShaderArtifact_usesCompilerOutputAndStableFormat() {
        string writerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactWriter.cpp");
        string formatPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactFormat.hpp");
        string cmakeSource = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt"));
        string writerSource = File.Exists(writerPath) ? File.ReadAllText(writerPath) : string.Empty;
        string formatSource = File.Exists(formatPath) ? File.ReadAllText(formatPath) : string.Empty;

        Assert.True(File.Exists(writerPath), "Expected one Vita shader artifact writer source file.");
        Assert.True(File.Exists(formatPath), "Expected one Vita shader artifact format header.");
        Assert.Contains("sceShaccCgInitializeCompileOptions", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgCompileProgram", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgDestroyCompileOutput", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgGetVersionString", writerSource, StringComparison.Ordinal);
        Assert.Contains("programData", writerSource, StringComparison.Ordinal);
        Assert.Contains("programSize", writerSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaShaderArtifactMagic", formatSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaShaderArtifactVersion", formatSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaShaderArtifactWriter.cpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("SceShaccCg_stub", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the exporter supports both Vita compiler stage profiles.
    /// </summary>
    [Fact]
    public void Source_whenExportingShaderArtifact_supportsVertexAndFragmentProfiles() {
        string writerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactWriter.cpp");
        string writerSource = File.Exists(writerPath) ? File.ReadAllText(writerPath) : string.Empty;

        Assert.Contains("SCE_SHACCCG_PROFILE_VP", writerSource, StringComparison.Ordinal);
        Assert.Contains("SCE_SHACCCG_PROFILE_FP", writerSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that failed Vita compiler output preserves its diagnostic messages for hardware-side retrieval.
    /// </summary>
    [Fact]
    public void Source_whenVitaCompilerRejectsShader_preservesCompilerDiagnostics() {
        string writerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactWriter.cpp");
        string writerHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactWriter.hpp");
        string writerSource = File.Exists(writerPath) ? File.ReadAllText(writerPath) : string.Empty;
        string writerHeader = File.Exists(writerHeaderPath) ? File.ReadAllText(writerHeaderPath) : string.Empty;

        Assert.Contains("diagnosticCount", writerSource, StringComparison.Ordinal);
        Assert.Contains("diagnostics", writerSource, StringComparison.Ordinal);
        Assert.Contains("GetLastDiagnostic", writerHeader, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that the PSM shader compiler is loaded as its installed user module instead of an invalid sysmodule identifier.
    /// </summary>
    [Fact]
    public void Source_whenStartingVitaCompiler_loadsInstalledShaccCgModule() {
        string writerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "shaders", "PsVitaShaderArtifactWriter.cpp");
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string writerSource = File.Exists(writerPath) ? File.ReadAllText(writerPath) : string.Empty;
        string cmakeSource = File.Exists(cmakePath) ? File.ReadAllText(cmakePath) : string.Empty;

        Assert.Contains("ur0:/data/libshacccg.suprx", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceKernelLoadStartModule", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgExtEnableExtensions", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgSetDefaultAllocator", writerSource, StringComparison.Ordinal);
        Assert.Contains("sceShaccCgInitializeCallbackList", writerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("sceSysmoduleLoadModule(SCE_SYSMODULE_SHACCCG)", writerSource, StringComparison.Ordinal);
        Assert.Contains("SceShaccCgExt", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("taihen_stub", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("SceKernelModulemgr_stub", cmakeSource, StringComparison.Ordinal);
    }
}
