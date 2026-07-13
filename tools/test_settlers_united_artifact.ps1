Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'SettlersUnitedArtifact.psm1') -Force

function Assert-Equal($Expected, $Actual, [string]$Message) {
    if ($Expected -ne $Actual) {
        throw "$Message expected=[$Expected] actual=[$Actual]"
    }
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function New-SampleArchive([string]$SourceDirectory, [string]$ArchivePath) {
    New-Item -ItemType Directory -Path (Join-Path $SourceDirectory 'Plugins/HotkeysConfig') -Force | Out-Null
    [IO.File]::WriteAllBytes(
        (Join-Path $SourceDirectory 'Plugins/SettlersUnited.asi'),
        [byte[]](0x53, 0x55, 0x2d, 0x41, 0x53, 0x49)
    )
    [IO.File]::WriteAllText(
        (Join-Path $SourceDirectory 'Plugins/HotkeysConfig/readme.txt'),
        "sample archive`r`n",
        [Text.UTF8Encoding]::new($false)
    )
    [IO.Compression.ZipFile]::CreateFromDirectory($SourceDirectory, $ArchivePath)
}

function New-DuplicateEntryArchive([string]$ArchivePath) {
    $stream = [IO.File]::Open($ArchivePath, [IO.FileMode]::CreateNew)
    try {
        $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
        try {
            foreach ($value in @('first', 'second')) {
                $entry = $zip.CreateEntry('Plugins/SettlersUnited.asi')
                $writer = [IO.StreamWriter]::new($entry.Open())
                try { $writer.Write($value) } finally { $writer.Dispose() }
            }
        } finally {
            $zip.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) "CampaignCompletionArtifactTests-$([guid]::NewGuid())"
try {
    $artifactDirectory = Join-Path $testRoot 's4_artifacts'
    $sourceDirectory = Join-Path $testRoot 'source'
    $backupDirectory = Join-Path $testRoot 'backup'
    $archive = Join-Path $artifactDirectory 'Plugin_SU.zip'
    $asiOne = Join-Path $testRoot 'CampaignCompletionDebug-one.asi'
    $asiTwo = Join-Path $testRoot 'CampaignCompletionDebug-two.asi'

    New-Item -ItemType Directory -Path $artifactDirectory, $sourceDirectory -Force | Out-Null
    New-SampleArchive $sourceDirectory $archive
    [IO.File]::WriteAllBytes($asiOne, [byte[]](1, 2, 3, 4, 5))
    [IO.File]::WriteAllBytes($asiTwo, [byte[]](9, 8, 7, 6, 5, 4))

    $originalHash = Get-FileSha256 $archive
    $first = Install-CampaignCompletionArtifact $archive $asiOne $backupDirectory
    Assert-Equal $originalHash $first.OriginalSha256 'original hash'
    Assert-Equal (Get-FileSha256 $asiOne) $first.EmbeddedAsiSha256 'embedded ASI'
    Assert-True (Test-Path (Join-Path $backupDirectory 'Plugin_SU.zip.original')) 'backup missing'
    Assert-True (-not (Test-Path "$archive.campaigncompletion.tmp")) 'temp leaked'

    $before = Get-ZipEntryHashes (Join-Path $backupDirectory 'Plugin_SU.zip.original')
    $after = Get-ZipEntryHashes $archive
    Assert-Equal $before['Plugins/SettlersUnited.asi'] $after['Plugins/SettlersUnited.asi'] 'existing ASI changed'
    Assert-Equal $before['Plugins/HotkeysConfig/readme.txt'] $after['Plugins/HotkeysConfig/readme.txt'] 'text changed'
    Assert-Equal (Get-FileSha256 $asiOne) $after['Plugins/CampaignCompletionDebug.asi'] 'diagnostic entry hash'

    $second = Install-CampaignCompletionArtifact $archive $asiTwo $backupDirectory
    Assert-Equal (Get-FileSha256 $asiTwo) $second.EmbeddedAsiSha256 'repatch failed'
    Assert-Equal $originalHash (Get-FileSha256 (Join-Path $backupDirectory 'Plugin_SU.zip.original')) 'backup was replaced'
    Restore-CampaignCompletionArtifact $archive $backupDirectory | Out-Null
    Assert-Equal $originalHash (Get-FileSha256 $archive) 'restore hash'

    $third = Install-CampaignCompletionArtifact $archive $asiOne $backupDirectory
    $patchedHash = Get-FileSha256 $archive
    [IO.File]::WriteAllBytes($archive, [byte[]](0x42, 0x41, 0x44))
    $mutatedHash = Get-FileSha256 $archive
    $restoreRefused = $false
    try {
        Restore-CampaignCompletionArtifact $archive $backupDirectory | Out-Null
    } catch {
        $restoreRefused = $true
    }
    Assert-True $restoreRefused 'restore accepted an externally mutated archive'
    Assert-Equal $mutatedHash (Get-FileSha256 $archive) 'refused restore changed archive'
    Assert-True (-not (Test-Path "$archive.campaigncompletion.tmp")) 'restore refusal leaked temp'

    Copy-Item -LiteralPath (Join-Path $backupDirectory 'Plugin_SU.zip.original') -Destination $archive -Force
    $beforeFailureHash = Get-FileSha256 $archive
    $installRefused = $false
    try {
        Install-CampaignCompletionArtifact $archive (Join-Path $testRoot 'missing.asi') $backupDirectory | Out-Null
    } catch {
        $installRefused = $true
    }
    Assert-True $installRefused 'install accepted a missing ASI'
    Assert-Equal $beforeFailureHash (Get-FileSha256 $archive) 'failed install changed archive'
    Assert-True (-not (Test-Path "$archive.campaigncompletion.tmp")) 'failed install leaked temp'

    $duplicateRoot = Join-Path $testRoot 'duplicate'
    $duplicateArtifactDirectory = Join-Path $duplicateRoot 's4_artifacts'
    $duplicateBackup = Join-Path $duplicateRoot 'backup'
    $duplicateArchive = Join-Path $duplicateArtifactDirectory 'Plugin_SU.zip'
    New-Item -ItemType Directory -Path $duplicateArtifactDirectory -Force | Out-Null
    New-DuplicateEntryArchive $duplicateArchive
    $duplicateHash = Get-FileSha256 $duplicateArchive
    $verificationRefused = $false
    try {
        Install-CampaignCompletionArtifact $duplicateArchive $asiOne $duplicateBackup | Out-Null
    } catch {
        $verificationRefused = $true
    }
    Assert-True $verificationRefused 'install accepted duplicate ZIP entries'
    Assert-Equal $duplicateHash (Get-FileSha256 $duplicateArchive) 'verification failure changed archive'
    Assert-True (-not (Test-Path "$duplicateArchive.campaigncompletion.tmp")) 'verification failure leaked temp'

    Write-Host 'Settlers United archive integration tests passed.'
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
