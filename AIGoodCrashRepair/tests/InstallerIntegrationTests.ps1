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
        $legacyArchive = Join-Path $backup 'Plugin_SU.zip.pre-pile-chain-repair'
        $originalEntries = [ordered]@{
            'Plugins/CampaignCompletionDebug.asi' = $campaignOne
            'Plugins/OtherPlugin.asi' = $other
        }
        if ($originalEntryPresent) { $originalEntries['Plugins/PileChainRepair.asi'] = $originalRepair }
        Write-Zip $legacyArchive $originalEntries
        $legacy = [ordered]@{
            archivePath = [IO.Path]::GetFullPath($archive)
            preRepairSha256 = HashBytes ([IO.File]::ReadAllBytes($legacyArchive))
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

function Invoke-FailedTransaction([string]$name, [bool]$uninstall, [bool]$failRollback, [bool]$firstInstall) {
    $root = Join-Path $testRoot $name
    $featureRoot = Join-Path $root 'AIGoodCrashRepair'
    $tools = Join-Path $featureRoot 'tools'
    $backup = Join-Path $featureRoot 'backups'
    $su = Join-Path $root 'Settlers United'
    $archiveDirectory = Join-Path $su 'resources/bin/s4_artifacts'
    $archive = Join-Path $archiveDirectory 'Plugin_SU.zip'
    $metadataPath = Join-Path $backup 'Plugin_SU.pile-chain-repair.json'
    $entryBackup = Join-Path $backup 'PileChainRepair.asi.pre-install'
    New-Item -ItemType Directory -Path $tools,$backup,$archiveDirectory -Force | Out-Null
    Copy-Item -LiteralPath $installSource -Destination (Join-Path $tools 'install.ps1')
    Copy-Item -LiteralPath $uninstallSource -Destination (Join-Path $tools 'uninstall.ps1')
    $originalRepair = [Text.Encoding]::UTF8.GetBytes('repair-before-transaction')
    Write-Zip $archive ([ordered]@{
        'Plugins/CampaignCompletionDebug.asi' = [Text.Encoding]::UTF8.GetBytes('current-campaign')
        'Plugins/PileChainRepair.asi' = $originalRepair
    })
    $beforeArchiveHash = HashBytes ([IO.File]::ReadAllBytes($archive))
    $beforeMetadataHash = ''
    if (-not $firstInstall) {
        [ordered]@{
            archivePath = [IO.Path]::GetFullPath($archive)
            hadPreviousEntry = $false
            previousAsiSha256 = ''
            installedAsiSha256 = HashBytes $originalRepair
        } | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding UTF8
        $beforeMetadataHash = HashBytes ([IO.File]::ReadAllBytes($metadataPath))
    }
    $asi = Join-Path $root 'new-repair.asi'
    [IO.File]::WriteAllBytes($asi, [Text.Encoding]::UTF8.GetBytes('repair-after-transaction'))

    # Model failures after archive publication, using real isolated ZIPs.
    function Move-Item {
        [CmdletBinding()]param([string]$LiteralPath, [string]$Destination, [switch]$Force)
        if ($Destination -ceq $metadataPath) { throw 'Injected metadata publish failure' }
        Microsoft.PowerShell.Management\Move-Item @PSBoundParameters
    }
    function Remove-Item {
        [CmdletBinding()]param([string]$LiteralPath, [switch]$Force)
        if ($LiteralPath -ceq $metadataPath -and $uninstall) {
            Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
            throw 'Injected metadata removal failure after deletion'
        }
        Microsoft.PowerShell.Management\Remove-Item @PSBoundParameters
    }
    function Copy-Item {
        [CmdletBinding()]param([string]$LiteralPath, [string]$Destination, [switch]$Force)
        if ($failRollback -and $Destination -ceq $archive -and $LiteralPath.EndsWith('.rollback')) { throw 'Injected archive rollback failure' }
        Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
    }
    $failed = $false
    try {
        try {
            if ($uninstall) { & (Join-Path $tools 'uninstall.ps1') -SettlersUnitedDirectory $su | Out-Null }
            else { & (Join-Path $tools 'install.ps1') -AsiPath $asi -SettlersUnitedDirectory $su | Out-Null }
        } catch { $failed = $true }
    } finally {
        Microsoft.PowerShell.Management\Remove-Item Function:Move-Item,Function:Copy-Item,Function:Remove-Item
    }
    Assert $failed 'Failure injection did not fail the transaction'
    if ($failRollback) {
        $archiveSnapshots = @(Get-ChildItem -LiteralPath $archiveDirectory -Filter '*.rollback')
        Assert ($archiveSnapshots.Count -eq 1) 'Failed rollback discarded its archive snapshot'
        Assert ((HashBytes ([IO.File]::ReadAllBytes($archiveSnapshots[0].FullName))) -ceq $beforeArchiveHash) 'Retained archive snapshot differs from the pre-transaction ZIP'
        $metadataSnapshots = @(Get-ChildItem -LiteralPath $backup -Filter '*.rollback')
        Assert ($metadataSnapshots.Count -eq 1) 'Failed rollback discarded its metadata snapshot'
        Assert ((HashBytes ([IO.File]::ReadAllBytes($metadataSnapshots[0].FullName))) -ceq $beforeMetadataHash) 'Retained metadata snapshot differs from original metadata'
    } else {
        Assert ((HashBytes ([IO.File]::ReadAllBytes($archive))) -ceq $beforeArchiveHash) 'Failed transaction did not restore the exact current archive'
        if ($firstInstall) {
            Assert (-not (Test-Path -LiteralPath $metadataPath)) 'Failed first install left ownership metadata'
            Assert (-not (Test-Path -LiteralPath $entryBackup)) 'Failed first install left an entry backup that blocks retry'
        } else {
            Assert ((HashBytes ([IO.File]::ReadAllBytes($metadataPath))) -ceq $beforeMetadataHash) 'Failed transaction did not restore ownership metadata'
        }
    }
}

function Invoke-RejectedLegacySnapshot {
    $root = Join-Path $testRoot 'legacy-invalid-snapshot'
    $tools = Join-Path $root 'AIGoodCrashRepair/tools'
    $backup = Join-Path $root 'AIGoodCrashRepair/backups'
    $su = Join-Path $root 'Settlers United'
    $archiveDirectory = Join-Path $su 'resources/bin/s4_artifacts'
    $archive = Join-Path $archiveDirectory 'Plugin_SU.zip'
    New-Item -ItemType Directory -Path $tools,$backup,$archiveDirectory -Force | Out-Null
    Copy-Item -LiteralPath $installSource -Destination (Join-Path $tools 'install.ps1')
    Copy-Item -LiteralPath $uninstallSource -Destination (Join-Path $tools 'uninstall.ps1')
    $repair = [Text.Encoding]::UTF8.GetBytes('legacy-repair')
    Write-Zip $archive ([ordered]@{'Plugins/PileChainRepair.asi' = $repair})
    Write-Zip (Join-Path $backup 'Plugin_SU.zip.pre-pile-chain-repair') ([ordered]@{'Plugins/OtherPlugin.asi' = $repair})
    [ordered]@{
        archivePath = [IO.Path]::GetFullPath($archive)
        preRepairSha256 = 'wrong-snapshot-hash'
        embeddedAsiSha256 = HashBytes $repair
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'Plugin_SU.pile-chain-repair.json') -Encoding UTF8
    $beforeArchiveHash = HashBytes ([IO.File]::ReadAllBytes($archive))
    $asi = Join-Path $root 'new-repair.asi'
    [IO.File]::WriteAllBytes($asi, $repair)
    foreach ($script in @('install.ps1', 'uninstall.ps1')) {
        $failed = $false
        try {
            if ($script -eq 'install.ps1') { & (Join-Path $tools $script) -AsiPath $asi -SettlersUnitedDirectory $su | Out-Null }
            else { & (Join-Path $tools $script) -SettlersUnitedDirectory $su | Out-Null }
        } catch { $failed = $true }
        Assert $failed 'Invalid legacy baseline was accepted'
        Assert ((HashBytes ([IO.File]::ReadAllBytes($archive))) -ceq $beforeArchiveHash) 'Invalid legacy baseline changed the current archive'
    }
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    # All archives are isolated under $testRoot. Keep the production guards
    # active in the installers, but model the closed-game precondition here.
    function Get-Process { return @() }
    Invoke-Scenario 'fresh-install' $false $false
    Invoke-Scenario 'legacy-installed-entry' $true $false
    Invoke-Scenario 'legacy-pre-existing-entry' $true $true
    Invoke-Scenario 'pre-existing-entry' $false $true
    Invoke-FailedTransaction 'install-metadata-failure' $false $false $false
    Invoke-FailedTransaction 'first-install-metadata-failure' $false $false $true
    Invoke-FailedTransaction 'uninstall-metadata-failure' $true $false $false
    Invoke-FailedTransaction 'install-rollback-failure' $false $true $false
    Invoke-FailedTransaction 'uninstall-rollback-failure' $true $true $false
    Invoke-RejectedLegacySnapshot
    Write-Output 'PileChainRepair installer integration tests passed'
} finally {
    Remove-Item Function:Get-Process -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
