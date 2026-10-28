function Get-KuraSourceManifest {
    param([Parameter(Mandatory=$true)][string]$Root)
    $files = @(Get-Item -LiteralPath (Join-Path $Root 'CMakeLists.txt'))
    foreach ($folder in @('src','resources','tests')) {
        $files += @(Get-ChildItem -LiteralPath (Join-Path $Root $folder) -File -Recurse)
    }
    $entryMap = @{}
    foreach ($file in $files) {
        $relativePath = $file.FullName.Substring($Root.TrimEnd('\','/').Length + 1).Replace('\','/')
        $entryMap[$relativePath] = $file.FullName
    }
    $orderedPaths = [string[]]@($entryMap.Keys)
    [Array]::Sort($orderedPaths, [StringComparer]::Ordinal)
    $entries = @(foreach ($relativePath in $orderedPaths) {
        [pscustomobject]@{
            path = $relativePath
            sha256 = (Get-FileHash -LiteralPath $entryMap[$relativePath] -Algorithm SHA256).Hash
        }
    })
    $lines = ($entries | ForEach-Object { $_.path + ':' + $_.sha256 }) -join "`n"
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try { $id = [BitConverter]::ToString($algorithm.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines))).Replace('-','') }
    finally { $algorithm.Dispose() }
    [pscustomobject]@{ id=$id; files=$entries }
}
