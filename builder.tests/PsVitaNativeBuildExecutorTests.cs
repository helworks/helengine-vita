using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the PS Vita native build uses a persistent object root and fresh package staging.
/// </summary>
public sealed class PsVitaNativeBuildExecutorTests {
    /// <summary>
    /// Ensures Docker receives separate mounts for cached native files and the current VPK output.
    /// </summary>
    [Fact]
    public void CreateNativeBuildArguments_whenCacheAndPackageRootsAreProvided_separatesNativeAndPackageOutputs() {
        IReadOnlyList<string> arguments = PsVitaNativeBuildExecutor.CreateNativeBuildArguments(
            "C:/repo/psvita",
            "C:/generated/core",
            "C:/working/staged-content",
            "C:/project/cache/build/psvita/debug/native",
            "C:/working/native/package-output",
            "Test Title");

        Assert.Contains("C:/repo/psvita:/workspace", arguments);
        Assert.Contains("C:/generated/core:/generated-core", arguments);
        Assert.Contains("C:/working/staged-content:/workspace/cooked", arguments);
        Assert.Contains("C:/project/cache/build/psvita/debug/native:/native-cache", arguments);
        Assert.Contains("C:/working/native/package-output:/package-output", arguments);
        Assert.Contains("BUILD_DIR=/native-cache", arguments);
        Assert.Contains("PACKAGE_DIR=/package-output", arguments);
        Assert.Contains("SOURCE_DIR=/workspace", arguments);
        Assert.Contains("HELENGINE_PSVITA_GAME_TITLE=Test Title", arguments);
        Assert.DoesNotContain("clean", arguments);
    }
}
