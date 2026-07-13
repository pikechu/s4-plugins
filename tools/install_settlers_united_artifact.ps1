[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$AsiPath,
    [string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$running = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @('S4_Main', 'Settlers United')
})
if ($running.Count -gt 0) {
    throw "Close S4_Main and Settlers United before archive installation: $($running.ProcessName -join ', ')"
}

Import-Module (Join-Path $PSScriptRoot 'SettlersUnitedArtifact.psm1') -Force
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$archive = Join-Path $SettlersUnitedDirectory 'resources/bin/s4_artifacts/Plugin_SU.zip'
$backup = Join-Path $repositoryRoot 'research/backups/settlers-united'
Install-CampaignCompletionArtifact -ArchivePath $archive -AsiPath $AsiPath -BackupDirectory $backup
