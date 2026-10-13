$ErrorActionPreference='Stop'
$metricsRepoRoot=Split-Path -Parent $PSScriptRoot
$metricsPaths=@(git -C $metricsRepoRoot ls-files --cached --others --exclude-standard | Sort-Object -Unique)
if ($LASTEXITCODE -ne 0) { throw 'Cannot list repository source files.' }
$metricsRows=@()
foreach ($metricsPath in $metricsPaths) {
    $metricsExtension=[IO.Path]::GetExtension($metricsPath).ToLowerInvariant()
    $metricsCpp=$metricsExtension -in @('.cpp','.h','.hpp','.c')
    if (-not $metricsCpp -and $metricsExtension -ne '.ps1' -and $metricsPath -ne 'CMakeLists.txt') { continue }
    $metricsLines=@(Get-Content -LiteralPath (Join-Path $metricsRepoRoot $metricsPath) | Where-Object {$_ -match '\S'}).Count
    $metricsRows += [pscustomobject]@{path=$metricsPath; cpp=$metricsCpp; nonemptyLines=$metricsLines}
}
$metricsCppRows=@($metricsRows | Where-Object {$_.cpp})
$metricsTotalLines=($metricsRows | Measure-Object -Property nonemptyLines -Sum).Sum
$metricsCppLines=($metricsCppRows | Measure-Object -Property nonemptyLines -Sum).Sum
[pscustomobject]@{
    method='Tracked and non-ignored handwritten C/C++ headers and sources, PowerShell scripts, and CMakeLists.txt; nonempty lines include comments and string literals. Docs, binaries, Qt dependencies, generated files excluded.'
    files=$metricsRows.Count; cppFiles=$metricsCppRows.Count
    nonemptyLines=$metricsTotalLines; cppNonemptyLines=$metricsCppLines
    cppFilePercent=[math]::Round(100*$metricsCppRows.Count/$metricsRows.Count,2)
    cppLinePercent=[math]::Round(100*$metricsCppLines/$metricsTotalLines,2)
    detail=$metricsRows
} | ConvertTo-Json -Depth 4
