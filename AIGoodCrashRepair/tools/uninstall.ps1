[CmdletBinding()]
param([string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$entryName = 'Plugins/PileChainRepair.asi'
$featureRoot = Split-Path -Parent $PSScriptRoot
$backupEntry = Join-Path (Join-Path $featureRoot 'backups') 'PileChainRepair.asi.pre-install'
$metadataPath = Join-Path (Join-Path $featureRoot 'backups') 'Plugin_SU.pile-chain-repair.json'
$archive = Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'
$candidate = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).tmp"
$rollback = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).rollback"

function Get-Sha256([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
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

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -in @('S4_Main', 'Settlers United') })
if ($running.Count -gt 0) { throw "Close S4_Main and Settlers United first: $($running.ProcessName -join ', ')" }
if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or -not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) { throw 'Archive or PileChainRepair install metadata is missing' }
if ((Test-Path -LiteralPath $candidate) -or (Test-Path -LiteralPath $rollback)) { throw 'A PileChainRepair transaction temporary already exists' }

$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
if ([IO.Path]::GetFullPath([string]$metadata.archivePath) -cne [IO.Path]::GetFullPath($archive)) { throw 'Install metadata belongs to a different archive' }
$before = Get-EntryHashes $archive
if (-not $before.ContainsKey($entryName)) { throw 'PileChainRepair entry is missing from archive' }
if ($metadata.PSObject.Properties.Name -contains 'installedAsiSha256') {
    $installedHash = [string]$metadata.installedAsiSha256
    $hadPreviousEntry = [bool]$metadata.hadPreviousEntry
    $previousHash = [string]$metadata.previousAsiSha256
} else {
    $installedHash = [string]$metadata.embeddedAsiSha256
    $hadPreviousEntry = $false
    $previousHash = ''
}
if ($before[$entryName] -cne $installedHash) { throw 'PileChainRepair entry changed after installation; refusing uninstall' }
if ($hadPreviousEntry -and ((-not (Test-Path -LiteralPath $backupEntry -PathType Leaf)) -or (Get-Sha256 $backupEntry) -cne $previousHash)) { throw 'Pre-install plugin entry backup failed verification' }

Copy-Item -LiteralPath $archive -Destination $rollback
try {
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
    Move-Item -LiteralPath $candidate -Destination $archive -Force
    Remove-Item -LiteralPath $metadataPath -Force
    if (Test-Path -LiteralPath $backupEntry) { Remove-Item -LiteralPath $backupEntry -Force }
    [pscustomobject]@{ Archive = $archive; Entry = $entryName; Removed = (-not $hadPreviousEntry) }
} catch {
    if (Test-Path -LiteralPath $rollback) { Copy-Item -LiteralPath $rollback -Destination $archive -Force }
    throw
} finally {
    foreach ($path in @($candidate, $rollback)) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force } }
}
