param([string]$Root = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'

# The .cmd entry points use Windows PowerShell 5.1, even when development uses 7.
if ($PSVersionTable.PSVersion.Major -gt 5) {
    $windowsPowerShell = Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
    & $windowsPowerShell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Root $Root
    if ($LASTEXITCODE -ne 0) { throw 'Windows PowerShell launcher compatibility check failed.' }
    return
}

$checked = 0
foreach ($script in Get-ChildItem -LiteralPath (Join-Path $Root 'scripts') -Filter '*.ps1' -File -Recurse) {
    $bytes = [IO.File]::ReadAllBytes($script.FullName)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191
    $hasNonAscii = $false
    foreach ($byte in $bytes) { if ($byte -gt 127) { $hasNonAscii = $true; break } }
    if ($hasNonAscii -and -not $hasBom) {
        throw "Save $($script.Name) as UTF-8 with BOM for Windows PowerShell 5.1."
    }
    $tokens = $null
    $parseErrors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw ($script.Name + ': ' + ($parseErrors.Message -join '; ')) }
    $checked++
}
Write-Host "Windows PowerShell 5.1: $checked launcher/build scripts passed encoding and syntax checks."
