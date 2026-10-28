param([switch]$Demo, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceManifest.ps1')
$outputRoot = Join-Path $repoRoot 'build'
$pointerPath = Join-Path $outputRoot 'current.json'
$source = Get-KuraSourceManifest $repoRoot
$pointer = if (Test-Path -LiteralPath $pointerPath) { Get-Content -LiteralPath $pointerPath -Raw -Encoding utf8 | ConvertFrom-Json } else { $null }
if (-not $pointer) {
    if ($CheckOnly) { throw 'No verified current release is available.' }
    Write-Host 'Preparing the first verified release...'
    & (Join-Path $PSScriptRoot 'Build-Unified.ps1')
    $pointer = Get-Content -LiteralPath $pointerPath -Raw -Encoding utf8 | ConvertFrom-Json
}
$release = [IO.Path]::GetFullPath((Join-Path $outputRoot $pointer.release))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $outputRoot 'releases')) + [IO.Path]::DirectorySeparatorChar
if (-not $release.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid release location.' }
$manifest = Get-Content -LiteralPath (Join-Path $release 'build-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
$executable = Join-Path $release 'KuraCpp.exe'
if ($manifest.sourceId -ne $pointer.sourceId) { throw 'The current release pointer does not match its verified manifest.' }
if ((Get-FileHash -LiteralPath $executable).Hash -ne $manifest.executableSha256) { throw 'The executable does not match the verified build.' }
if ($CheckOnly) {
    if ($manifest.sourceId -ne (Get-KuraSourceManifest $repoRoot).id) { throw 'The current release does not match the source tree.' }
    Write-Host "Current release verified: $release"; return
}
if ($manifest.sourceId -ne $source.id) {
    Write-Host 'Source edits are pending. Opening the latest verified release; use the update entry after editing is complete.'
}
if ($Demo) { Start-Process -FilePath $executable -ArgumentList '--demo' -WorkingDirectory $release }
else { Start-Process -FilePath $executable -WorkingDirectory $release }
