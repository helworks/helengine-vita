using System;
using System.IO;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the Vita3K launcher script so the standardized PS Vita emulator entrypoint cannot regress.
/// </summary>
public sealed class Vita3KLauncherScriptAuditTests {
    /// <summary>
    /// Verifies the launcher script exists and exposes the explicit VPK-path contract required by the standardized emulator workflow.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_existsAndRequiresExplicitVpkPath() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");

        Assert.True(File.Exists(scriptPath));

        string scriptSource = File.ReadAllText(scriptPath);
        Assert.Contains("param", scriptSource, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("VpkPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("KeepInstalledTitle", scriptSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the launcher validates the VPK and Vita3K executable paths and uses strict failure behavior before attempting any emulator work.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_validatesPathsAndUsesStrictFailureBehavior() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");
        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("Set-StrictMode -Version Latest", scriptSource, StringComparison.Ordinal);
        Assert.Contains("$ErrorActionPreference = 'Stop'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Test-Path -LiteralPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Vita3K.exe", scriptSource, StringComparison.Ordinal);
        Assert.Contains(".vpk", scriptSource, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Verifies the launcher refreshes the default installed title unless the faster keep-installed mode is explicitly requested.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_deletesDefaultTitleUnlessKeepInstalledIsRequested() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");
        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("HLEN00001", scriptSource, StringComparison.Ordinal);
        Assert.Contains("-KeepInstalledTitle", scriptSource, StringComparison.Ordinal);
        Assert.Contains("-d", scriptSource, StringComparison.Ordinal);
    }
}
