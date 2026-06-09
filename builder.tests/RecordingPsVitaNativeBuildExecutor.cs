using helengine.psvita.builder;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Fake native build executor that creates a VPK artifact without invoking Docker.
/// </summary>
sealed class RecordingPsVitaNativeBuildExecutor : IPsVitaNativeBuildExecutor {
    /// <summary>
    /// Gets whether the native build executor was invoked.
    /// </summary>
    public bool WasCalled { get; private set; }

    /// <summary>
    /// Gets the repository root that the builder passed into the native build executor.
    /// </summary>
    public string RepositoryRootPath { get; private set; }

    /// <summary>
    /// Gets the generated-core root that the builder passed into the native build executor.
    /// </summary>
    public string GeneratedCoreRootPath { get; private set; }

    /// <summary>
    /// Gets the staged content root that the builder passed into the native build executor.
    /// </summary>
    public string StagedContentRootPath { get; private set; }

    /// <summary>
    /// Records the native build invocation and writes a fake VPK artifact.
    /// </summary>
    /// <param name="repositoryRoot">Repository root supplied by the builder.</param>
    /// <param name="nativeBuildRoot">Native build root supplied by the builder.</param>
    /// <param name="generatedCoreCppRootPath">Generated core root supplied by the builder.</param>
    /// <param name="stagedContentRootPath">Staged content root supplied by the builder.</param>
    /// <param name="cancellationToken">Cancellation token supplied by the builder.</param>
    /// <returns>Path to the fake VPK artifact.</returns>
    public string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath, CancellationToken cancellationToken) {
        WasCalled = true;
        RepositoryRootPath = repositoryRoot;
        GeneratedCoreRootPath = generatedCoreCppRootPath;
        StagedContentRootPath = stagedContentRootPath;
        Assert.True(Directory.Exists(repositoryRoot));
        Assert.True(Directory.Exists(generatedCoreCppRootPath));
        Assert.True(Directory.Exists(stagedContentRootPath));
        Directory.CreateDirectory(nativeBuildRoot);
        string vpkPath = Path.Combine(nativeBuildRoot, "helengine_psvita.vpk");
        File.WriteAllText(vpkPath, "fake vpk");
        return vpkPath;
    }
}
