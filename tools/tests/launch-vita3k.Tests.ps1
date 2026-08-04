$ScriptPath = Join-Path -Path $PSScriptRoot -ChildPath '..\launch-vita3k.ps1'

Describe 'launch-vita3k.ps1' {
    It 'does not start the installed title after Vita3K already started the VPK' {
        $source = Get-Content -LiteralPath $ScriptPath -Raw

        $source | Should Match 'if \(-not \$InstalledFromVpk\) \{\s*Start-Process -FilePath \$Vita3KPath -ArgumentList @\(''-r'', \$InstalledTitleId, ''-S'', ''eboot\.bin''\)'
    }
}
