Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$script:DiagnosticEntry = 'Plugins/CampaignCompletionDebug.asi'

function Get-FileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "File does not exist: $Path"
    }
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-AuthorizedArchivePath([string]$ArchivePath) {
    $full = [IO.Path]::GetFullPath($ArchivePath)
    $parent = [IO.Path]::GetDirectoryName($full)
    if ([IO.Path]::GetFileName($full) -cne 'Plugin_SU.zip' -or
        [IO.Path]::GetFileName($parent) -cne 's4_artifacts') {
        throw "Archive must be s4_artifacts/Plugin_SU.zip: $full"
    }
    $full
}

function Get-ZipEntryHashes([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "ZIP does not exist: $Path"
    }

    $hashes = [Collections.Generic.Dictionary[string,string]]::new(
        [StringComparer]::Ordinal
    )
    $zip = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Path))
    try {
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            if ($hashes.ContainsKey($entry.FullName)) {
                throw "ZIP contains duplicate entry: $($entry.FullName)"
            }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try {
                $hash = $sha.ComputeHash($stream)
                $hashes.Add(
                    $entry.FullName,
                    ([BitConverter]::ToString($hash).Replace('-', '').ToLowerInvariant())
                )
            } finally {
                $sha.Dispose()
                $stream.Dispose()
            }
        }
    } finally {
        $zip.Dispose()
    }
    return $hashes
}

function Write-BackupMetadata([string]$Path, $Metadata) {
    $json = $Metadata | ConvertTo-Json -Depth 4
    [IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Read-BackupMetadata([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Backup metadata is missing: $Path"
    }
    Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Compare-ArchiveEntries([string]$BaselinePath, [string]$CandidatePath, [string]$ExpectedAsiHash) {
    $baseline = Get-ZipEntryHashes $BaselinePath
    $candidate = Get-ZipEntryHashes $CandidatePath

    foreach ($name in $baseline.Keys) {
        if ($name -ceq $script:DiagnosticEntry) { continue }
        if (-not $candidate.ContainsKey($name)) {
            throw "Patched ZIP is missing original entry: $name"
        }
        if ($baseline[$name] -cne $candidate[$name]) {
            throw "Patched ZIP changed original entry: $name"
        }
    }

    foreach ($name in $candidate.Keys) {
        if ($name -ceq $script:DiagnosticEntry) { continue }
        if (-not $baseline.ContainsKey($name)) {
            throw "Patched ZIP contains unexpected entry: $name"
        }
    }

    if (-not $candidate.ContainsKey($script:DiagnosticEntry)) {
        throw "Patched ZIP is missing $($script:DiagnosticEntry)"
    }
    if ($candidate[$script:DiagnosticEntry] -cne $ExpectedAsiHash) {
        throw "Embedded diagnostic ASI hash mismatch"
    }

    $expectedCount = $baseline.Count
    if (-not $baseline.ContainsKey($script:DiagnosticEntry)) { $expectedCount++ }
    if ($candidate.Count -ne $expectedCount) {
        throw "Patched ZIP entry count mismatch"
    }
}

function New-PatchedArchive([string]$SourcePath, [string]$DestinationPath, [string]$AsiPath) {
    $source = [IO.Compression.ZipFile]::OpenRead($SourcePath)
    try {
        $outputStream = [IO.File]::Open(
            $DestinationPath,
            [IO.FileMode]::CreateNew,
            [IO.FileAccess]::ReadWrite,
            [IO.FileShare]::None
        )
        try {
            $destination = [IO.Compression.ZipArchive]::new(
                $outputStream,
                [IO.Compression.ZipArchiveMode]::Create,
                $true
            )
            try {
                $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
                foreach ($entry in $source.Entries) {
                    if (-not $names.Add($entry.FullName)) {
                        throw "ZIP contains duplicate entry: $($entry.FullName)"
                    }
                    if ($entry.FullName -ceq $script:DiagnosticEntry) { continue }

                    $copy = $destination.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
                    $copy.LastWriteTime = $entry.LastWriteTime
                    if ($entry.FullName.EndsWith('/')) { continue }
                    $input = $entry.Open()
                    $output = $copy.Open()
                    try {
                        $input.CopyTo($output)
                    } finally {
                        $output.Dispose()
                        $input.Dispose()
                    }
                }

                $diagnostic = $destination.CreateEntry(
                    $script:DiagnosticEntry,
                    [IO.Compression.CompressionLevel]::Optimal
                )
                $asiInput = [IO.File]::OpenRead($AsiPath)
                $asiOutput = $diagnostic.Open()
                try {
                    $asiInput.CopyTo($asiOutput)
                } finally {
                    $asiOutput.Dispose()
                    $asiInput.Dispose()
                }
            } finally {
                $destination.Dispose()
            }
        } finally {
            $outputStream.Dispose()
        }
    } finally {
        $source.Dispose()
    }
}

function Install-CampaignCompletionArtifact(
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [Parameter(Mandatory = $true)][string]$AsiPath,
    [Parameter(Mandatory = $true)][string]$BackupDirectory
) {
    $archive = Assert-AuthorizedArchivePath $ArchivePath
    $asi = [IO.Path]::GetFullPath($AsiPath)
    $backupRoot = [IO.Path]::GetFullPath($BackupDirectory)
    $tempSibling = "$archive.campaigncompletion.tmp"
    $backupZip = Join-Path $backupRoot 'Plugin_SU.zip.original'
    $metadataPath = Join-Path $backupRoot 'Plugin_SU.backup.json'
    $workspace = Join-Path ([IO.Path]::GetTempPath()) "CampaignCompletionPatch-$([guid]::NewGuid())"
    $candidate = Join-Path $workspace 'Plugin_SU.zip'
    $replacementCompleted = $false

    if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) { throw "Archive does not exist: $archive" }
    if (-not (Test-Path -LiteralPath $asi -PathType Leaf)) { throw "ASI does not exist: $asi" }
    if (Test-Path -LiteralPath $tempSibling) { throw "Authorized temporary sibling already exists: $tempSibling" }

    try {
        New-Item -ItemType Directory -Path $backupRoot, $workspace -Force | Out-Null

        $backupExists = Test-Path -LiteralPath $backupZip -PathType Leaf
        $metadataExists = Test-Path -LiteralPath $metadataPath -PathType Leaf
        if ($backupExists -xor $metadataExists) {
            throw 'Original backup and metadata must either both exist or both be absent'
        }

        if (-not $backupExists) {
            $originalInfo = Get-Item -LiteralPath $archive
            $originalHash = Get-FileSha256 $archive
            Copy-Item -LiteralPath $archive -Destination $backupZip
            if ((Get-FileSha256 $backupZip) -cne $originalHash) { throw 'Original backup hash verification failed' }
            $metadata = [pscustomobject]@{
                archivePath = $archive
                originalSha256 = $originalHash
                originalSize = $originalInfo.Length
                originalLastWriteTimeUtc = $originalInfo.LastWriteTimeUtc.ToString('o')
                patchedSha256 = ''
                patchedSize = 0
                patchedLastWriteTimeUtc = ''
                embeddedAsiSha256 = ''
            }
            Write-BackupMetadata $metadataPath $metadata
        } else {
            $metadata = Read-BackupMetadata $metadataPath
        }

        $backupHash = Get-FileSha256 $backupZip
        if ($backupHash -cne [string]$metadata.originalSha256) {
            throw 'Original backup no longer matches its recorded hash'
        }

        $installedHash = Get-FileSha256 $archive
        $knownPatchedHash = [string]$metadata.patchedSha256
        if ($installedHash -cne [string]$metadata.originalSha256 -and
            ([string]::IsNullOrEmpty($knownPatchedHash) -or $installedHash -cne $knownPatchedHash)) {
            throw 'Installed Plugin_SU.zip changed outside this integration; refusing to patch'
        }

        $asiHash = Get-FileSha256 $asi
        New-PatchedArchive $archive $candidate $asi
        Compare-ArchiveEntries $backupZip $candidate $asiHash

        Copy-Item -LiteralPath $candidate -Destination $tempSibling
        Compare-ArchiveEntries $backupZip $tempSibling $asiHash
        Move-Item -LiteralPath $tempSibling -Destination $archive -Force
        $replacementCompleted = $true

        $patchedInfo = Get-Item -LiteralPath $archive
        $metadata.patchedSha256 = Get-FileSha256 $archive
        $metadata.patchedSize = $patchedInfo.Length
        $metadata.patchedLastWriteTimeUtc = $patchedInfo.LastWriteTimeUtc.ToString('o')
        $metadata.embeddedAsiSha256 = $asiHash
        Write-BackupMetadata $metadataPath $metadata
        return $metadata
    } catch {
        if ($replacementCompleted) {
            $verifiedBackupHash = Get-FileSha256 $backupZip
            if ($verifiedBackupHash -cne [string]$metadata.originalSha256) {
                throw "Patch failed after replacement and backup verification also failed: $($_.Exception.Message)"
            }
            Copy-Item -LiteralPath $backupZip -Destination $archive -Force
        }
        throw
    } finally {
        if (Test-Path -LiteralPath $tempSibling) {
            Remove-Item -LiteralPath $tempSibling -Force
        }
        if (Test-Path -LiteralPath $workspace) {
            Remove-Item -LiteralPath $workspace -Recurse -Force
        }
    }
}

function Restore-CampaignCompletionArtifact(
    [Parameter(Mandatory = $true)][string]$ArchivePath,
    [Parameter(Mandatory = $true)][string]$BackupDirectory
) {
    $archive = Assert-AuthorizedArchivePath $ArchivePath
    $backupRoot = [IO.Path]::GetFullPath($BackupDirectory)
    $tempSibling = "$archive.campaigncompletion.tmp"
    $backupZip = Join-Path $backupRoot 'Plugin_SU.zip.original'
    $metadataPath = Join-Path $backupRoot 'Plugin_SU.backup.json'

    if (Test-Path -LiteralPath $tempSibling) { throw "Authorized temporary sibling already exists: $tempSibling" }
    $metadata = Read-BackupMetadata $metadataPath
    if ((Get-FileSha256 $backupZip) -cne [string]$metadata.originalSha256) {
        throw 'Original backup no longer matches its recorded hash'
    }
    if ((Get-FileSha256 $archive) -cne [string]$metadata.patchedSha256) {
        throw 'Installed Plugin_SU.zip does not match the recorded patched hash; refusing to restore'
    }

    try {
        Copy-Item -LiteralPath $backupZip -Destination $tempSibling
        if ((Get-FileSha256 $tempSibling) -cne [string]$metadata.originalSha256) {
            throw 'Restore temporary sibling failed hash verification'
        }
        Move-Item -LiteralPath $tempSibling -Destination $archive -Force
        (Get-Item -LiteralPath $archive).LastWriteTimeUtc = [datetime]::Parse(
            [string]$metadata.originalLastWriteTimeUtc,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind
        )
        return [pscustomobject]@{
            ArchivePath = $archive
            RestoredSha256 = Get-FileSha256 $archive
        }
    } finally {
        if (Test-Path -LiteralPath $tempSibling) {
            Remove-Item -LiteralPath $tempSibling -Force
        }
    }
}

Export-ModuleMember -Function Get-FileSha256, Get-ZipEntryHashes, Install-CampaignCompletionArtifact, Restore-CampaignCompletionArtifact
