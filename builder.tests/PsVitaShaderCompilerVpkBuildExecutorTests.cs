using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies argument validation for the standalone shader compiler VPK build executor.
/// </summary>
public sealed class PsVitaShaderCompilerVpkBuildExecutorTests {
    /// <summary>
    /// Ensures the executor requires an explicit repository root before attempting any Docker build.
    /// </summary>
    [Fact]
    public void Build_WhenRepositoryRootIsMissing_ThrowsAnArgumentException() {
        PsVitaShaderCompilerVpkBuildExecutor executor = new();

        Assert.Throws<ArgumentException>(() => executor.Build(string.Empty, "output", CancellationToken.None));
    }
}
