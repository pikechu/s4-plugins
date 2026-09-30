<# Compatibility entry point. Build and extract the accepted pair before installing. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameDirectory,
    [Parameter(Mandatory = $true)][string]$PackageDirectory,
    [string]$SettlersUnitedDirectory = 'C:\Program Files\Settlers United',
    [string]$BackupDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$installer = Join-Path $PackageDirectory 'install.ps1'
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw 'Build tools/package_accepted_release.ps1, extract the ZIP, and pass its directory with -PackageDirectory.' }
& $installer -GameDirectory $GameDirectory -SettlersUnitedDirectory $SettlersUnitedDirectory -BackupDirectory $BackupDirectory
