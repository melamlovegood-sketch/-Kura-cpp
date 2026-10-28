param([switch]$Demo, [switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceManifest.ps1')
$outputRoot = Join-Path $repoRoot 'build'
$pointerPath = Join-Path $outputRoot 'current.json'
$source = Get-KuraSourceManifest $repoRoot
$pointer = if (Test-Path -LiteralPath $pointerPath) { Get-Content -LiteralPath $pointerPath -Raw -Encoding utf8 | ConvertFrom-Json } else { $null }
if (-not $pointer -or $pointer.sourceId -ne $source.id) {
    if ($CheckOnly) { throw 'The current release does not match the source tree.' }
    Write-Host 'Source updated. Building and verifying the latest version first...'
    & (Join-Path $PSScriptRoot 'Build-Unified.ps1')
    $pointer = Get-Content -LiteralPath $pointerPath -Raw -Encoding utf8 | ConvertFrom-Json
}
$release = [IO.Path]::GetFullPath((Join-Path $outputRoot $pointer.release))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $outputRoot 'releases')) + [IO.Path]::DirectorySeparatorChar
if (-not $release.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid release location.' }
$manifest = Get-Content -LiteralPath (Join-Path $release 'build-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
$executable = Join-Path $release 'KuraCpp.exe'
if ($manifest.sourceId -ne (Get-KuraSourceManifest $repoRoot).id) { throw 'Source changed while preparing to launch. Try again after editing has finished.' }
if ((Get-FileHash -LiteralPath $executable).Hash -ne $manifest.executableSha256) { throw 'The executable does not match the verified build.' }
if ($CheckOnly) { Write-Host "Current release verified: $release"; return }
if ($Demo) { Start-Process -FilePath $executable -ArgumentList '--demo' -WorkingDirectory $release }
else { Start-Process -FilePath $executable -WorkingDirectory $release }
