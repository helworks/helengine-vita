using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita CMake source so staged cooked content is packaged into the VPK for app0 runtime access.
/// </summary>
public sealed class PsVitaCMakeSourceAuditTests {
    /// <summary>
    /// Verifies the PS Vita VPK build includes staged cooked files as additional VPK resources.
    /// </summary>
    [Fact]
    public void CMake_whenCookedContentIsPresent_packagesCookedFilesIntoTheVpk() {
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("file(GLOB_RECURSE HELENGINE_PSVITA_COOKED_FILES", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("list(APPEND HELENGINE_PSVITA_VPK_FILE_ARGS", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("${HELENGINE_PSVITA_VPK_FILE_ARGS}", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the VPK includes an application icon so emulator and Vita shell metadata loading can complete before the title starts.
    /// </summary>
    [Fact]
    public void CMake_whenCreatingTheVpk_packagesTheApplicationIcon() {
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("sce_sys/icon0-indexed.png", cmakeSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the packaged application icon uses the indexed PNG representation required by the Vita installer.
    /// </summary>
    [Fact]
    public void CMake_whenCreatingTheVpk_packagesAnIndexedApplicationIcon() {
        string iconPath = PsVitaRepositoryPathResolver.ResolvePath("sce_sys", "icon0-indexed.png");
        byte[] iconHeader = File.ReadAllBytes(iconPath);

        Assert.True(iconHeader.Length >= 26, "Expected the packaged icon to contain a complete PNG IHDR header.");
        Assert.Equal((byte)8, iconHeader[24]);
        Assert.Equal((byte)3, iconHeader[25]);
    }

    /// <summary>
    /// Verifies the PS Vita link step pulls in pthread when the generated core uses the shared std::thread runtime surface.
    /// </summary>
    [Fact]
    public void CMake_whenGeneratedCoreUsesStdThread_linksPthreadSupport() {
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string cmakeSource = File.ReadAllText(cmakePath);

        Assert.Contains("target_link_libraries(${PROJECT_NAME} PRIVATE", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("pthread", cmakeSource, StringComparison.Ordinal);
    }
}
