param(
    [string]$InnoSetupCompiler = "$env:LOCALAPPDATA/KuraBuild/tools/InnoSetup/ISCC.exe",
    [string]$RuntimeLicenseDirectory = 'C:/msys64/ucrt64/share/licenses',
    [switch]$IncludeTrialAI
)
$ErrorActionPreference = 'Stop'
$installerRepoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'SourceManifest.ps1')
$installerBuildRoot = Join-Path $installerRepoRoot 'build'
$installerLock = $null
try {
    $installerLock = [IO.File]::Open((Join-Path $installerBuildRoot 'unified-build.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    & (Join-Path $PSScriptRoot 'Start-Current.ps1') -CheckOnly
    & (Join-Path $PSScriptRoot 'Test-LauncherCompatibility.ps1') -Root $installerRepoRoot
    if (-not (Test-Path -LiteralPath $InnoSetupCompiler -PathType Leaf)) { throw "Install Inno Setup 6 and specify -InnoSetupCompiler: $InnoSetupCompiler" }
    $installerPointer = Get-Content -LiteralPath (Join-Path $installerBuildRoot 'current.json') -Raw -Encoding utf8 | ConvertFrom-Json
    $installerRelease = [IO.Path]::GetFullPath((Join-Path $installerBuildRoot $installerPointer.release))
    $installerManifest = Get-Content -LiteralPath (Join-Path $installerRelease 'build-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
    if ($installerManifest.trialAIIncluded -and -not $IncludeTrialAI) { throw 'This release includes shared AI credentials. Explicit -IncludeTrialAI is required to redistribute it.' }
    $installerTestLog = Get-Content -LiteralPath (Join-Path $installerRelease 'verified-tests.log') -Raw -Encoding utf8
    $installerPassed = [regex]::Matches($installerTestLog, '(?m)^Test Passed\.\r?$').Count
    if ($installerPassed -le 0 -or $installerPassed -ne $installerManifest.passedTestGroups) { throw 'The complete passing regression log is required.' }
    foreach ($installerDependency in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll','Qt6Network.dll','zlib1.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll')) {
        if (-not (Test-Path -LiteralPath (Join-Path $installerRelease $installerDependency) -PathType Leaf)) { throw "Missing runtime: $installerDependency" }
    }
    $installerStamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $installerVariant = if ($installerManifest.trialAIIncluded) { '-AITrial' } else { '' }
    $installerName = "Kura-$($installerManifest.version)$installerVariant-Setup-windows-x64-$installerStamp"
    $installerOutput = Join-Path $installerBuildRoot "installers/$installerName"
    $installerPayload = Join-Path $installerOutput 'payload'
    New-Item -ItemType Directory -Path $installerPayload | Out-Null
    Get-ChildItem -LiteralPath $installerRelease -Filter '*.dll' -File | Copy-Item -Destination $installerPayload
    foreach ($installerFile in @('KuraCpp.exe','qt.conf','Demo.cmd','build-manifest.json','verified-tests.log')) {
        Copy-Item -LiteralPath (Join-Path $installerRelease $installerFile) -Destination $installerPayload
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'TESTER_README.txt') -Destination (Join-Path $installerPayload '使用说明.txt')
    Copy-Item -LiteralPath (Join-Path $installerRepoRoot 'resources/icons/kura.ico') -Destination (Join-Path $installerPayload 'kura.ico')
    foreach ($installerFolder in @('platforms','sqldrivers','styles','imageformats','generic','networkinformation','tls','docs','examples','licenses')) {
        $installerFolderPath = Join-Path $installerRelease $installerFolder
        if (Test-Path -LiteralPath $installerFolderPath) { Copy-Item -LiteralPath $installerFolderPath -Destination $installerPayload -Recurse }
    }
    if ($IncludeTrialAI -and $installerManifest.trialAIIncluded) {
        Copy-Item -LiteralPath (Join-Path $installerRelease 'kura-trial.json') -Destination $installerPayload
        Copy-Item -LiteralPath (Join-Path $installerRelease '试用说明.txt') -Destination $installerPayload
        $installerReadmePath = Join-Path $installerPayload '使用说明.txt'
        $installerReadme = Get-Content -LiteralPath $installerReadmePath -Raw -Encoding utf8
        $installerReadme = $installerReadme.Replace('AI 功能需在“设置 → AI 服务”配置自己的 API Key，页面内有申请教程。本常规测试包不提供共享 Key。','本测试包已提供 Kura AI，新用户默认使用试用，无需填写 API Key；可在“设置 → AI 服务”切换自己的 Key。试用由项目方提供，仅限指定测试用户使用，请勿公开转发。')
        Set-Content -LiteralPath $installerReadmePath -Value $installerReadme -Encoding utf8
    }
    $installerLicenseTarget = Join-Path $installerPayload 'licenses/runtime'
    Copy-Item -LiteralPath $RuntimeLicenseDirectory -Destination $installerLicenseTarget -Recurse
    $installerQtLicense = Join-Path $installerLicenseTarget 'qt6-base/LGPL-3.0-only.txt'
    New-Item -ItemType Directory -Path (Split-Path -Parent $installerQtLicense) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses/Qt-LGPL-3.0-only.txt') -Destination $installerQtLicense
    $installerUnexpected = @(Get-ChildItem -LiteralPath $installerPayload -Recurse -File | Where-Object { $_.Name -match '\.(sqlite($|-)|db$|ini$)' })
    if ($installerUnexpected.Count) { throw 'Personal data or settings were found in the installer payload.' }
    $installerPayloadFiles = @(Get-ChildItem -LiteralPath $installerPayload -Recurse -File | ForEach-Object {
        [pscustomobject]@{ path = $_.FullName.Substring($installerPayload.Length + 1); sha256 = (Get-FileHash -LiteralPath $_.FullName).Hash }
    })
    & $InnoSetupCompiler "/DAppVersion=$($installerManifest.version)" "/DPayload=$installerPayload" "/DInstallerOutput=$installerOutput" "/DInstallerName=$installerName" (Join-Path $PSScriptRoot 'Kura-Setup.iss')
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }
    foreach ($installerEntry in $installerPayloadFiles) {
        if ((Get-FileHash -LiteralPath (Join-Path $installerPayload $installerEntry.path)).Hash -ne $installerEntry.sha256) { throw 'Installer payload changed during compilation.' }
    }
    if ((Get-KuraSourceManifest $installerRepoRoot).id -ne $installerManifest.sourceId) { throw 'Source changed during packaging; do not distribute this installer.' }
    $installerExecutable = Join-Path $installerOutput "$installerName.exe"
    [ordered]@{
        version = $installerManifest.version
        sourceId = $installerManifest.sourceId
        passedTestGroups = $installerPassed
        installer = $installerExecutable
        sha256 = (Get-FileHash -LiteralPath $installerExecutable).Hash
        trialAIIncluded = [bool]$installerManifest.trialAIIncluded
        files = $installerPayloadFiles
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $installerOutput 'installer-manifest.json') -Encoding utf8
    Write-Host "Verified installer: $installerExecutable"
} finally { if ($installerLock) { $installerLock.Dispose() } }
