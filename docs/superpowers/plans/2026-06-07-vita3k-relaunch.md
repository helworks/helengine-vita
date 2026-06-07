# PS Vita Vita3K Relaunch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Force-stop any running Vita3K instances before the PS Vita launcher deletes the installed title and launches a fresh VPK.

**Architecture:** Keep the current launcher contract intact and insert one explicit emulator-cleanup block between path validation and title deletion. Lock the behavior with a source audit so future script edits cannot silently remove the forced relaunch cleanup.

**Tech Stack:** PowerShell launcher script, xUnit source-audit tests, git

---

### Task 1: Lock the Relaunch Contract in Source Audits

**Files:**
- Modify: `builder.tests/Vita3KLauncherScriptAuditTests.cs`
- Test: `builder.tests/Vita3KLauncherScriptAuditTests.cs`

- [ ] **Step 1: Write the failing source-audit assertion**

Add one new audit test to `builder.tests/Vita3KLauncherScriptAuditTests.cs`:

```csharp
    /// <summary>
    /// Verifies the launcher force-stops existing Vita3K processes before it deletes the installed title or launches the new VPK.
    /// </summary>
    [Fact]
    public void Script_whenLaunchingVita3K_forceStopsExistingProcessesBeforeRelaunch() {
        string scriptPath = PsVitaRepositoryPathResolver.ResolvePath("tools", "launch-vita3k.ps1");
        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("Get-Process -Name 'Vita3K'", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Stop-Process -Force", scriptSource, StringComparison.Ordinal);
        Assert.True(
            scriptSource.IndexOf("Stop-Process -Force", StringComparison.Ordinal) < scriptSource.IndexOf("Start-Process -FilePath $Vita3KPath", StringComparison.Ordinal),
            "The launcher must stop existing Vita3K processes before starting the new instance.");
    }
```

- [ ] **Step 2: Run the focused audit to verify it fails**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --filter "FullyQualifiedName~Vita3KLauncherScriptAuditTests" -clp:ErrorsOnly
```

Expected: FAIL because the current script does not reference `Get-Process -Name 'Vita3K'` or `Stop-Process -Force`.

- [ ] **Step 3: Commit the failing test only if working in an isolated task branch**

```bash
git add builder.tests/Vita3KLauncherScriptAuditTests.cs
git commit -m "test: lock Vita3K relaunch cleanup contract"
```

If working directly in a shared branch with unimplemented failures, skip this commit and continue immediately to Task 2.

### Task 2: Implement Forced Vita3K Cleanup in the Launcher Script

**Files:**
- Modify: `tools/launch-vita3k.ps1`
- Modify: `builder.tests/Vita3KLauncherScriptAuditTests.cs`
- Test: `builder.tests/Vita3KLauncherScriptAuditTests.cs`

- [ ] **Step 1: Add the forced process-cleanup block**

Insert this block in `tools/launch-vita3k.ps1` after the `Vita3K.exe` path validation and before the `$DeletedInstalledTitle = $false` line:

```powershell
$RunningVita3KProcesses = @(Get-Process -Name 'Vita3K' -ErrorAction SilentlyContinue)
if ($RunningVita3KProcesses.Count -gt 0) {
    $RunningVita3KProcesses | Stop-Process -Force
}
```

The surrounding script should then look like this:

```powershell
$Vita3KPath = 'C:\dev\helworks\emus\vita-3k\Vita3K.exe'
if (-not (Test-Path -LiteralPath $Vita3KPath -PathType Leaf)) {
    throw "Vita3K executable was not found at '$Vita3KPath'."
}

$RunningVita3KProcesses = @(Get-Process -Name 'Vita3K' -ErrorAction SilentlyContinue)
if ($RunningVita3KProcesses.Count -gt 0) {
    $RunningVita3KProcesses | Stop-Process -Force
}

$DeletedInstalledTitle = $false
if (-not $KeepInstalledTitle) {
    $DeleteOutput = & $Vita3KPath -d HLEN00001 2>&1
```

- [ ] **Step 2: Run the focused audit to verify it passes**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --filter "FullyQualifiedName~Vita3KLauncherScriptAuditTests" -clp:ErrorsOnly
```

Expected: PASS with all Vita3K launcher source audits green.

- [ ] **Step 3: Run a no-build recheck for the same audit set**

Run:

```powershell
dotnet test 'C:\dev\helworks\helengine-psvita\builder.tests\helengine.psvita.builder.tests.csproj' --no-build --filter "FullyQualifiedName~Vita3KLauncherScriptAuditTests" -clp:ErrorsOnly
```

Expected: PASS again, confirming the compiled test assembly and source assertions both remain stable.

- [ ] **Step 4: Commit the launcher change**

```bash
git add tools/launch-vita3k.ps1 builder.tests/Vita3KLauncherScriptAuditTests.cs
git commit -m "Force-stop Vita3K before relaunch"
```

- [ ] **Step 5: Summarize remaining worktree state**

Run:

```bash
git status --short --branch
```

Expected: the new commit exists and any unrelated pre-existing Vita worktree changes remain untouched.
