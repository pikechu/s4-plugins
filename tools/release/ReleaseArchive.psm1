Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$script:ReleaseId = 'Settlers4Plugins-0.13.4-0.3.1'
$script:Plugins = @(
    [pscustomobject]@{ Name = 'CampaignCompletionDebug.asi'; Version = '0.13.4'; Sha256 = 'de10824a451bec3d566dbf417307c23fb9912fbf6aa2ea2b03e98e9d20655534' },
    [pscustomobject]@{ Name = 'PileChainRepair.asi'; Version = '0.3.1'; Sha256 = '8d15897471516c162dac98376cd8760a7ea7bbf16d6305247a3ce353aba32cc4' }
)

function Get-ReleaseSha256([string]$Path) {
    if (-not [IO.File]::Exists($Path)) { throw "Required file missing: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Assert-ReleaseProcessesClosed {
    $running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -eq 'S4_Main' -or $_.ProcessName -like '*Settlers*United*'
    })
    if ($running.Count -ne 0) { throw "Close the game and Settlers United first: $($running.ProcessName -join ', ')" }
}

function Assert-ReleasePackage([string]$Directory) {
    $manifestPath = Join-Path $Directory 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.releaseId -cne $script:ReleaseId -or $manifest.schemaVersion -ne 1) { throw 'Unexpected release manifest' }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $manifest.files) {
        $relative = [string]$file.path
        if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[/\\])\.\.([/\\]|$)' -or $relative -match ':') { throw "Unsafe manifest path: $relative" }
        if (-not $seen.Add($relative)) { throw "Duplicate manifest path: $relative" }
        $path = Join-Path $Directory ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))
        if ((Get-ReleaseSha256 $path) -cne [string]$file.sha256) { throw "Package hash mismatch: $relative" }
        if ((Get-Item -LiteralPath $path).Length -ne [long]$file.size) { throw "Package size mismatch: $relative" }
    }
    foreach ($plugin in $script:Plugins) {
        $relative = 'Plugins/' + $plugin.Name
        if (-not $seen.Contains($relative)) { throw "Plugin absent from manifest: $relative" }
        if ((Get-ReleaseSha256 (Join-Path $Directory $relative)) -cne $plugin.Sha256) { throw "Plugin is not the accepted version: $relative" }
    }
    foreach ($required in @('install.ps1', 'rollback.ps1', 'ReleaseArchive.psm1', 'README.md', 'config/CampaignCompletionDebug.ini.example')) {
        if (-not $seen.Contains($required)) { throw "Required payload missing from manifest: $required" }
    }
}

function Assert-ReleaseGameDirectory([string]$GameDirectory) {
    $executable = Join-Path $GameDirectory 'S4_Main.exe'
    $expected = '3b561269fb7ce4c281959f8f0db691cebf7cd36a04ad3594461b94290c5d3816'
    if ((Get-ReleaseSha256 $executable) -cne $expected) { throw 'The selected game directory does not contain the admitted S4_Main.exe 2.50.1516.0 build' }
    if (-not [IO.Directory]::Exists((Join-Path $GameDirectory 'Plugins'))) { throw "Game Plugins directory missing: $GameDirectory" }
}

function Get-ReleaseArchiveHashes([string]$Path) {
    # Include directory entries to verify that the candidate preserves the full layout.
    $hashes = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    $caseNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $zip = [IO.Compression.ZipFile]::OpenRead($Path)
    try {
        foreach ($entry in $zip.Entries) {
            if (-not $caseNames.Add($entry.FullName)) { throw "Duplicate or case-ambiguous ZIP entry: $($entry.FullName)" }
            foreach ($plugin in $script:Plugins) {
                $name = 'Plugins/' + $plugin.Name
                if ($entry.FullName.Equals($name, [StringComparison]::OrdinalIgnoreCase) -and $entry.FullName -cne $name) { throw "Case-ambiguous plugin ZIP entry: $($entry.FullName)" }
            }
            $stream = $entry.Open(); $sha = [Security.Cryptography.SHA256]::Create()
            try { $hashes.Add($entry.FullName, [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()) }
            finally { $sha.Dispose(); $stream.Dispose() }
        }
    } finally { $zip.Dispose() }
    return ,$hashes
}

function Export-ReleaseArchiveEntry([string]$Archive, [string]$Name, [string]$Destination) {
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        $entry = $zip.GetEntry($Name)
        if ($null -eq $entry) { throw "Entry missing: $Name" }
        $inputStream = $entry.Open()
        $outputStream = [IO.File]::Open($Destination, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
    } finally { $zip.Dispose() }
}

function New-ReleaseArchive([string]$Source, [string]$Destination, [object[]]$Replacements) {
    if ([IO.File]::Exists($Destination)) { throw "Candidate already exists: $Destination" }
    $inputZip = [IO.Compression.ZipFile]::OpenRead($Source)
    try {
        $outputZip = [IO.Compression.ZipFile]::Open($Destination, [IO.Compression.ZipArchiveMode]::Create)
        try {
            foreach ($entry in $inputZip.Entries) {
                if (@($Replacements | Where-Object { $_.Name -ceq $entry.FullName }).Count -ne 0) { continue }
                $copy = $outputZip.CreateEntry($entry.FullName, [IO.Compression.CompressionLevel]::Optimal)
                $copy.LastWriteTime = $entry.LastWriteTime
                $inputStream = $entry.Open(); $outputStream = $copy.Open()
                try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
            }
            foreach ($replacement in $Replacements) {
                if (-not $replacement.Present) { continue }
                $copy = $outputZip.CreateEntry($replacement.Name, [IO.Compression.CompressionLevel]::Optimal)
                $inputStream = [IO.File]::OpenRead($replacement.Source); $outputStream = $copy.Open()
                try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose(); $inputStream.Dispose() }
            }
        } finally { $outputZip.Dispose() }
    } finally { $inputZip.Dispose() }
}

function Assert-ReleaseArchiveCandidate($Before, $After, [object[]]$Replacements) {
    $targetNames = @($Replacements | ForEach-Object { $_.Name })
    $expectedCount = $Before.Count
    foreach ($name in $Before.Keys) {
        if ($targetNames -ccontains $name) { continue }
        if (-not $After.ContainsKey($name) -or $After[$name] -cne $Before[$name]) { throw "Non-target ZIP entry changed: $name" }
    }
    foreach ($replacement in $Replacements) {
        if ($Before.ContainsKey($replacement.Name)) { $expectedCount-- }
        if ($replacement.Present) {
            $expectedCount++
            if (-not $After.ContainsKey($replacement.Name) -or $After[$replacement.Name] -cne $replacement.Sha256) { throw "Candidate plugin hash mismatch: $($replacement.Name)" }
        } elseif ($After.ContainsKey($replacement.Name)) { throw "Candidate still contains removed entry: $($replacement.Name)" }
    }
    if ($After.Count -ne $expectedCount) { throw 'Candidate ZIP entry count mismatch' }
}

function Get-ReleaseFileState([string]$Path) {
    if (Test-Path -LiteralPath $Path -PathType Container) { throw "Target path is a directory: $Path" }
    $present = [IO.File]::Exists($Path)
    return [pscustomobject]@{ Present = $present; Sha256 = $(if ($present) { Get-ReleaseSha256 $Path } else { '' }) }
}

function Assert-ReleaseFileState([string]$Path, $State) {
    $current = Get-ReleaseFileState $Path
    if ($current.Present -ne [bool]$State.Present -or $current.Sha256 -cne [string]$State.Sha256) { throw "Target changed during the transaction: $Path" }
}

function Copy-ReleaseVerified([string]$Source, [string]$Destination, [string]$Sha256) {
    Copy-Item -LiteralPath $Source -Destination $Destination
    if ((Get-ReleaseSha256 $Destination) -cne $Sha256) { throw "Backup verification failed: $Destination" }
}

function Set-ReleaseJson([string]$Path, $Value) {
    $temporary = "$Path.$([guid]::NewGuid()).tmp"
    try {
        $json = $Value | ConvertTo-Json -Depth 10
        [IO.File]::WriteAllText($temporary, $json, [Text.UTF8Encoding]::new($false))
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temporary, $Path, [Management.Automation.Language.NullString]::Value) }
        else { [IO.File]::Move($temporary, $Path) }
    } finally {
        if ([IO.File]::Exists($temporary)) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Set-ReleaseFile([string]$Source, [string]$Destination) {
    # Stage beside the destination so publication uses an atomic same-volume rename.
    $temporary = "$Destination.$([guid]::NewGuid()).tmp"
    try {
        Copy-Item -LiteralPath $Source -Destination $temporary
        if ((Get-ReleaseSha256 $temporary) -cne (Get-ReleaseSha256 $Source)) { throw "Staged file hash mismatch: $Destination" }
        if ([IO.File]::Exists($Destination)) { [IO.File]::Replace($temporary, $Destination, [Management.Automation.Language.NullString]::Value) }
        else { [IO.File]::Move($temporary, $Destination) }
    } finally {
        if ([IO.File]::Exists($temporary)) { Remove-Item -LiteralPath $temporary -Force }
    }
}

function Restore-ReleaseTransaction([string]$Archive, [string]$ArchiveSnapshot, [string]$ArchiveHash, [object[]]$LiveSnapshots, [bool]$ArchiveChanged, [object[]]$ChangedFiles) {
    $errors = [Collections.Generic.List[string]]::new()
    if ($ArchiveChanged) {
        try {
            Set-ReleaseFile $ArchiveSnapshot $Archive
            if ((Get-ReleaseSha256 $Archive) -cne $ArchiveHash) { throw 'Archive restore verification failed' }
        } catch { $errors.Add([string]$_) }
    }
    foreach ($snapshot in $LiveSnapshots) {
        if ($ChangedFiles -cnotcontains $snapshot.Path) { continue }
        try {
            if ($snapshot.Present) { Set-ReleaseFile $snapshot.Backup $snapshot.Path }
            elseif ([IO.File]::Exists($snapshot.Path)) { Remove-Item -LiteralPath $snapshot.Path -Force }
            Assert-ReleaseFileState $snapshot.Path $snapshot
        } catch { $errors.Add([string]$_) }
    }
    if ($errors.Count -ne 0) { throw ($errors -join '; ') }
}

function Enter-ReleaseLock([string]$Archive) {
    # Keep the lock file; deleting it after releasing could race a waiting installer.
    $lockPath = "$Archive.accepted-release.lock"
    try { return [IO.File]::Open($lockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch { throw "Another combination transaction holds the lock, or the archive directory is not writable: $lockPath. $_" }
}

function Install-AcceptedRelease([string]$PackageDirectory, [string]$SettlersUnitedDirectory, [string]$GameDirectory, [string]$BackupDirectory) {
    Assert-ReleaseProcessesClosed
    Assert-ReleasePackage $PackageDirectory
    Assert-ReleaseGameDirectory $GameDirectory
    $archive = [IO.Path]::GetFullPath((Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'))
    $game = [IO.Path]::GetFullPath($GameDirectory)
    $pluginsDirectory = Join-Path $game 'Plugins'
    if (-not [IO.Directory]::Exists($pluginsDirectory)) { throw "Game Plugins directory missing: $pluginsDirectory" }
    [void](Get-ReleaseSha256 $archive)
    if ([string]::IsNullOrWhiteSpace($BackupDirectory)) {
        $BackupDirectory = Join-Path $env:LOCALAPPDATA ('Settlers4Plugins/backups/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
    }
    $backup = [IO.Path]::GetFullPath($BackupDirectory)
    if (Test-Path -LiteralPath $backup) { throw "Use a new backup directory for each installation: $backup" }
    $lock = Enter-ReleaseLock $archive
    $candidate = "$archive.$([guid]::NewGuid()).candidate"
    $archiveChanged = $false
    $changedFiles = [Collections.Generic.List[string]]::new()
    $liveSnapshots = @()
    $snapshotReady = $false
    $metadata = $null
    try {
        Assert-ReleaseProcessesClosed
        New-Item -ItemType Directory -Path $backup | Out-Null
        $before = Get-ReleaseArchiveHashes $archive
        $archiveHash = Get-ReleaseSha256 $archive
        $archiveSnapshot = Join-Path $backup 'archive-before.zip'
        Copy-ReleaseVerified $archive $archiveSnapshot $archiveHash
        $records = @()
        $replacements = @()
        foreach ($plugin in $script:Plugins) {
            $entryName = 'Plugins/' + $plugin.Name
            $archivePresent = $before.ContainsKey($entryName)
            $archivePreviousHash = if ($archivePresent) { $before[$entryName] } else { '' }
            $archiveBackupName = 'archive-' + $plugin.Name + '.before'
            if ($archivePresent) {
                $path = Join-Path $backup $archiveBackupName
                Export-ReleaseArchiveEntry $archive $entryName $path
                if ((Get-ReleaseSha256 $path) -cne $archivePreviousHash) { throw "ZIP entry backup verification failed: $entryName" }
            }
            $livePath = Join-Path $pluginsDirectory $plugin.Name
            $liveState = Get-ReleaseFileState $livePath
            $liveBackupName = 'live-' + $plugin.Name + '.before'
            $liveBackup = Join-Path $backup $liveBackupName
            if ($liveState.Present) { Copy-ReleaseVerified $livePath $liveBackup $liveState.Sha256 }
            $liveSnapshots += [pscustomobject]@{ Path = $livePath; Present = $liveState.Present; Sha256 = $liveState.Sha256; Backup = $liveBackup }
            $records += [pscustomobject]@{
                name = $plugin.Name; archiveEntry = $entryName; installedSha256 = $plugin.Sha256
                archiveBeforePresent = $archivePresent; archiveBeforeSha256 = $archivePreviousHash; archiveBackup = $archiveBackupName
                livePath = $livePath; liveBeforePresent = $liveState.Present; liveBeforeSha256 = $liveState.Sha256; liveBackup = $liveBackupName
            }
            $replacements += [pscustomobject]@{ Name = $entryName; Present = $true; Sha256 = $plugin.Sha256; Source = (Join-Path $PackageDirectory $entryName) }
        }
        $snapshotReady = $true
        $metadata = [pscustomobject][ordered]@{
            schemaVersion = 1; releaseId = $script:ReleaseId; status = 'prepared'; installedUtc = [DateTime]::UtcNow.ToString('o')
            archivePath = $archive; gameDirectory = $game; archiveBeforeSha256 = $archiveHash; entries = $records
        }
        Set-ReleaseJson (Join-Path $backup 'metadata.json') $metadata
        New-ReleaseArchive $archive $candidate $replacements
        Assert-ReleaseArchiveCandidate $before (Get-ReleaseArchiveHashes $candidate) $replacements
        $candidateHash = Get-ReleaseSha256 $candidate
        # Second guard immediately before any target publication.
        Assert-ReleaseProcessesClosed
        Assert-ReleasePackage $PackageDirectory
        Assert-ReleaseGameDirectory $GameDirectory
        if ((Get-ReleaseSha256 $archive) -cne $archiveHash) { throw 'SU archive changed while preparing installation' }
        foreach ($state in $liveSnapshots) { Assert-ReleaseFileState $state.Path $state }
        $archiveChanged = $true
        [IO.File]::Replace($candidate, $archive, [Management.Automation.Language.NullString]::Value)
        foreach ($plugin in $script:Plugins) {
            $livePath = Join-Path $pluginsDirectory $plugin.Name
            $changedFiles.Add($livePath)
            Set-ReleaseFile (Join-Path $PackageDirectory ('Plugins/' + $plugin.Name)) $livePath
            if ((Get-ReleaseSha256 $livePath) -cne $plugin.Sha256) { throw "Live plugin verification failed: $livePath" }
        }
        if ((Get-ReleaseSha256 $archive) -cne $candidateHash) { throw 'Published SU archive verification failed' }
        Assert-ReleaseArchiveCandidate $before (Get-ReleaseArchiveHashes $archive) $replacements
        $metadata.status = 'installed'
        Set-ReleaseJson (Join-Path $backup 'metadata.json') $metadata
        Write-Host "Installed $script:ReleaseId"
        Write-Host "BackupDirectory: $backup"
        return [pscustomobject]@{ Release = $script:ReleaseId; Archive = $archive; GameDirectory = $game; BackupDirectory = $backup }
    } catch {
        $failure = $_
        $restoreFailure = $null
        if ($snapshotReady) {
            try { Restore-ReleaseTransaction $archive $archiveSnapshot $archiveHash $liveSnapshots $archiveChanged $changedFiles.ToArray() }
            catch { $restoreFailure = $_ }
        }
        if ($null -ne $metadata) {
            $metadata.status = if ($null -eq $restoreFailure) { 'failed-restored' } else { 'recovery-required' }
            try { Set-ReleaseJson (Join-Path $backup 'metadata.json') $metadata } catch { Write-Warning "Could not update recovery metadata: $_" }
        }
        if ($null -ne $restoreFailure) { throw "Installation failed: $failure. Transaction restoration failed: $restoreFailure. Keep all snapshots: $backup" }
        throw "Installation failed; changed targets were restored and verified: $failure. Snapshots retained: $backup"
    } finally {
        if ([IO.File]::Exists($candidate)) {
            try { Remove-Item -LiteralPath $candidate -Force } catch { Write-Warning "Candidate retained at ${candidate}: $_" }
        }
        $lock.Dispose()
    }
}

function Read-ReleaseBackup([string]$BackupDirectory) {
    $metadataPath = Join-Path $BackupDirectory 'metadata.json'
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    if ($metadata.schemaVersion -ne 1 -or $metadata.releaseId -cne $script:ReleaseId -or $metadata.status -cne 'installed') { throw 'Backup is not an installed combination transaction' }
    if (@($metadata.entries).Count -ne $script:Plugins.Count) { throw 'Unexpected backup plugin count' }
    foreach ($plugin in $script:Plugins) {
        $records = @($metadata.entries | Where-Object { $_.name -ceq $plugin.Name })
        if ($records.Count -ne 1) { throw "Invalid backup plugin record: $($plugin.Name)" }
        $entry = $records[0]
        if ($entry.archiveEntry -cne ('Plugins/' + $plugin.Name) -or $entry.installedSha256 -cne $plugin.Sha256) { throw 'Backup belongs to a different plugin build' }
        $expectedLivePath = [IO.Path]::GetFullPath((Join-Path (Join-Path $metadata.gameDirectory 'Plugins') $plugin.Name))
        if ([IO.Path]::GetFullPath([string]$entry.livePath) -cne $expectedLivePath) { throw 'Backup live path is inconsistent' }
        if ($entry.archiveBackup -cne ('archive-' + $plugin.Name + '.before') -or $entry.liveBackup -cne ('live-' + $plugin.Name + '.before')) { throw 'Unexpected backup file name' }
        if ($entry.archiveBeforePresent -and (Get-ReleaseSha256 (Join-Path $BackupDirectory $entry.archiveBackup)) -cne $entry.archiveBeforeSha256) { throw 'Original archive-entry backup hash mismatch' }
        if ($entry.liveBeforePresent -and (Get-ReleaseSha256 (Join-Path $BackupDirectory $entry.liveBackup)) -cne $entry.liveBeforeSha256) { throw 'Original live-plugin backup hash mismatch' }
    }
    return $metadata
}

function Undo-AcceptedRelease([string]$BackupDirectory) {
    Assert-ReleaseProcessesClosed
    $backup = [IO.Path]::GetFullPath($BackupDirectory)
    $metadataPath = Join-Path $backup 'metadata.json'
    $metadata = Read-ReleaseBackup $backup
    $metadataHash = Get-ReleaseSha256 $metadataPath
    $archive = [string]$metadata.archivePath
    $lock = Enter-ReleaseLock $archive
    $candidate = "$archive.$([guid]::NewGuid()).candidate"
    $attempt = Join-Path $backup ('rollback-attempt-' + [guid]::NewGuid().ToString('N'))
    $archiveChanged = $false
    $changedFiles = [Collections.Generic.List[string]]::new()
    $liveSnapshots = @()
    $snapshotReady = $false
    $metadataChanged = $false
    try {
        if ((Get-ReleaseSha256 $metadataPath) -cne $metadataHash) { throw 'Backup metadata changed before the rollback lock was acquired' }
        $metadata = Read-ReleaseBackup $backup
        $before = Get-ReleaseArchiveHashes $archive
        $archiveHash = Get-ReleaseSha256 $archive
        $replacements = @()
        foreach ($entry in $metadata.entries) {
            if (-not $before.ContainsKey([string]$entry.archiveEntry) -or $before[[string]$entry.archiveEntry] -cne $entry.installedSha256) { throw "Target plugin changed outside this installer; refusing rollback: $($entry.archiveEntry)" }
            $currentState = Get-ReleaseFileState $entry.livePath
            if (-not $currentState.Present -or $currentState.Sha256 -cne $entry.installedSha256) { throw "Live target changed outside this installer; refusing rollback: $($entry.livePath)" }
            $replacements += [pscustomobject]@{ Name = [string]$entry.archiveEntry; Present = [bool]$entry.archiveBeforePresent; Sha256 = [string]$entry.archiveBeforeSha256; Source = (Join-Path $backup $entry.archiveBackup) }
        }
        New-Item -ItemType Directory -Path $attempt | Out-Null
        $archiveSnapshot = Join-Path $attempt 'archive-before-rollback.zip'
        Copy-ReleaseVerified $archive $archiveSnapshot $archiveHash
        Copy-ReleaseVerified $metadataPath (Join-Path $attempt 'metadata-before.json') $metadataHash
        foreach ($entry in $metadata.entries) {
            $path = Join-Path $attempt $entry.name
            Copy-ReleaseVerified $entry.livePath $path $entry.installedSha256
            $liveSnapshots += [pscustomobject]@{ Path = [string]$entry.livePath; Present = $true; Sha256 = [string]$entry.installedSha256; Backup = $path }
        }
        $snapshotReady = $true
        New-ReleaseArchive $archive $candidate $replacements
        Assert-ReleaseArchiveCandidate $before (Get-ReleaseArchiveHashes $candidate) $replacements
        $candidateHash = Get-ReleaseSha256 $candidate
        Assert-ReleaseProcessesClosed
        if ((Get-ReleaseSha256 $archive) -cne $archiveHash -or (Get-ReleaseSha256 $metadataPath) -cne $metadataHash) { throw 'Archive or metadata changed while preparing rollback' }
        [void](Read-ReleaseBackup $backup)
        foreach ($state in $liveSnapshots) { Assert-ReleaseFileState $state.Path $state }
        $archiveChanged = $true
        [IO.File]::Replace($candidate, $archive, [Management.Automation.Language.NullString]::Value)
        foreach ($entry in $metadata.entries) {
            $changedFiles.Add([string]$entry.livePath)
            if ($entry.liveBeforePresent) { Set-ReleaseFile (Join-Path $backup $entry.liveBackup) $entry.livePath }
            else { Remove-Item -LiteralPath $entry.livePath -Force }
            Assert-ReleaseFileState $entry.livePath ([pscustomobject]@{ Present = [bool]$entry.liveBeforePresent; Sha256 = [string]$entry.liveBeforeSha256 })
        }
        if ((Get-ReleaseSha256 $archive) -cne $candidateHash) { throw 'Published rollback archive verification failed' }
        Assert-ReleaseArchiveCandidate $before (Get-ReleaseArchiveHashes $archive) $replacements
        $metadata.status = 'rolled-back'
        $metadata | Add-Member -NotePropertyName rolledBackUtc -NotePropertyValue ([DateTime]::UtcNow.ToString('o'))
        $metadataChanged = $true
        Set-ReleaseJson $metadataPath $metadata
        Write-Host "Restored the two plugins to their state before this installation. Backup retained: $backup"
    } catch {
        $failure = $_
        $restoreErrors = [Collections.Generic.List[string]]::new()
        if ($snapshotReady) {
            try { Restore-ReleaseTransaction $archive $archiveSnapshot $archiveHash $liveSnapshots $archiveChanged $changedFiles.ToArray() }
            catch { $restoreErrors.Add([string]$_) }
            if ($metadataChanged) {
                try {
                    Set-ReleaseFile (Join-Path $attempt 'metadata-before.json') $metadataPath
                    if ((Get-ReleaseSha256 $metadataPath) -cne $metadataHash) { throw 'Metadata restoration hash mismatch' }
                } catch { $restoreErrors.Add([string]$_) }
            }
        }
        if ($restoreErrors.Count -ne 0) { throw "Rollback failed: $failure. Transaction restoration failed: $($restoreErrors -join '; '). Keep recovery snapshots: $attempt" }
        throw "Rollback failed; changed targets were restored and verified: $failure. Snapshots retained: $attempt"
    } finally {
        if ([IO.File]::Exists($candidate)) {
            try { Remove-Item -LiteralPath $candidate -Force } catch { Write-Warning "Candidate retained at ${candidate}: $_" }
        }
        $lock.Dispose()
    }
}

function Test-ReleaseWriteAccess([string[]]$Directories, [string[]]$Files) {
    foreach ($path in $Files) {
        if (-not [IO.File]::Exists($path)) { continue }
        try {
            $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
            $stream.Dispose()
        } catch { return $false }
    }
    foreach ($directory in $Directories) {
        if ([string]::IsNullOrWhiteSpace($directory)) { continue }
        $path = [IO.Path]::GetFullPath($directory)
        while (-not [IO.Directory]::Exists($path)) {
            $parent = [IO.Path]::GetDirectoryName($path)
            if ([string]::IsNullOrWhiteSpace($parent) -or $parent -ceq $path) { return $false }
            $path = $parent
        }
        $probe = Join-Path $path ('.accepted-release-access-' + [guid]::NewGuid().ToString('N') + '.tmp')
        try {
            $stream = [IO.File]::Open($probe, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $stream.Dispose()
            Remove-Item -LiteralPath $probe -Force
        } catch {
            if ([IO.File]::Exists($probe)) { Remove-Item -LiteralPath $probe -Force -ErrorAction SilentlyContinue }
            return $false
        }
    }
    return $true
}

function Invoke-ReleaseElevation([string]$ScriptPath, [hashtable]$Parameters) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { return $false }
    # An encoded command preserves spaces and quotes in user-selected directories.
    $quote = { param([string]$Text) "'" + $Text.Replace("'", "''") + "'" }
    $command = '& ' + (& $quote $ScriptPath)
    foreach ($key in $Parameters.Keys) {
        if ($key -eq 'Elevated' -or [string]::IsNullOrWhiteSpace([string]$Parameters[$key])) { continue }
        $command += ' -' + $key + ' ' + (& $quote ([string]$Parameters[$key]))
    }
    $command += ' -Elevated; if (-not $?) { exit 1 }'
    $encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($command))
    $process = Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', $encoded)
    if ($process.ExitCode -ne 0) { throw "Elevated operation failed with exit code $($process.ExitCode)" }
    return $true
}

Export-ModuleMember -Function Get-ReleaseSha256, Assert-ReleaseGameDirectory, Assert-ReleaseProcessesClosed, Assert-ReleasePackage, Install-AcceptedRelease, Undo-AcceptedRelease, Invoke-ReleaseElevation, Test-ReleaseWriteAccess
