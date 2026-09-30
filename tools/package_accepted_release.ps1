[CmdletBinding()]
param(
    [string]$CampaignAsiPath,
    [string]$PileRepairAsiPath,
    [string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repository = Split-Path -Parent $PSScriptRoot
$releaseId = 'Settlers4Plugins-0.13.4-0.3.1'
if ([string]::IsNullOrWhiteSpace($CampaignAsiPath)) { $CampaignAsiPath = Join-Path $repository 'artifacts/phase-7-4/inner/Plugins/CampaignCompletionDebug.asi' }
if ([string]::IsNullOrWhiteSpace($PileRepairAsiPath)) { $PileRepairAsiPath = Join-Path $repository 'artifacts/pile-chain-repair-0.3.1/PileChainRepair.asi' }
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = Join-Path $repository 'dist' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
Import-Module (Join-Path $PSScriptRoot 'release/ReleaseArchive.psm1') -Force
$campaignHash = 'de10824a451bec3d566dbf417307c23fb9912fbf6aa2ea2b03e98e9d20655534'
$pileHash = '8d15897471516c162dac98376cd8760a7ea7bbf16d6305247a3ce353aba32cc4'
if ((Get-ReleaseSha256 $CampaignAsiPath) -cne $campaignHash) { throw 'Campaign ASI differs from accepted 0.13.4' }
if ((Get-ReleaseSha256 $PileRepairAsiPath) -cne $pileHash) { throw 'PileChainRepair ASI differs from accepted 0.3.1' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$package = Join-Path $OutputDirectory ($releaseId + '.zip')
$stage = Join-Path $OutputDirectory ('.' + $releaseId + '-' + [guid]::NewGuid().ToString('N'))
$candidate = "$package.$([guid]::NewGuid()).tmp"
try {
    foreach ($directory in @($stage, (Join-Path $stage 'Plugins'), (Join-Path $stage 'config'), (Join-Path $stage 'docs'))) { New-Item -ItemType Directory -Path $directory | Out-Null }
    Copy-Item -LiteralPath $CampaignAsiPath -Destination (Join-Path $stage 'Plugins/CampaignCompletionDebug.asi')
    Copy-Item -LiteralPath $PileRepairAsiPath -Destination (Join-Path $stage 'Plugins/PileChainRepair.asi')
    foreach ($name in @('install.ps1', 'rollback.ps1', 'ReleaseArchive.psm1', 'README.md')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('release/' + $name)) -Destination (Join-Path $stage $name) }
    Copy-Item -LiteralPath (Join-Path $repository 'CampaignMarker/config/CampaignCompletionDebug.ini') -Destination (Join-Path $stage 'config/CampaignCompletionDebug.ini.example')
    Copy-Item -LiteralPath (Join-Path $repository 'AIGoodCrashRepair/docs/2026-09-30-acceptance.md') -Destination (Join-Path $stage 'docs/pile-chain-acceptance.md')
    Copy-Item -LiteralPath (Join-Path $repository 'CampaignMarker/docs/research/phase-7-4-container-offset-marker-candidate-audit.md') -Destination (Join-Path $stage 'docs/campaign-marker-audit.md')
    Copy-Item -LiteralPath (Join-Path $repository 'README.md') -Destination (Join-Path $stage 'docs/project-readme.md')
    # Source documents are snapshots; rewrite their repository-relative links for the bundle.
    $docLinks = @{
        'docs/pile-chain-acceptance.md' = @{
            '../../CampaignMarker/docs/research/phase-7-4-container-offset-marker-candidate-audit.md' = 'campaign-marker-audit.md'
            '../README.md' = 'https://github.com/pikechu/s4-plugins/blob/main/AIGoodCrashRepair/README.md'
        }
        'docs/campaign-marker-audit.md' = @{
            '../../../AIGoodCrashRepair/docs/2026-09-30-acceptance.md' = 'pile-chain-acceptance.md'
        }
        'docs/project-readme.md' = @{
            'AIGoodCrashRepair/docs/2026-09-30-acceptance.md' = 'pile-chain-acceptance.md'
            'CampaignMarker/docs/research/phase-7-4-container-offset-marker-candidate-audit.md' = 'campaign-marker-audit.md'
        }
    }
    foreach ($relative in $docLinks.Keys) {
        $path = Join-Path $stage $relative
        $document = [IO.File]::ReadAllText($path)
        foreach ($oldLink in $docLinks[$relative].Keys) { $document = $document.Replace('](' + $oldLink + ')', '](' + $docLinks[$relative][$oldLink] + ')') }
        [IO.File]::WriteAllText($path, $document, [Text.UTF8Encoding]::new($false))
    }
    $files = @()
    $stagePrefix = $stage.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    foreach ($file in (Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($stagePrefix.Length).Replace('\', '/')
        $files += [pscustomobject][ordered]@{ path = $relative; sha256 = Get-ReleaseSha256 $file.FullName; size = $file.Length }
    }
    $manifest = [pscustomobject][ordered]@{
        schemaVersion = 1; releaseId = $releaseId; acceptedDate = '2026-09-30'
        versions = [pscustomobject]@{ CampaignMarker = '0.13.4'; PileChainRepair = '0.3.1' }
        files = $files
    }
    [IO.File]::WriteAllText((Join-Path $stage 'manifest.json'), ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Assert-ReleasePackage $stage
    $outputZip = [IO.Compression.ZipFile]::Open($candidate, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($relative in @(@($files | ForEach-Object { $_.path }) + @('manifest.json'))) {
            $entry = $outputZip.CreateEntry($relative, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(2026, 9, 30, 0, 0, 0, [TimeSpan]::Zero)
            $inputStream = [IO.File]::OpenRead((Join-Path $stage $relative)); $outputStream = $entry.Open()
            try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
        }
    } finally { $outputZip.Dispose() }
    # Inspect the produced archive, including every manifest payload, before publishing.
    $zip = [IO.Compression.ZipFile]::OpenRead($candidate)
    try {
        if ($zip.Entries.Count -ne ($files.Count + 1)) { throw 'Package entry count mismatch' }
        foreach ($file in $files) {
            $entry = $zip.GetEntry($file.path)
            if ($null -eq $entry -or $entry.Length -ne $file.size) { throw "Packaged file missing or size mismatch: $($file.path)" }
            $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
            try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
            finally { $sha.Dispose(); $stream.Dispose() }
            if ($actual -cne $file.sha256) { throw "Packaged hash mismatch: $($file.path)" }
        }
    } finally { $zip.Dispose() }
    if ([IO.File]::Exists($package)) { [IO.File]::Replace($candidate, $package, [Management.Automation.Language.NullString]::Value) } else { [IO.File]::Move($candidate, $package) }
    $digest = Get-ReleaseSha256 $package
    [IO.File]::WriteAllText(($package + '.sha256'), ($digest + '  ' + [IO.Path]::GetFileName($package) + [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
    [pscustomobject]@{ Package = $package; Sha256 = $digest; CampaignVersion = '0.13.4'; PileChainRepairVersion = '0.3.1'; PayloadFiles = $files.Count }
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    if ([IO.File]::Exists($candidate)) { Remove-Item -LiteralPath $candidate -Force }
}
