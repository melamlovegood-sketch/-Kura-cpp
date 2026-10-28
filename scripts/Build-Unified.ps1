param(
    [string]$QtRoot = 'C:/msys64/ucrt64',
    [ValidateRange(1,32)][int]$Parallel = 4
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceManifest.ps1')
$outputRoot = Join-Path $repoRoot 'build'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$buildLock = $null
try {
    try { $buildLock = [IO.File]::Open((Join-Path $outputRoot 'unified-build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
    catch { throw 'Another unified build is running. Wait for it to finish before trying again.' }
    $qtBin = Join-Path $QtRoot 'bin'
    $env:PATH = "$qtBin;$env:PATH"
    $env:QT_PLUGIN_PATH = Join-Path $QtRoot 'share/qt6/plugins'
    $cmake = Join-Path $qtBin 'cmake.exe'
    $ctest = Join-Path $qtBin 'ctest.exe'
    if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake is missing: $cmake" }

    # This is a directory junction to the one source tree, never a copied checkout.
    $aliasParent = Join-Path $env:LOCALAPPDATA 'KuraBuild'
    New-Item -ItemType Directory -Path $aliasParent -Force | Out-Null
    $sourceAlias = Join-Path $aliasParent 'source'
    if (Test-Path -LiteralPath $sourceAlias) {
        $aliasItem = Get-Item -LiteralPath $sourceAlias
        if ($aliasItem.LinkType -ne 'Junction' -or [IO.Path]::GetFullPath($aliasItem.Target).TrimEnd('\') -ne [IO.Path]::GetFullPath($repoRoot).TrimEnd('\')) {
            throw "Build alias points to another source tree: $sourceAlias"
        }
    } else { New-Item -ItemType Junction -Path $sourceAlias -Target $repoRoot | Out-Null }
    $buildDirectory = Join-Path $aliasParent 'build'
    $source = Get-KuraSourceManifest $repoRoot
    Write-Host 'Building the unified source tree...'
    & $cmake -S $sourceAlias -B $buildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_PREFIX_PATH=$QtRoot" '-DBUILD_TESTING=ON'
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $buildDirectory --parallel $Parallel
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. The current release was not changed.' }
    & $ctest --test-dir $buildDirectory --output-on-failure --parallel 1
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed. The current release was not changed.' }
    $testListing = (& $ctest --test-dir $buildDirectory --show-only=json-v1 | Out-String) | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Could not read test results.' }
    if ((Get-KuraSourceManifest $repoRoot).id -ne $source.id) { throw 'Source files changed during verification. Run the build again; no release was promoted.' }

    $runtime = $null
    foreach ($candidate in @('build/secondary-focused-run','build/planning-focused-run','build')) {
        $directory = Join-Path $repoRoot $candidate
        if ((Test-Path -LiteralPath (Join-Path $directory 'Qt6Core.dll')) -and (Test-Path -LiteralPath (Join-Path $directory 'platforms/qwindows.dll'))) {
            if ((Get-FileHash -LiteralPath (Join-Path $directory 'Qt6Core.dll')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $qtBin 'Qt6Core.dll')).Hash) { $runtime=$directory; break }
        }
    }
    if (-not $runtime) { throw 'No matching portable Qt runtime found. Preserve build/secondary-focused-run or build/platforms and the runtime DLLs.' }
    $versionText = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
    if ($versionText -notmatch 'project\(KuraCpp VERSION ([0-9.]+)') { throw 'Application version not found.' }
    $version = $Matches[1]
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $label = "KuraCpp-$version-unified-$stamp-windows-x64"
    $releaseRoot = Join-Path $outputRoot 'releases'
    New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
    $destination = Join-Path $releaseRoot $label
    New-Item -ItemType Directory -Path $destination | Out-Null
    Get-ChildItem -LiteralPath $runtime -Filter '*.dll' -File | Copy-Item -Destination $destination
    foreach ($plugin in @('platforms','sqldrivers','styles','imageformats','generic','networkinformation','tls')) {
        $pluginSource = Join-Path $runtime $plugin
        if (Test-Path -LiteralPath $pluginSource) { Copy-Item -LiteralPath $pluginSource -Destination $destination -Recurse }
    }
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'KuraCpp.exe') -Destination $destination
    Copy-Item -LiteralPath (Join-Path $repoRoot 'docs') -Destination $destination -Recurse
    Copy-Item -LiteralPath (Join-Path $repoRoot 'examples') -Destination $destination -Recurse
    Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $destination
    $licenses = Join-Path $destination 'licenses/fonts'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'resources/fonts') -Filter '*-OFL.txt' -File | Copy-Item -Destination $licenses
    Set-Content -LiteralPath (Join-Path $destination 'Demo.cmd') -Value '@echo off','start "" "%~dp0KuraCpp.exe" --demo' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $destination 'qt.conf') -Value '[Paths]','Plugins=.' -Encoding ascii
    $manifest = [ordered]@{
        version=$version; builtAt=(Get-Date -Format o); sourceRoot=$repoRoot
        sourceId=$source.id; sourceIdFormat='sha256-ordinal-path-v1'; files=$source.files; passedTestGroups=@($testListing.tests).Count
        executableSha256=(Get-FileHash -LiteralPath (Join-Path $destination 'KuraCpp.exe')).Hash
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'build-manifest.json') -Encoding utf8
    Copy-Item -LiteralPath (Join-Path $buildDirectory 'Testing/Temporary/LastTest.log') -Destination (Join-Path $destination 'verified-tests.log')
    Compress-Archive -LiteralPath $destination -DestinationPath "$destination.zip" -CompressionLevel Optimal
    if ((Get-KuraSourceManifest $repoRoot).id -ne $source.id) { throw 'Source files changed while packaging. No release was promoted; run the build again.' }
    $pointer = Join-Path $outputRoot 'current.json'
    @{ release="releases/$label"; sourceId=$source.id } | ConvertTo-Json | Set-Content -LiteralPath "$pointer.tmp" -Encoding utf8
    Move-Item -LiteralPath "$pointer.tmp" -Destination $pointer -Force
    Write-Host "Verified $(@($testListing.tests).Count) test groups. Current release: $destination"
} finally { if ($buildLock) { $buildLock.Dispose() } }
