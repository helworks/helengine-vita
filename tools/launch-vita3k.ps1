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

$DeletedInstalledTitle = $false
if (-not $KeepInstalledTitle) {
    $DeleteOutput = & $Vita3KPath -d HLEN00001 2>&1
    $DeleteOutputText = ($DeleteOutput | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -and -not ($DeleteOutputText -imatch 'not installed')) {
        throw "Failed to delete installed title HLEN00001.`n$DeleteOutputText"
    }

    $DeletedInstalledTitle = $true
}

Start-Process -FilePath $Vita3KPath -ArgumentList @($ResolvedVpkPath) -WindowStyle Normal

Write-Output "Launched Vita3K: $Vita3KPath"
Write-Output "VPK: $ResolvedVpkPath"
if ($DeletedInstalledTitle) {
    Write-Output "Deleted installed title: HLEN00001"
} else {
    Write-Output "Preserved installed title: HLEN00001"
}
