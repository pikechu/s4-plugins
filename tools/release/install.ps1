[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameDirectory,
    [string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United',
    [string]$BackupDirectory,
    [switch]$Elevated
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'ReleaseArchive.psm1') -Force
Assert-ReleaseProcessesClosed
Assert-ReleasePackage $PSScriptRoot
Assert-ReleaseGameDirectory $GameDirectory
$archive = Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'
$plugins = Join-Path $GameDirectory 'Plugins'
$backupParent = if ([string]::IsNullOrWhiteSpace($BackupDirectory)) { Join-Path $env:LOCALAPPDATA 'Settlers4Plugins/backups' } else { $BackupDirectory }
$canWrite = Test-ReleaseWriteAccess -Directories @([IO.Path]::GetDirectoryName($archive), $plugins, $backupParent) -Files @($archive, (Join-Path $plugins 'CampaignCompletionDebug.asi'), (Join-Path $plugins 'PileChainRepair.asi'))
if (-not $Elevated -and -not $canWrite -and (Invoke-ReleaseElevation $PSCommandPath @{
    GameDirectory = $GameDirectory; SettlersUnitedDirectory = $SettlersUnitedDirectory; BackupDirectory = $BackupDirectory
})) { return }
Install-AcceptedRelease -PackageDirectory $PSScriptRoot -SettlersUnitedDirectory $SettlersUnitedDirectory -GameDirectory $GameDirectory -BackupDirectory $BackupDirectory
