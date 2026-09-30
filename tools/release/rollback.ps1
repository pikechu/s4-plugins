[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BackupDirectory,
    [switch]$Elevated
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'ReleaseArchive.psm1') -Force
Assert-ReleaseProcessesClosed
Assert-ReleasePackage $PSScriptRoot
$metadata = Get-Content -LiteralPath (Join-Path $BackupDirectory 'metadata.json') -Raw | ConvertFrom-Json
$archive = [string]$metadata.archivePath
$plugins = Join-Path ([string]$metadata.gameDirectory) 'Plugins'
$canWrite = Test-ReleaseWriteAccess -Directories @([IO.Path]::GetDirectoryName($archive), $plugins, $BackupDirectory) -Files @($archive, (Join-Path $plugins 'CampaignCompletionDebug.asi'), (Join-Path $plugins 'PileChainRepair.asi'))
if (-not $Elevated -and -not $canWrite -and (Invoke-ReleaseElevation $PSCommandPath @{ BackupDirectory = $BackupDirectory })) { return }
Undo-AcceptedRelease -BackupDirectory $BackupDirectory
