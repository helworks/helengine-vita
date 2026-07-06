[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$VpkPath,
    [switch]$KeepInstalledTitle
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($VpkPath)) {
    throw "Parameter -VpkPath is required and must point to a .vpk file."
}

if (-not (Test-Path -LiteralPath $VpkPath)) {
    throw "VPK file was not found at '$VpkPath'."
}

$ResolvedVpkPath = (Resolve-Path -LiteralPath $VpkPath).ProviderPath
$VpkItem = Get-Item -LiteralPath $ResolvedVpkPath
if (($VpkItem.Attributes -band [IO.FileAttributes]::Directory) -ne 0) {
    throw "Expected a .vpk file but got directory '$ResolvedVpkPath'."
}

if ($VpkItem.Extension -ine '.vpk') {
    throw "Expected a .vpk file but got '$ResolvedVpkPath'."
}

$Vita3KPath = 'C:\dev\helworks\emus\vita-3k\Vita3K.exe'
if (-not (Test-Path -LiteralPath $Vita3KPath -PathType Leaf)) {
    throw "Vita3K executable was not found at '$Vita3KPath'."
}

$InstalledTitleId = 'HLEN00001'
$InstalledTitlePath = "C:\Users\Helena\AppData\Roaming\Vita3K\Vita3K\ux0\app\$InstalledTitleId"
$InstalledEbootPath = Join-Path -Path $InstalledTitlePath -ChildPath 'eboot.bin'

$GetInstalledTitleFingerprint = {
    param(
        [string]$installedTitlePath
    )

    if (-not (Test-Path -LiteralPath $installedTitlePath -PathType Container)) {
        return $null
    }

    $entries = @(Get-ChildItem -LiteralPath $installedTitlePath -Recurse -File -ErrorAction SilentlyContinue)
    if ($entries.Count -eq 0) {
        return [pscustomobject]@{
            FileCount = 0
            TotalLength = 0L
            LastWriteTimeUtcTicks = 0L
        }
    }

    $totalLength = 0L
    $lastWriteTimeUtcTicks = 0L
    foreach ($entry in $entries) {
        $totalLength += $entry.Length
        $entryLastWriteTimeUtcTicks = $entry.LastWriteTimeUtc.Ticks
        if ($entryLastWriteTimeUtcTicks -gt $lastWriteTimeUtcTicks) {
            $lastWriteTimeUtcTicks = $entryLastWriteTimeUtcTicks
        }
    }

    return [pscustomobject]@{
        FileCount = $entries.Count
        TotalLength = $totalLength
        LastWriteTimeUtcTicks = $lastWriteTimeUtcTicks
    }
}

$WaitForInstalledTitleExtraction = {
    param(
        [System.Diagnostics.Process]$installProcess,
        [string]$installedTitlePath,
        [string]$installedEbootPath
    )

    $installDeadline = (Get-Date).AddSeconds(60)
    $previousFingerprint = $null
    $stableFingerprintSamples = 0

    while ((Get-Date) -lt $installDeadline) {
        if ($installProcess.HasExited) {
            return Test-Path -LiteralPath $installedEbootPath -PathType Leaf
        }

        if (Test-Path -LiteralPath $installedEbootPath -PathType Leaf) {
            $currentFingerprint = & $GetInstalledTitleFingerprint $installedTitlePath
            if ($null -ne $currentFingerprint) {
                if ($null -ne $previousFingerprint `
                    -and $previousFingerprint.FileCount -eq $currentFingerprint.FileCount `
                    -and $previousFingerprint.TotalLength -eq $currentFingerprint.TotalLength `
                    -and $previousFingerprint.LastWriteTimeUtcTicks -eq $currentFingerprint.LastWriteTimeUtcTicks) {
                    $stableFingerprintSamples++
                } else {
                    $stableFingerprintSamples = 0
                }

                $previousFingerprint = $currentFingerprint
                if ($stableFingerprintSamples -ge 4) {
                    return $true
                }
            }
        }

        Start-Sleep -Milliseconds 500
    }

    return Test-Path -LiteralPath $installedEbootPath -PathType Leaf
}

$RunningVita3KProcesses = @(Get-Process -Name 'Vita3K' -ErrorAction SilentlyContinue)
if ($RunningVita3KProcesses.Count -gt 0) {
    $RunningVita3KProcesses | Stop-Process -Force
}

$DeletedInstalledTitle = $false
$InstalledFromVpk = $false
# Unless -KeepInstalledTitle is passed, refresh the default installed title before launching the installed app.
if (-not $KeepInstalledTitle) {
    $DeleteOutput = & $Vita3KPath -d $InstalledTitleId 2>&1
    $DeleteOutputText = ($DeleteOutput | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -and -not ($DeleteOutputText -imatch 'not installed') -and -not ($DeleteOutputText -imatch 'not in \\{\\}')) {
        if (Test-Path -LiteralPath $InstalledTitlePath) {
            Remove-Item -LiteralPath $InstalledTitlePath -Recurse -Force
        } else {
            throw "Failed to delete installed title $InstalledTitleId.`n$DeleteOutputText"
        }
    }

    $DeletedInstalledTitle = $true
}

$ShouldInstallFromVpk = $DeletedInstalledTitle -or -not (Test-Path -LiteralPath $InstalledEbootPath -PathType Leaf)
if ($ShouldInstallFromVpk) {
    $InstallProcess = Start-Process -FilePath $Vita3KPath -ArgumentList @($ResolvedVpkPath) -PassThru -WindowStyle Normal
    $InstalledFromVpk = & $WaitForInstalledTitleExtraction $InstallProcess $InstalledTitlePath $InstalledEbootPath

    if (-not (Test-Path -LiteralPath $InstalledEbootPath -PathType Leaf)) {
        if (-not $InstallProcess.HasExited) {
            Stop-Process -Id $InstallProcess.Id -Force
        }

        throw "Installed title '$InstalledTitleId' was not materialized after opening '$ResolvedVpkPath'."
    }

    if (-not $InstallProcess.HasExited) {
        Stop-Process -Id $InstallProcess.Id -Force
    }
}

Start-Process -FilePath $Vita3KPath -ArgumentList @('-r', $InstalledTitleId, '-S', 'eboot.bin') -WindowStyle Normal

Write-Output "Launched Vita3K: $Vita3KPath"
Write-Output "VPK: $ResolvedVpkPath"
if ($DeletedInstalledTitle) {
    Write-Output "Deleted installed title: $InstalledTitleId"
} else {
    Write-Output "Preserved installed title: $InstalledTitleId"
}
if ($InstalledFromVpk) {
    Write-Output "Installed title from VPK: $InstalledTitleId"
}
