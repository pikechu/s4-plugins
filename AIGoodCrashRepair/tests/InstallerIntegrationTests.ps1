[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$sourceRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "PileChainRepairInstallerTests-$([guid]::NewGuid())"
$installSource = Join-Path $sourceRoot 'AIGoodCrashRepair/tools/install.ps1'
$uninstallSource = Join-Path $sourceRoot 'AIGoodCrashRepair/tools/uninstall.ps1'

function Assert([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function HashBytes([byte[]]$bytes) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Read-Entry([string]$path, [string]$name) {
    $zip = [IO.Compression.ZipFile]::OpenRead($path)
    try {
        $matches = @($zip.Entries | Where-Object { $_.FullName -ceq $name })
        Assert ($matches.Count -le 1) "Duplicate test ZIP entry: $name"
        if ($matches.Count -eq 0) { return $null }
        $stream = $matches[0].Open()
        $memory = [IO.MemoryStream]::new()
        try { $stream.CopyTo($memory); return ,$memory.ToArray() }
        finally { $memory.Dispose(); $stream.Dispose() }
    } finally { $zip.Dispose() }
}

function Write-Zip([string]$path, [Collections.IDictionary]$entries) {
    $zip = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $entries.Keys) {
            $entry = $zip.CreateEntry([string]$name, [IO.Compression.CompressionLevel]::Optimal)
            $stream = $entry.Open()
            try { $stream.Write($entries[$name], 0, $entries[$name].Length) } finally { $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
}

function Invoke-Scenario([string]$name, [bool]$legacyInstalledEntry, [bool]$originalEntryPresent) {
    $root = Join-Path $testRoot $name
    $featureRoot = Join-Path $root 'AIGoodCrashRepair'
    $tools = Join-Path $featureRoot 'tools'
    $su = Join-Path $root 'Settlers United'
    $archiveDirectory = Join-Path $su 'resources/bin/s4_artifacts'
    $archive = Join-Path $archiveDirectory 'Plugin_SU.zip'
    $backup = Join-Path $featureRoot 'backups'
    New-Item -ItemType Directory -Path $tools,$archiveDirectory,$backup -Force | Out-Null
    Copy-Item -LiteralPath $installSource -Destination (Join-Path $tools 'install.ps1')
    Copy-Item -LiteralPath $uninstallSource -Destination (Join-Path $tools 'uninstall.ps1')

    $campaignOne = [Text.Encoding]::UTF8.GetBytes('campaign-one')
    $other = [Text.Encoding]::UTF8.GetBytes('unrelated-plugin')
    $originalRepair = [Text.Encoding]::UTF8.GetBytes('old-repair')
    $repairOne = Join-Path $root 'repair-one.asi'
    $repairTwo = Join-Path $root 'repair-two.asi'
    [IO.File]::WriteAllBytes($repairOne, [Text.Encoding]::UTF8.GetBytes('repair-one'))
    [IO.File]::WriteAllBytes($repairTwo, [Text.Encoding]::UTF8.GetBytes('repair-two'))
    $entries = [ordered]@{
        'Plugins/CampaignCompletionDebug.asi' = $campaignOne
        'Plugins/OtherPlugin.asi' = $other
    }
    if ($legacyInstalledEntry) { $entries['Plugins/PileChainRepair.asi'] = [Text.Encoding]::UTF8.GetBytes('legacy-repair') }
    elseif ($originalEntryPresent) { $entries['Plugins/PileChainRepair.asi'] = $originalRepair }
    Write-Zip $archive $entries

    if ($legacyInstalledEntry) {
        $legacy = [ordered]@{
            archivePath = [IO.Path]::GetFullPath($archive)
            preRepairSha256 = 'legacy-whole-archive-hash'
            patchedSha256 = 'stale-whole-archive-hash'
            embeddedAsiSha256 = HashBytes $entries['Plugins/PileChainRepair.asi']
        }
        $legacy | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'Plugin_SU.pile-chain-repair.json') -Encoding UTF8
    }

    & (Join-Path $tools 'install.ps1') -AsiPath $repairOne -SettlersUnitedDirectory $su | Out-Null
    Assert ((HashBytes (Read-Entry $archive 'Plugins/PileChainRepair.asi')) -ceq (HashBytes ([IO.File]::ReadAllBytes($repairOne)))) 'Install did not publish the new repair entry'
    Assert ((HashBytes (Read-Entry $archive 'Plugins/CampaignCompletionDebug.asi')) -ceq (HashBytes $campaignOne)) 'Install changed CampaignMarker'

    $campaignTwo = [Text.Encoding]::UTF8.GetBytes('campaign-two-after-update')
    $otherAfter = [Text.Encoding]::UTF8.GetBytes('unrelated-plugin-after-update')
    $repairCurrent = Read-Entry $archive 'Plugins/PileChainRepair.asi'
    $updatedEntries = [ordered]@{
        'Plugins/CampaignCompletionDebug.asi' = $campaignTwo
        'Plugins/OtherPlugin.asi' = $otherAfter
        'Plugins/PileChainRepair.asi' = $repairCurrent
    }
    $updatedArchive = "$archive.updated"
    Write-Zip $updatedArchive $updatedEntries
    Move-Item -LiteralPath $updatedArchive -Destination $archive -Force

    & (Join-Path $tools 'install.ps1') -AsiPath $repairTwo -SettlersUnitedDirectory $su | Out-Null
    Assert ((HashBytes (Read-Entry $archive 'Plugins/PileChainRepair.asi')) -ceq (HashBytes ([IO.File]::ReadAllBytes($repairTwo)))) 'Update did not replace the repair entry'
    Assert ((HashBytes (Read-Entry $archive 'Plugins/CampaignCompletionDebug.asi')) -ceq (HashBytes $campaignTwo)) 'Update reverted CampaignMarker'

    & (Join-Path $tools 'uninstall.ps1') -SettlersUnitedDirectory $su | Out-Null
    $restoredEntry = Read-Entry $archive 'Plugins/PileChainRepair.asi'
    if ($originalEntryPresent) {
        Assert ((HashBytes $restoredEntry) -ceq (HashBytes $originalRepair)) 'Uninstall did not restore the pre-existing plugin entry'
    } else { Assert ($null -eq $restoredEntry) 'Uninstall left the repair entry installed' }
    Assert ((HashBytes (Read-Entry $archive 'Plugins/CampaignCompletionDebug.asi')) -ceq (HashBytes $campaignTwo)) 'Uninstall changed CampaignMarker'
    Assert ((HashBytes (Read-Entry $archive 'Plugins/OtherPlugin.asi')) -ceq (HashBytes $otherAfter)) 'Uninstall changed another plugin'
    Assert (-not (Test-Path -LiteralPath (Join-Path $backup 'Plugin_SU.pile-chain-repair.json'))) 'Uninstall left install metadata'
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    # All archives are isolated under $testRoot. Keep the production guards
    # active in the installers, but model the closed-game precondition here.
    function Get-Process { return @() }
    Invoke-Scenario 'fresh-install' $false $false
    Invoke-Scenario 'legacy-installed-entry' $true $false
    Invoke-Scenario 'pre-existing-entry' $false $true
    Write-Output 'PileChainRepair installer integration tests passed'
} finally {
    Remove-Item Function:Get-Process -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
