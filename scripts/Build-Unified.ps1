param(
    [string]$QtRoot = 'C:/msys64/ucrt64',
    [ValidateRange(1,32)][int]$Parallel = 4,
    [switch]$IncludeTrialAI,
    [string]$TrialAISettingsPath = ''
)
$ErrorActionPreference = 'Stop'
if ($TrialAISettingsPath -and -not $IncludeTrialAI) { throw '-TrialAISettingsPath requires -IncludeTrialAI.' }
if ($TrialAISettingsPath) { $TrialAISettingsPath = (Resolve-Path -LiteralPath $TrialAISettingsPath).Path }
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceManifest.ps1')
$outputRoot = Join-Path $repoRoot 'build'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$buildLock = $null
try {
    try { $buildLock = [IO.File]::Open((Join-Path $outputRoot 'unified-build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
    catch { throw 'Another unified build is running. Wait for it to finish before trying again.' }
    & (Join-Path $PSScriptRoot 'Test-LauncherCompatibility.ps1') -Root $repoRoot
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
    $buildDirectory = Join-Path $aliasParent $(if ($IncludeTrialAI) { 'build-trial-ai' } else { 'build' })
    $source = Get-KuraSourceManifest $repoRoot
    $verifiedBase = $null
    # A trial edition may reuse the exact current binary only when the same
    # source already passed every recorded test. Source changes still rebuild.
    $existingPointer = Join-Path $outputRoot 'current.json'
    if ($IncludeTrialAI -and (Test-Path -LiteralPath $existingPointer)) {
        try {
            $previous = Get-Content -LiteralPath $existingPointer -Raw -Encoding utf8 | ConvertFrom-Json
            $previousRoot = [IO.Path]::GetFullPath((Join-Path $outputRoot $previous.release))
            $releaseBoundary = [IO.Path]::GetFullPath((Join-Path $outputRoot 'releases')) + [IO.Path]::DirectorySeparatorChar
            if ($previous.sourceId -eq $source.id -and $previousRoot.StartsWith($releaseBoundary,[StringComparison]::OrdinalIgnoreCase)) {
                $previousManifest = Get-Content -LiteralPath (Join-Path $previousRoot 'build-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
                $previousLog = [IO.File]::ReadAllBytes((Join-Path $previousRoot 'verified-tests.log'))
                $loggedGroups = [regex]::Matches([Text.Encoding]::UTF8.GetString($previousLog),'(?m)^Test Passed\.\r?$').Count
                if ($previousManifest.sourceId -eq $source.id -and $previousManifest.passedTestGroups -gt 0 -and $loggedGroups -eq $previousManifest.passedTestGroups -and
                    (Get-FileHash -LiteralPath (Join-Path $previousRoot 'KuraCpp.exe')).Hash -eq $previousManifest.executableSha256 -and
                    (Get-FileHash -LiteralPath (Join-Path $previousRoot 'Qt6Core.dll')).Hash -eq (Get-FileHash -LiteralPath (Join-Path $qtBin 'Qt6Core.dll')).Hash) {
                    $verifiedBase = $previousRoot
                    $verifiedTestLog = $previousLog
                    $passedTestGroups = [int]$previousManifest.passedTestGroups
                    $executableSource = Join-Path $verifiedBase 'KuraCpp.exe'
                    Write-Host "Reusing the current binary and $passedTestGroups verified test groups for the trial edition."
                }
            }
        } catch { Write-Host 'The current release cannot be reused; performing a full build and regression.' }
    }
    if (-not $verifiedBase) {
        Write-Host 'Building the unified source tree...'
        & $cmake -S $sourceAlias -B $buildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_PREFIX_PATH=$QtRoot" '-DBUILD_TESTING=ON'
        if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
        & $cmake --build $buildDirectory --parallel $Parallel
        if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. The current release was not changed.' }
        & $ctest --test-dir $buildDirectory --output-on-failure --parallel 1
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed. The current release was not changed.' }
        # CTest --show-only rewrites LastTest.log. Keep the completed regression
        # log before enumerating tests so the package contains actual results.
        $verifiedTestLog = [IO.File]::ReadAllBytes((Join-Path $buildDirectory 'Testing/Temporary/LastTest.log'))
        $testListing = (& $ctest --test-dir $buildDirectory --show-only=json-v1 | Out-String) | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0) { throw 'Could not read test results.' }
        $passedTestGroups = @($testListing.tests).Count
        $executableSource = Join-Path $buildDirectory 'KuraCpp.exe'
    }
    if ((Get-KuraSourceManifest $repoRoot).id -ne $source.id) { throw 'Source files changed during verification. Run the build again; no release was promoted.' }

    $runtime = $verifiedBase
    foreach ($candidate in @('build/secondary-focused-run','build/planning-focused-run','build')) {
        if ($runtime) { break }
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
    $variant = if ($IncludeTrialAI) { 'trial-' } else { '' }
    $label = "KuraCpp-$version-unified-$variant$stamp-windows-x64"
    $releaseRoot = Join-Path $outputRoot 'releases'
    New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
    $destination = Join-Path $releaseRoot $label
    New-Item -ItemType Directory -Path $destination | Out-Null
    Get-ChildItem -LiteralPath $runtime -Filter '*.dll' -File | Copy-Item -Destination $destination
    foreach ($plugin in @('platforms','sqldrivers','styles','imageformats','generic','networkinformation','tls')) {
        $pluginSource = Join-Path $runtime $plugin
        if (Test-Path -LiteralPath $pluginSource) { Copy-Item -LiteralPath $pluginSource -Destination $destination -Recurse }
    }
    Copy-Item -LiteralPath $executableSource -Destination $destination
    if ($IncludeTrialAI) {
        # Export only the explicitly requested AI credential, never the owner's
        # database or machine-bound settings file. Do not print the credential.
        $trialPath = Join-Path $destination 'kura-trial.json'
        $exportArguments = @('--export-trial-ai', ('"' + $trialPath + '"'))
        if ($TrialAISettingsPath) { $exportArguments += @('--trial-ai-settings', ('"' + $TrialAISettingsPath + '"')) }
        $export = Start-Process -FilePath (Join-Path $destination 'KuraCpp.exe') -ArgumentList $exportArguments -WorkingDirectory $destination -WindowStyle Hidden -Wait -PassThru
        if ($export.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $trialPath)) { throw 'Trial AI export failed. The current release was not changed.' }
        Set-Content -LiteralPath (Join-Path $destination '试用说明.txt') -Encoding utf8 -Value @(
            'Kura Windows 小组测试版',
            '完整解压后打开 KuraCpp.exe。正式模式会保留自己的记录；Demo.cmd 使用临时示例数据，退出后删除。',
            '新用户默认选择 Kura AI 试用，无需注册或填写密钥，可直接确认下一步。已有用户可在 设置 → AI 服务 中启用；不会覆盖已明确选择的个人配置。启动和保存本身不会发起 AI 请求。',
            '试用期间由项目提供 AI 服务；点击 AI 功能时，相关文字或截图会发送给配置的服务商。',
            '试用暂不可用或结束后，可以在同一位置切换“使用自己的 API Key”。本地记账、清单等功能仍可使用。',
            '此包含本轮测试的共享凭据，仅供结对小组使用，请勿公开转发。结束时由项目方在服务商后台撤销。'
        )
    }
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
        sourceId=$source.id; sourceIdFormat='sha256-ordinal-path-v1'; files=$source.files; passedTestGroups=$passedTestGroups
        executableSha256=(Get-FileHash -LiteralPath (Join-Path $destination 'KuraCpp.exe')).Hash
        trialAIIncluded=[bool]$IncludeTrialAI
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $destination 'build-manifest.json') -Encoding utf8
    [IO.File]::WriteAllBytes((Join-Path $destination 'verified-tests.log'),$verifiedTestLog)
    Compress-Archive -LiteralPath $destination -DestinationPath "$destination.zip" -CompressionLevel Optimal
    if ((Get-KuraSourceManifest $repoRoot).id -ne $source.id) { throw 'Source files changed while packaging. No release was promoted; run the build again.' }
    $pointer = Join-Path $outputRoot 'current.json'
    @{ release="releases/$label"; sourceId=$source.id } | ConvertTo-Json | Set-Content -LiteralPath "$pointer.tmp" -Encoding utf8
    Move-Item -LiteralPath "$pointer.tmp" -Destination $pointer -Force
    Write-Host "Verified $passedTestGroups test groups. Current release: $destination"
} finally { if ($buildLock) { $buildLock.Dispose() } }
