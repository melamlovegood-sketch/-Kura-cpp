param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
    [string]$LicenseDirectory,
    [ValidatePattern('^[a-z0-9-]*$')][string]$PackageSuffix = ''
)
$ErrorActionPreference='Stop'
$packageRepoRoot=Split-Path -Parent $PSScriptRoot
$packageVersionText=Get-Content -LiteralPath (Join-Path $packageRepoRoot 'CMakeLists.txt') -Raw
if ($packageVersionText -notmatch 'project\(KuraCpp VERSION ([0-9.]+)') { throw 'Cannot determine application version.' }
$packageVersion=$Matches[1]
$packageExecutable=(Resolve-Path -LiteralPath $Executable).Path
$packageRuntime=(Resolve-Path -LiteralPath $RuntimeDirectory).Path
foreach ($requiredFile in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Sql.dll','Qt6Network.dll','zlib1.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageRuntime $requiredFile) -PathType Leaf)) { throw "Missing runtime file: $requiredFile" }
}
$packageOutputRoot=Join-Path $packageRepoRoot 'build/releases'
$packageLabel=if($PackageSuffix) { "-$PackageSuffix" } else { '' }
$packageDestination=Join-Path $packageOutputRoot "KuraCpp-$packageVersion$packageLabel-windows-x64"
$packageZip="$packageDestination.zip"
if ((Test-Path -LiteralPath $packageDestination) -or (Test-Path -LiteralPath $packageZip)) { throw 'Release output already exists; choose a new version or preserve and relocate the previous package first.' }
New-Item -ItemType Directory -Path $packageDestination -Force | Out-Null
foreach ($dll in Get-ChildItem -LiteralPath $packageRuntime -Filter '*.dll' -File) {
    Copy-Item -LiteralPath $dll.FullName -Destination $packageDestination
}
foreach ($pluginDirectory in @('platforms','sqldrivers','styles','imageformats','generic','networkinformation','tls')) {
    $pluginSource=Join-Path $packageRuntime $pluginDirectory
    if (Test-Path -LiteralPath $pluginSource -PathType Container) { Copy-Item -LiteralPath $pluginSource -Destination $packageDestination -Recurse }
}
Copy-Item -LiteralPath $packageExecutable -Destination (Join-Path $packageDestination 'KuraCpp.exe')
Copy-Item -LiteralPath (Join-Path $packageRepoRoot 'docs') -Destination $packageDestination -Recurse
Copy-Item -LiteralPath (Join-Path $packageRepoRoot 'README.md') -Destination $packageDestination
$packageExamples=Join-Path $packageRepoRoot 'examples'
if (Test-Path -LiteralPath $packageExamples -PathType Container) { Copy-Item -LiteralPath $packageExamples -Destination $packageDestination -Recurse }
if ($LicenseDirectory) { Copy-Item -LiteralPath (Resolve-Path -LiteralPath $LicenseDirectory).Path -Destination (Join-Path $packageDestination 'licenses') -Recurse }
$fontLicenseSource=Join-Path $packageRepoRoot 'resources/fonts'
if (Test-Path -LiteralPath $fontLicenseSource -PathType Container) {
    $fontLicenseDestination=Join-Path $packageDestination 'licenses/fonts'
    New-Item -ItemType Directory -Path $fontLicenseDestination -Force | Out-Null
    Get-ChildItem -LiteralPath $fontLicenseSource -Filter '*-OFL.txt' -File | Copy-Item -Destination $fontLicenseDestination
}
Set-Content -LiteralPath (Join-Path $packageDestination 'Demo.cmd') -Value '@echo off', 'start "" "%~dp0KuraCpp.exe" --demo' -Encoding ascii
Set-Content -LiteralPath (Join-Path $packageDestination 'qt.conf') -Value '[Paths]','Plugins=.' -Encoding ascii
$packageReadme=@'
Kura C++ Windows portable runtime

1. Extract the entire folder before launching.
2. KuraCpp.exe starts normal mode with your local saved data.
3. Demo.cmd starts isolated fictional demo data. Normal data is not changed.
4. Keep the DLL files and plugin folders next to the executable.
5. Chinese instructions and the acceptance checklist are in docs/.

This ZIP is an application runtime, not the final course submission archive.
The final submission still needs real names, student ids, personal reports
and teacher/user-group review records from the team.
'@
Set-Content -LiteralPath (Join-Path $packageDestination 'START-HERE.txt') -Value $packageReadme -Encoding utf8
$previewDirectory=Join-Path $packageRepoRoot 'build/preview'
if (Test-Path -LiteralPath $previewDirectory) {
    $packagePreviews=Join-Path $packageDestination 'previews'
    New-Item -ItemType Directory -Path $packagePreviews | Out-Null
    foreach ($preview in Get-ChildItem -LiteralPath $previewDirectory -File | Where-Object { $_.Name -like 'release-*.png' -or $_.Name -like 'shell-*.png' -or $_.Name -like 'workflow-*.png' -or $_.Name -like 'supplement-*.png' -or $_.Name -like 'review-refresh-*.png' -or $_.Name -like 'planning-*.png' -or $_.Name -like 'billing-*.png' -or $_.Name -like 'dialog-*.png' -or $_.Name -in @('decision-confirm.png','decision-home.png') }) { Copy-Item -LiteralPath $preview.FullName -Destination $packagePreviews }
}
Compress-Archive -LiteralPath $packageDestination -DestinationPath $packageZip -CompressionLevel Optimal
[pscustomobject]@{directory=$packageDestination; archive=$packageZip; sha256=(Get-FileHash -LiteralPath $packageZip).Hash} | ConvertTo-Json
