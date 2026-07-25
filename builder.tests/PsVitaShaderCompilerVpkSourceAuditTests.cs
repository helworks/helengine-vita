using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies that the standalone Vita compiler VPK uses only its fixed launch-and-exit protocol.
/// </summary>
public sealed class PsVitaShaderCompilerVpkSourceAuditTests {
    /// <summary>
    /// Ensures launching the compiler reads its fixed inbox, processes one job, and exits without command-line behavior.
    /// </summary>
    [Fact]
    public void CompilerVpk_WhenLaunched_ReadsTheFixedInboxWritesTheMatchingOutboxAndExits() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("tools", "shader-compiler", "src", "main.cpp"));

        Assert.Contains("ux0:data/helengine_shader_compiler/inbox/manifest.json", source, StringComparison.Ordinal);
        Assert.Contains("return queueProcessor.ProcessSingleJob();", source, StringComparison.Ordinal);
        Assert.DoesNotContain("argc", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Ensures the compiler VPK links the taiHEN stubs required by the libshacccg extension library.
    /// </summary>
    [Fact]
    public void CompilerVpk_WhenLinkingShaccCgExtensions_LinksTaihenStub() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("tools", "shader-compiler", "CMakeLists.txt"));

        Assert.Contains("taihen_stub", source, StringComparison.Ordinal);
    }
}
