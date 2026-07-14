param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$expectedOutput = [IO.Path]::GetFullPath((Join-Path $repository "dist"))
$requestedOutput = [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
if ($requestedOutput -ne $expectedOutput) {
    throw "OutputDirectory must be the repository dist directory"
}

$build = [IO.Path]::GetFullPath((Join-Path (Get-Location) $BuildDirectory))
$asi = Join-Path $build "CampaignCompletionDebug.asi"
$configuration = Join-Path $repository "config/CampaignCompletionDebug.ini"
if (-not (Test-Path -LiteralPath $asi -PathType Leaf)) {
    throw "Built ASI not found: $asi"
}
if (-not (Test-Path -LiteralPath $configuration -PathType Leaf)) {
    throw "Diagnostic configuration not found: $configuration"
}

$staging = Join-Path $expectedOutput "staging"
$archive = Join-Path $expectedOutput "CampaignCompletionDebug.zip"
if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}

$plugins = New-Item -ItemType Directory -Path (Join-Path $staging "Plugins") -Force
$settings = New-Item -ItemType Directory -Path (Join-Path $plugins "CampaignCompletion") -Force
Copy-Item -LiteralPath $asi -Destination (Join-Path $plugins "CampaignCompletionDebug.asi")
Copy-Item -LiteralPath $configuration -Destination (Join-Path $settings "CampaignCompletionDebug.ini")

$fixedTimestamp = [DateTime]::SpecifyKind([DateTime]"2026-07-13T00:00:00", "Utc")
Get-ChildItem -LiteralPath $staging -File -Recurse | ForEach-Object {
    $_.LastWriteTimeUtc = $fixedTimestamp
}
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $archive -CompressionLevel Optimal
