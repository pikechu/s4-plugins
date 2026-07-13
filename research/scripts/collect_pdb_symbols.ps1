[CmdletBinding()]
param(
    [string]$WorkspaceWsl = "/mnt/f/claude projects/thesettler4plugin",
    [string]$PdbWsl = "/mnt/f/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/thesettlers4/S4_MainR.pdb",
    [string]$ExeWsl = "/mnt/f/Program Files (x86)/Ubisoft/Ubisoft Game Launcher/games/thesettlers4/S4_Main.exe",
    [string]$PdbWindows = "F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\S4_MainR.pdb",
    [string]$ExeWindows = "F:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\games\thesettlers4\S4_Main.exe"
)

$ErrorActionPreference = "Stop"

$workspaceWindows = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$manifestPath = Join-Path $workspaceWindows "research\evidence\manifest.sha256"
$outputPath = Join-Path $workspaceWindows "research\evidence\pdb-symbols.txt"
$rawDirectory = Join-Path $workspaceWindows "research\evidence\raw"
$rawPath = Join-Path $rawDirectory "pdb-dump.txt"
New-Item -ItemType Directory -Force -Path $rawDirectory | Out-Null

$manifest = Get-Content -LiteralPath $manifestPath -Raw
$pdbHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $PdbWindows).Hash.ToLowerInvariant()
$exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExeWindows).Hash.ToLowerInvariant()
if ($manifest -notmatch [regex]::Escape("$pdbHash  $PdbWsl")) {
    throw "PDB hash does not match manifest"
}
if ($manifest -notmatch [regex]::Escape("$exeHash  $ExeWsl")) {
    throw "EXE hash does not match manifest"
}

$toolWsl = "$WorkspaceWsl/research/vendor/llvm-pdbutil/usr/lib/llvm-14/bin/llvm-pdbutil"
$libraryWsl = "$WorkspaceWsl/research/vendor/llvm-pdbutil/usr/lib/x86_64-linux-gnu"
$versionPath = Join-Path $rawDirectory "llvm-pdbutil-version.txt"
$versionArguments = @(
    '-e', 'env', "`"LD_LIBRARY_PATH=$libraryWsl`"",
    "`"$toolWsl`"", '--version'
)
$versionProcess = Start-Process -FilePath 'wsl.exe' -ArgumentList $versionArguments `
    -RedirectStandardOutput $versionPath -Wait -PassThru -NoNewWindow
if ($versionProcess.ExitCode -ne 0) { throw "llvm-pdbutil version check failed" }
$toolVersion = (Get-Content -LiteralPath $versionPath -Raw).Trim()

$dumpArguments = @(
    '-e', 'env', "`"LD_LIBRARY_PATH=$libraryWsl`"", "`"$toolWsl`"", 'dump',
    '--summary', '--publics', '--globals', '--modules', '--section-headers',
    '--section-contribs', "`"$PdbWsl`""
)
$dumpProcess = Start-Process -FilePath 'wsl.exe' -ArgumentList $dumpArguments `
    -RedirectStandardOutput $rawPath -Wait -PassThru -NoNewWindow
if ($dumpProcess.ExitCode -ne 0) { throw "llvm-pdbutil dump failed" }

$category = '(Victory|Defeat|GameEnd|Campaign|Mission|MapFile|RandomMap|Multiplayer|Lobby|MainMenu|AfterGame|Player.*(Won|Lost)|State.*Screen)'
$matches = Select-String -LiteralPath $rawPath -Pattern $category -CaseSensitive:$false -Context 1,1

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# Normalized PDB candidate symbols")
$lines.Add("generated_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))")
$lines.Add("tool_version=$($toolVersion -replace "`r?`n", '; ')")
$lines.Add("pdb_sha256=$pdbHash")
$lines.Add("exe_sha256=$exeHash")
$lines.Add("address_format=CodeView section:offset; convert to RVA only with committed section headers")
$lines.Add("name_format=LLVM-rendered public name; decorated RTTI names are preserved verbatim")
$lines.Add("")

$records = foreach ($match in $matches) {
    $context = @($match.Context.PreContext + $match.Line + $match.Context.PostContext)
    ($context -join "`n").TrimEnd()
}
$records | Sort-Object -Unique | ForEach-Object {
    $lines.Add("---")
    $lines.Add($_)
}

$utf8WithBom = New-Object Text.UTF8Encoding $true
[IO.File]::WriteAllText($outputPath, (($lines -join "`n") + "`n"), $utf8WithBom)
