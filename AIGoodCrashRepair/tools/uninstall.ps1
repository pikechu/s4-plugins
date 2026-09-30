[CmdletBinding()]
param([string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$entryName = 'Plugins/PileChainRepair.asi'
$featureRoot = Split-Path -Parent $PSScriptRoot
$backupRoot = Join-Path $featureRoot 'backups'
$backupEntry = Join-Path $backupRoot 'PileChainRepair.asi.pre-install'
$metadataPath = Join-Path $backupRoot 'Plugin_SU.pile-chain-repair.json'
$legacyBackup = Join-Path $backupRoot 'Plugin_SU.zip.pre-pile-chain-repair'
$transactionLockPath = Join-Path $backupRoot 'PileChainRepair.transaction.lock'
$archive = Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'
$candidate = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).tmp"
$rollback = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).rollback"
$metadataRollback = "$metadataPath.$([guid]::NewGuid()).rollback"

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Get-EntryHashes([string]$Path) {
    $hashes = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    $zip = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            if ([string]::Equals($entry.FullName, $entryName, [StringComparison]::OrdinalIgnoreCase) -and $entry.FullName -cne $entryName) { throw "Case-ambiguous plugin entry: $($entry.FullName)" }
            if ($hashes.ContainsKey($entry.FullName)) { throw "Duplicate ZIP entry: $($entry.FullName)" }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $hashes.Add($entry.FullName, [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()) }
            finally { $sha.Dispose(); $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    return $hashes
}

function Copy-ArchiveEntry([string]$ZipPath, [string]$Name, [string]$Destination) {
    $zip = [IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $matches = @($zip.Entries | Where-Object { $_.FullName -ceq $Name })
        if ($matches.Count -ne 1) { throw "Legacy plugin entry lookup was inconsistent: $Name" }
        $input = $matches[0].Open()
        $output = [IO.File]::Open($Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
    } finally { $zip.Dispose() }
}

function New-ArchiveRestoredEntry([string]$Source, [string]$Destination, [string]$Name, [bool]$HadPreviousEntry, [string]$PreviousEntry) {
    $inputZip = [IO.Compression.ZipFile]::OpenRead($Source)
    try {
        $outputZip = [IO.Compression.ZipFile]::Open($Destination, [IO.Compression.ZipArchiveMode]::Create)
        try {
            $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($entry in $inputZip.Entries) {
                if (-not $names.Add($entry.FullName)) { throw "Duplicate ZIP entry: $($entry.FullName)" }
                if ($entry.FullName -ceq $Name) { continue }
                $copy = $outputZip.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
                $copy.LastWriteTime = $entry.LastWriteTime
                if ($entry.FullName.EndsWith('/')) { continue }
                $sourceStream = $entry.Open()
                $targetStream = $copy.Open()
                try { $sourceStream.CopyTo($targetStream) } finally { $targetStream.Dispose(); $sourceStream.Dispose() }
            }
            if ($HadPreviousEntry) {
                $restored = $outputZip.CreateEntry($Name, [IO.Compression.CompressionLevel]::Optimal)
                $sourceStream = [IO.File]::OpenRead($PreviousEntry)
                $targetStream = $restored.Open()
                try { $sourceStream.CopyTo($targetStream) } finally { $targetStream.Dispose(); $sourceStream.Dispose() }
            }
        } finally { $outputZip.Dispose() }
    } finally { $inputZip.Dispose() }
}

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -eq 'S4_Main' -or $_.ProcessName -like '*Settlers*United*' })
if ($running.Count -gt 0) { throw "Close S4_Main and Settlers United first: $($running.ProcessName -join ', ')" }
if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or -not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) { throw 'Archive or PileChainRepair install metadata is missing' }
if ((Test-Path -LiteralPath $candidate) -or (Test-Path -LiteralPath $rollback) -or (Test-Path -LiteralPath "$metadataPath.tmp")) { throw 'A PileChainRepair transaction temporary already exists' }
try { $transactionLock = [IO.File]::Open($transactionLockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
catch { throw "Another PileChainRepair transaction holds the lock: $transactionLockPath" }
$createdEntryBackup = $false
$archiveMayHaveChanged = $false
$metadataMayHaveChanged = $false
$rollbackFailed = $false
$committed = $false
try {
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    $originalMetadataHash = Get-Sha256 $metadataPath
    if ([IO.Path]::GetFullPath([string]$metadata.archivePath) -cne [IO.Path]::GetFullPath($archive)) { throw 'Install metadata belongs to a different archive' }
    $before = Get-EntryHashes $archive
    $originalArchiveHash = Get-Sha256 $archive
    if (-not $before.ContainsKey($entryName)) { throw 'PileChainRepair entry is missing from archive' }
    if ($metadata.PSObject.Properties.Name -contains 'installedAsiSha256') {
        $installedHash = [string]$metadata.installedAsiSha256
        $hadPreviousEntry = [bool]$metadata.hadPreviousEntry
        $previousHash = [string]$metadata.previousAsiSha256
    } else {
        if (-not (Test-Path -LiteralPath $legacyBackup -PathType Leaf) -or (Get-Sha256 $legacyBackup) -cne [string]$metadata.preRepairSha256) { throw 'Legacy pre-install archive failed verification' }
        $legacyEntries = Get-EntryHashes $legacyBackup
        $installedHash = [string]$metadata.embeddedAsiSha256
        $hadPreviousEntry = $legacyEntries.ContainsKey($entryName)
        $previousHash = if ($hadPreviousEntry) { $legacyEntries[$entryName] } else { '' }
        if ($hadPreviousEntry -and -not (Test-Path -LiteralPath $backupEntry)) {
            $createdEntryBackup = $true
            Copy-ArchiveEntry $legacyBackup $entryName $backupEntry
        }
    }
    if ($before[$entryName] -cne $installedHash) { throw 'PileChainRepair entry changed after installation; refusing uninstall' }
    if ($hadPreviousEntry -and ((-not (Test-Path -LiteralPath $backupEntry -PathType Leaf)) -or (Get-Sha256 $backupEntry) -cne $previousHash)) { throw 'Pre-install plugin entry backup failed verification' }

    Copy-Item -LiteralPath $archive -Destination $rollback
    if ((Get-Sha256 $rollback) -cne $originalArchiveHash) { throw 'Current archive snapshot failed verification' }
    Copy-Item -LiteralPath $metadataPath -Destination $metadataRollback
    if ((Get-Sha256 $metadataRollback) -cne $originalMetadataHash) { throw 'Install metadata snapshot failed verification' }
    New-ArchiveRestoredEntry $archive $candidate $entryName $hadPreviousEntry $backupEntry
    $after = Get-EntryHashes $candidate
    foreach ($name in $before.Keys) {
        if ($name -ceq $entryName) { continue }
        if (-not $after.ContainsKey($name) -or $after[$name] -cne $before[$name]) { throw "Candidate changed existing entry: $name" }
    }
    if ($hadPreviousEntry) {
        if (-not $after.ContainsKey($entryName) -or $after[$entryName] -cne $previousHash) { throw 'Restored plugin entry hash mismatch' }
    } elseif ($after.ContainsKey($entryName)) { throw 'Plugin entry was not removed' }
    if ($after.Count -ne ($before.Count - [int](-not $hadPreviousEntry))) { throw 'Candidate entry count mismatch' }
    $running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -eq 'S4_Main' -or $_.ProcessName -like '*Settlers*United*' })
    if ($running.Count -gt 0) { throw 'Game or Settlers United started during uninstall' }
    if ((Get-Sha256 $archive) -cne $originalArchiveHash) { throw 'Archive changed during uninstall' }
    $archiveMayHaveChanged = $true
    Move-Item -LiteralPath $candidate -Destination $archive -Force
    $metadataMayHaveChanged = $true
    Remove-Item -LiteralPath $metadataPath -Force
    $committed = $true
    [pscustomobject]@{ Archive = $archive; Entry = $entryName; Removed = (-not $hadPreviousEntry) }
} catch {
    $failure = $_
    try {
        if ($archiveMayHaveChanged) {
            Copy-Item -LiteralPath $rollback -Destination $archive -Force
            if ((Get-Sha256 $archive) -cne $originalArchiveHash) { throw 'Restored archive failed verification' }
        }
        if ($metadataMayHaveChanged) {
            Copy-Item -LiteralPath $metadataRollback -Destination $metadataPath -Force
            if ((Get-Sha256 $metadataPath) -cne $originalMetadataHash) { throw 'Restored metadata failed verification' }
        }
    } catch {
        $rollbackFailed = $true
        throw "Uninstall failed: $failure. Rollback failed: $_. Recovery snapshots retained at $rollback and $metadataRollback"
    }
    throw $failure
} finally {
    if (-not $rollbackFailed) {
        foreach ($path in @($candidate, $rollback, $metadataRollback)) {
            if (Test-Path -LiteralPath $path) {
                try { Remove-Item -LiteralPath $path -Force } catch { Write-Warning "Transaction cleanup failed for ${path}: $_" }
            }
        }
        if (($committed -or $createdEntryBackup) -and (Test-Path -LiteralPath $backupEntry)) {
            try { Remove-Item -LiteralPath $backupEntry -Force } catch { Write-Warning "Entry backup cleanup failed: $_" }
        }
    }
    $transactionLock.Dispose()
    Remove-Item -LiteralPath $transactionLockPath -Force -ErrorAction SilentlyContinue
}
