using System;
using System.IO;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita builder repository-root resolution source so copied builder assemblies can still recover the real checkout root.
/// </summary>
public sealed class PsVitaRepositoryRootSourceAuditTests {
    /// <summary>
    /// Verifies repository-root resolution does not rely only on a fixed number of parent traversals and instead searches upward for PS Vita repository markers.
    /// </summary>
    [Fact]
    public void Source_whenResolvingRepositoryRoot_searchesParentDirectoriesForPsVitaRepositoryMarkers() {
        string builderSourcePath = PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaPlatformAssetBuilder.cs");
        string builderSource = File.ReadAllText(builderSourcePath);

        Assert.Contains("HELENGINE_PSVITA_REPOSITORY_ROOT", builderSource, StringComparison.Ordinal);
        Assert.Contains("DirectoryInfo candidateDirectory", builderSource, StringComparison.Ordinal);
        Assert.Contains("candidateDirectory.Parent", builderSource, StringComparison.Ordinal);
        Assert.Contains("Dockerfile", builderSource, StringComparison.Ordinal);
        Assert.Contains("PsVitaBootHost.hpp", builderSource, StringComparison.Ordinal);
    }
}
