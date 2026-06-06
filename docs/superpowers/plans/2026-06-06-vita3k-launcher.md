# Vita3K Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Standardize PS Vita emulator launch with one strict PowerShell script that launches an explicit `.vpk` path, validates inputs up front, deletes installed title `HLEN00001` by default, and forwards real failures clearly.

**Architecture:** Add a repo-local `tools/launch-vita3k.ps1` script with a narrow contract: required `-VpkPath`, optional `-KeepInstalledTitle`, fixed Vita3K executable path, fail-fast validation, blocking delete invocation, and non-blocking final launch. Keep documentation aligned with the existing standard build script flow in `README.md`.

**Tech Stack:** PowerShell, Vita3K Windows executable invocation, xUnit script-audit tests, README documentation, real local Vita3K launch verification.

---

## File Map

- Create: `builder.tests/Vita3KLauncherScriptAuditTests.cs`
  - Source-level coverage for launcher script contract and validation behavior.
- Create: `tools/launch-vita3k.ps1`
  - Standard Vita3K launcher script.
- Modify: `README.md`
  - Add one launch example paired with the standardized build command.

## Task 1: Lock the launcher contract with failing script audits

**Files:**
- Create: `builder.tests/Vita3KLauncherScriptAuditTests.cs`
- Test: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Write the failing audit that requires the launcher script file and parameters**

```csharp
using System;
using System.IO;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the Vita3K launcher script so the standardized emulator entrypoint cannot regress.
/// </summary>
public sealed class Vita3KLauncherScriptAuditTests {
    /// <summary>
    /// Verifies the launcher script exists and exposes the required VPK-path contract.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_existsAndRequiresExplicitVpkPath() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");

        Assert.True(File.Exists(scriptPath));

        string script = File.ReadAllText(scriptPath);
        Assert.Contains("param", script, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("VpkPath", script, StringComparison.Ordinal);
        Assert.Contains("KeepInstalledTitle", script, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 2: Write the failing audit for validation and strict-mode behavior**

```csharp
    /// <summary>
    /// Verifies the launcher validates the VPK path and Vita3K executable path before launch.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_validatesPathsAndUsesStrictFailureBehavior() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");
        string script = File.ReadAllText(scriptPath);

        Assert.Contains("Set-StrictMode -Version Latest", script, StringComparison.Ordinal);
        Assert.Contains("$ErrorActionPreference = 'Stop'", script, StringComparison.Ordinal);
        Assert.Contains("Test-Path -LiteralPath", script, StringComparison.Ordinal);
        Assert.Contains("Vita3K.exe", script, StringComparison.Ordinal);
        Assert.Contains(".vpk", script, StringComparison.OrdinalIgnoreCase);
    }
```

- [ ] **Step 3: Write the failing audit for default title deletion behavior**

```csharp
    /// <summary>
    /// Verifies the launcher deletes the default title by id unless the keep-installed option is used.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_deletesDefaultTitleUnlessKeepInstalledIsRequested() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");
        string script = File.ReadAllText(scriptPath);

        Assert.Contains("HLEN00001", script, StringComparison.Ordinal);
        Assert.Contains("-KeepInstalledTitle", script, StringComparison.Ordinal);
        Assert.Contains("-d", script, StringComparison.Ordinal);
    }
}
```

- [ ] **Step 4: Run the tests to verify they fail for the right reason**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\vita3k-launcher-red-bin\ -p:BaseIntermediateOutputPath=C:\tmp\vita3k-launcher-red-obj\
```

Expected:

- FAIL in `Vita3KLauncherScriptAuditTests`
- failure should show the launcher script does not exist yet or is missing required contract strings

- [ ] **Step 5: Commit the red tests**

```bash
git add builder.tests/Vita3KLauncherScriptAuditTests.cs
git commit -m "Add Vita3K launcher script audits"
```

## Task 2: Implement the standardized Vita3K launcher script

**Files:**
- Create: `tools/launch-vita3k.ps1`
- Test: `builder.tests/helengine.psvita.builder.tests.csproj`

- [ ] **Step 1: Create the script entrypoint and strict PowerShell setup**

Create `tools/launch-vita3k.ps1` with:

```powershell
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VpkPath,
    [switch]$KeepInstalledTitle
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
```

- [ ] **Step 2: Add explicit path validation helpers inline in the script body**

The script should:

- resolve the VPK path with `Resolve-Path -LiteralPath`
- throw if the VPK path does not exist
- throw if the resolved target is not a file
- throw if the extension is not `.vpk`
- validate the fixed Vita3K executable path:
  - `C:\dev\helworks\emus\vita-3k\Vita3K.exe`

Expected implementation shape:

```powershell
$resolvedVpkPath = (Resolve-Path -LiteralPath $VpkPath).ProviderPath
$vpkItem = Get-Item -LiteralPath $resolvedVpkPath
if ($vpkItem.Extension -ine '.vpk') {
    throw "Expected a .vpk file but got '$resolvedVpkPath'."
}

$vita3kPath = 'C:\dev\helworks\emus\vita-3k\Vita3K.exe'
if (-not (Test-Path -LiteralPath $vita3kPath -PathType Leaf)) {
    throw "Vita3K executable was not found at '$vita3kPath'."
}
```

- [ ] **Step 3: Implement default title deletion with real failure forwarding**

The script should:

- skip delete when `-KeepInstalledTitle` is passed
- otherwise invoke `Vita3K.exe -d HLEN00001`
- allow the known "not installed" case
- throw for other failures

Suggested shape:

```powershell
if (-not $KeepInstalledTitle) {
    $deleteOutput = & $vita3kPath -d HLEN00001 2>&1
    if ($LASTEXITCODE -ne 0 -and -not ($deleteOutput -match 'not installed')) {
        throw "Failed to delete installed title HLEN00001.`n$deleteOutput"
    }
}
```

- [ ] **Step 4: Launch Vita3K with the resolved VPK path**

The script should:

- call `Start-Process -FilePath $vita3kPath -ArgumentList @($resolvedVpkPath)`
- not wait for exit
- print concise success output including:
  - VPK path
  - Vita3K path
  - whether the title was deleted or preserved

- [ ] **Step 5: Run the tests again**

Run:

```powershell
dotnet test builder.tests\helengine.psvita.builder.tests.csproj -v minimal -p:BaseOutputPath=C:\tmp\vita3k-launcher-green-bin\ -p:BaseIntermediateOutputPath=C:\tmp\vita3k-launcher-green-obj\
```

Expected:

- `Vita3KLauncherScriptAuditTests` PASS

- [ ] **Step 6: Commit the launcher implementation**

```bash
git add tools/launch-vita3k.ps1 builder.tests/Vita3KLauncherScriptAuditTests.cs
git commit -m "Add Vita3K launcher script"
```

## Task 3: Document the standard build-and-launch workflow

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add the launcher usage example next to the build example**

Update `README.md` to include:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\launch-vita3k.ps1 `
    -VpkPath C:\dev\helprojs\city\vita-build\helengine_psvita.vpk
```

The README section should show:

1. build with `artifacts/build-platform.ps1`
2. launch that exact output with `tools/launch-vita3k.ps1`

- [ ] **Step 2: Review the README wording for standardization clarity**

Make sure the text communicates:

- the build script is the standard build path
- the launcher script is the standard emulator path
- both scripts operate on explicit user-provided paths

- [ ] **Step 3: Commit the README update**

```bash
git add README.md
git commit -m "Document Vita3K launcher usage"
```

## Task 4: Verify the real local launch flow

**Files:**
- Verify only

- [ ] **Step 1: Build with the standardized build script**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\artifacts\build-platform.ps1 `
    -Project C:\dev\helprojs\city\project.heproj `
    -Platform psvita `
    -Output C:\dev\helprojs\city\vita-build
```

Expected:

- build succeeds
- output VPK exists at `C:\dev\helprojs\city\vita-build\helengine_psvita.vpk`

- [ ] **Step 2: Launch with the new standardized launcher**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\launch-vita3k.ps1 `
    -VpkPath C:\dev\helprojs\city\vita-build\helengine_psvita.vpk
```

Expected:

- `HLEN00001` is refreshed by default
- Vita3K starts from the specified VPK

- [ ] **Step 3: Verify failure handling with an invalid VPK path**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\launch-vita3k.ps1 `
    -VpkPath C:\tmp\missing.vpk
```

Expected:

- script fails before touching Vita3K
- error clearly identifies the missing path

- [ ] **Step 4: Verify the keep-installed fast path**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\launch-vita3k.ps1 `
    -VpkPath C:\dev\helprojs\city\vita-build\helengine_psvita.vpk `
    -KeepInstalledTitle
```

Expected:

- delete step is skipped
- Vita3K still launches the specified VPK path

## Task 5: Final validation and integration

**Files:**
- Verify and commit only

- [ ] **Step 1: Confirm final git status contains only the intended launcher files**

Run:

```bash
git status --short
```

Expected:

- launcher script
- launcher tests
- README update
- no unrelated file changes staged by accident

- [ ] **Step 2: Run a final verification pass**

Minimum required checks:

- `dotnet test builder.tests\helengine.psvita.builder.tests.csproj`
- real `build-platform.ps1` output exists
- `tools/launch-vita3k.ps1` launches the expected VPK path
- invalid-path validation fails correctly

- [ ] **Step 3: Present completion with concrete verification evidence**

Report:

- exact build command used
- exact launcher command used
- whether default delete and keep-installed modes both worked
- any remaining limitations or assumptions
