[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$AsiPath,
    [string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$entryName = 'Plugins/PileChainRepair.asi'
$featureRoot = Split-Path -Parent $PSScriptRoot
$backupRoot = Join-Path $featureRoot 'backups'
$backupEntry = Join-Path $backupRoot 'PileChainRepair.asi.pre-install'
$metadataPath = Join-Path $backupRoot 'Plugin_SU.pile-chain-repair.json'
$archive = Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'
$candidate = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).tmp"
$rollback = Join-Path ([IO.Path]::GetDirectoryName($archive)) "Plugin_SU.pile-chain-repair.$([guid]::NewGuid()).rollback"
$metadataTemporary = "$metadataPath.tmp"

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
            try {
                $digest = $sha.ComputeHash($stream)
                $hashes.Add($entry.FullName, [BitConverter]::ToString($digest).Replace('-', '').ToLowerInvariant())
            } finally {
                $sha.Dispose()
                $stream.Dispose()
            }
        }
    } finally { $zip.Dispose() }
    return $hashes
}

function Copy-ArchiveEntry([string]$ZipPath, [string]$Name, [string]$Destination) {
    $zip = [IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $matches = @($zip.Entries | Where-Object { $_.FullName -ceq $Name })
        if ($matches.Count -gt 1) { throw "Duplicate ZIP entry: $Name" }
        if ($matches.Count -eq 0) { return $false }
        $input = $matches[0].Open()
        $output = [IO.File]::Open($Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $input.CopyTo($output) } finally { $output.Dispose(); $input.Dispose() }
        return $true
    } finally { $zip.Dispose() }
}

function New-ArchiveWithEntry([string]$Source, [string]$Destination, [string]$Name, [string]$Replacement) {
    $inputZip = [IO.Compression.ZipFile]::OpenRead($Source)
    try {
        $outputZip = [IO.Compression.ZipFile]::Open($Destination, [IO.Compression.ZipArchiveMode]::Create)
        try {
            $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            $entryExists = $false
            foreach ($entry in $inputZip.Entries) {
                if (-not $names.Add($entry.FullName)) { throw "Duplicate ZIP entry: $($entry.FullName)" }
                if ($entry.FullName -ceq $Name) { $entryExists = $true; continue }
                $copy = $outputZip.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
                $copy.LastWriteTime = $entry.LastWriteTime
                if ($entry.FullName.EndsWith('/')) { continue }
                $sourceStream = $entry.Open()
                $targetStream = $copy.Open()
                try { $sourceStream.CopyTo($targetStream) } finally { $targetStream.Dispose(); $sourceStream.Dispose() }
            }
            if (-not $entryExists) {
                $repair = $outputZip.CreateEntry($Name, [IO.Compression.CompressionLevel]::Optimal)
                $asiStream = [IO.File]::OpenRead($Replacement)
                $repairStream = $repair.Open()
                try { $asiStream.CopyTo($repairStream) } finally { $repairStream.Dispose(); $asiStream.Dispose() }
            } else {
                $repair = $outputZip.CreateEntry($Name, [IO.Compression.CompressionLevel]::Optimal)
                $asiStream = [IO.File]::OpenRead($Replacement)
                $repairStream = $repair.Open()
                try { $asiStream.CopyTo($repairStream) } finally { $repairStream.Dispose(); $asiStream.Dispose() }
            }
        } finally { $outputZip.Dispose() }
    } finally { $inputZip.Dispose() }
}

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -in @('S4_Main', 'Settlers United') })
if ($running.Count -gt 0) { throw "Close S4_Main and Settlers United first: $($running.ProcessName -join ', ')" }
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Plugin_SU.zip not found: $archive" }
if (-not (Test-Path -LiteralPath $AsiPath -PathType Leaf)) { throw "PileChainRepair.asi not found: $AsiPath" }
if ((Test-Path -LiteralPath $candidate) -or (Test-Path -LiteralPath $rollback) -or (Test-Path -LiteralPath $metadataTemporary)) { throw 'A PileChainRepair transaction temporary already exists' }

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$before = Get-EntryHashes $archive
$desiredHash = Get-Sha256 $AsiPath
$metadata = $null
$firstInstall = -not (Test-Path -LiteralPath $metadataPath -PathType Leaf)

if ($firstInstall) {
    if (Test-Path -LiteralPath $backupEntry) { throw "Unexpected pre-install entry backup exists: $backupEntry" }
    $hadPreviousEntry = Copy-ArchiveEntry $archive $entryName $backupEntry
    $previousHash = if ($hadPreviousEntry) { Get-Sha256 $backupEntry } else { '' }
    $expectedCurrentHash = $previousHash
    if (-not $hadPreviousEntry -and $before.ContainsKey($entryName)) { throw 'Archive entry lookup was inconsistent' }
} else {
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    if ([IO.Path]::GetFullPath([string]$metadata.archivePath) -cne [IO.Path]::GetFullPath($archive)) { throw 'Install metadata belongs to a different archive' }
    if (-not $before.ContainsKey($entryName)) {
        throw 'Installed PileChainRepair entry is missing from the archive'
    }
    if ($metadata.PSObject.Properties.Name -contains 'installedAsiSha256') {
        $recordedInstalledHash = [string]$metadata.installedAsiSha256
        $hadPreviousEntry = [bool]$metadata.hadPreviousEntry
        $previousHash = [string]$metadata.previousAsiSha256
    } else {
        # Migrate the original whole-archive metadata. The original snapshot
        # predates the first PileChainRepair entry, so uninstall removes it.
        $recordedInstalledHash = [string]$metadata.embeddedAsiSha256
        $hadPreviousEntry = $false
        $previousHash = ''
    }
    if ($before[$entryName] -cne $recordedInstalledHash) {
        throw 'Installed PileChainRepair entry changed outside this installer'
    }
    if ($hadPreviousEntry -and ((-not (Test-Path -LiteralPath $backupEntry -PathType Leaf)) -or (Get-Sha256 $backupEntry) -cne $previousHash)) { throw 'Pre-install plugin entry backup failed verification' }
    if (-not $hadPreviousEntry -and (Test-Path -LiteralPath $backupEntry)) { throw 'Unexpected pre-install plugin entry backup exists' }
    $expectedCurrentHash = $recordedInstalledHash
}

if ($before.ContainsKey($entryName) -ne ($expectedCurrentHash -ne '')) { throw 'Current plugin entry presence differs from recorded state' }
if ($before.ContainsKey($entryName) -and $before[$entryName] -cne $expectedCurrentHash) { throw 'Current plugin entry does not match its recorded hash' }

$afterMetadata = [pscustomobject][ordered]@{
    archivePath = [IO.Path]::GetFullPath($archive)
    hadPreviousEntry = $hadPreviousEntry
    previousAsiSha256 = $previousHash
    installedAsiSha256 = $desiredHash
}
$afterMetadata | ConvertTo-Json | Set-Content -LiteralPath $metadataTemporary -Encoding UTF8
Copy-Item -LiteralPath $archive -Destination $rollback

try {
    New-ArchiveWithEntry $archive $candidate $entryName $AsiPath
    $after = Get-EntryHashes $candidate
    foreach ($name in $before.Keys) {
        if ($name -ceq $entryName) { continue }
        if (-not $after.ContainsKey($name) -or $after[$name] -cne $before[$name]) { throw "Candidate changed existing entry: $name" }
    }
    if (-not $after.ContainsKey($entryName) -or $after[$entryName] -cne $desiredHash -or $after.Count -ne ($before.Count + [int](-not $before.ContainsKey($entryName)))) { throw 'Candidate archive entry verification failed' }
    Move-Item -LiteralPath $candidate -Destination $archive -Force
    Move-Item -LiteralPath $metadataTemporary -Destination $metadataPath -Force
    [pscustomobject]@{ Archive = $archive; EmbeddedAsiSha256 = $desiredHash; Entry = $entryName }
} catch {
    if (Test-Path -LiteralPath $rollback) { Copy-Item -LiteralPath $rollback -Destination $archive -Force }
    if (Test-Path -LiteralPath $metadataTemporary) { Remove-Item -LiteralPath $metadataTemporary -Force }
    throw
} finally {
    foreach ($path in @($candidate, $rollback)) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force } }
    if ($firstInstall -and (Test-Path -LiteralPath $metadataPath -PathType Leaf) -eq $false -and (Test-Path -LiteralPath $backupEntry)) { Remove-Item -LiteralPath $backupEntry -Force }
}
