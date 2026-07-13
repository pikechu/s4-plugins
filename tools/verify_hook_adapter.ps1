param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,
    [string]$SourceRoot = "."
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Binary -PathType Leaf)) {
    throw "Hook adapter binary missing: $Binary"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio/Installer/vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "vswhere.exe is unavailable"
}
$dumpbin = @(& $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find "VC\Tools\MSVC\**\bin\Hostx64\x86\dumpbin.exe")[0]
if ([string]::IsNullOrWhiteSpace($dumpbin) -or
    -not (Test-Path -LiteralPath $dumpbin -PathType Leaf)) {
    throw "Win32 dumpbin.exe is unavailable"
}

$exports = @(& $dumpbin /NOLOGO /EXPORTS $Binary 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /EXPORTS failed"
}
$exportRows = @($exports | Where-Object {
    $_ -match "CampaignCompletionFixedMapAdapter"
})
if ($exportRows.Count -ne 1) {
    throw "Expected exactly one exported CampaignCompletionFixedMapAdapter, found $($exportRows.Count)"
}
$exportMatch = [regex]::Match(
    $exportRows[0], "^\s*\d+\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+")
if (-not $exportMatch.Success) {
    throw "Could not parse adapter RVA from export row: $($exportRows[0])"
}
$adapterRva = [Convert]::ToUInt64($exportMatch.Groups[1].Value, 16)

$headers = @(& $dumpbin /NOLOGO /HEADERS $Binary 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /HEADERS failed"
}
$imageBaseRow = @($headers | Where-Object {
    $_ -match "^\s*[0-9A-Fa-f]+\s+image base(?:\s|$)"
})
if ($imageBaseRow.Count -ne 1) {
    throw "Could not identify one PE image base"
}
$imageBaseMatch = [regex]::Match(
    $imageBaseRow[0], "^\s*([0-9A-Fa-f]+)\s+image base(?:\s|$)")
$imageBase = [Convert]::ToUInt64($imageBaseMatch.Groups[1].Value, 16)
$adapterAddress = "{0:X8}" -f ($imageBase + $adapterRva)
$adapterRvaAddress = "{0:X8}" -f $adapterRva

$disassembly = @(& $dumpbin /NOLOGO /DISASM $Binary 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /DISASM failed"
}
$addressRows = @(
    for ($index = 0; $index -lt $disassembly.Count; ++$index) {
        if ($disassembly[$index] -match
            "^\s*(?:$adapterAddress|$adapterRvaAddress)\s*:") {
            $index
        }
    }
)
if ($addressRows.Count -ne 1) {
    throw "Adapter VA $adapterAddress / RVA $adapterRvaAddress not found exactly once in disassembly"
}

$start = $addressRows[0]
$thunkMatch = [regex]::Match(
    $disassembly[$start],
    "^\s*[0-9A-Fa-f]{8}\s*:\s+E9\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\s+([0-9A-Fa-f]{2})\b")
if ($thunkMatch.Success) {
    $relativeBits = [Convert]::ToUInt64($thunkMatch.Groups[1].Value, 16) +
        ([Convert]::ToUInt64($thunkMatch.Groups[2].Value, 16) -shl 8) +
        ([Convert]::ToUInt64($thunkMatch.Groups[3].Value, 16) -shl 16) +
        ([Convert]::ToUInt64($thunkMatch.Groups[4].Value, 16) -shl 24)
    $relative = [int64]$relativeBits
    if (($relativeBits -band 0x80000000) -ne 0) {
        $relative -= 0x100000000
    }
    $implementationAddress = "{0:X8}" -f (
        [int64]($imageBase + $adapterRva) + 5 + $relative)
    $implementationRows = @(
        for ($index = 0; $index -lt $disassembly.Count; ++$index) {
            if ($disassembly[$index] -match "^\s*$implementationAddress\s*:") {
                $index
            }
        }
    )
    if ($implementationRows.Count -ne 1) {
        throw "Incremental-link target $implementationAddress was not found exactly once"
    }
    $start = $implementationRows[0]
}
$end = [Math]::Min($start + 300, $disassembly.Count - 1)
for ($index = $start + 1; $index -le $end; ++$index) {
    if ($disassembly[$index] -match "\bENDP\b") {
        $end = $index
        break
    }
}
$adapterBody = @($disassembly[$start..$end])
$returns = @($adapterBody | Where-Object { $_ -match "\bret\b" })
$ret0c = @($returns | Where-Object {
    $_ -match "\bret\s+(?:0*0C(?:h)?)\b"
})
if ($ret0c.Count -ne 1 -or $returns.Count -ne 1) {
    throw "Adapter must contain exactly one 'ret 0Ch'; returns were: $($returns -join ' | '); body: $($adapterBody -join ' | ')"
}

$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $SourceRoot "src") `
    -Recurse -File -Include *.cpp,*.h)
$jmpReferences = @($sourceFiles | Select-String -Pattern "hlib::JmpPatch")
if ($jmpReferences.Count -ne 0) {
    throw "JmpPatch is forbidden: $($jmpReferences.Path -join ', ')"
}
$construction = @($sourceFiles | Select-String -Pattern `
    "make_unique\s*<\s*hlib::CallPatch\s*>")
if ($construction.Count -ne 1) {
    throw "Expected exactly one CallPatch construction, found $($construction.Count)"
}
$expectedSource = [IO.Path]::GetFullPath(
    (Join-Path $SourceRoot "src/hook/HlibCallPatchBackend.cpp"))
if ([IO.Path]::GetFullPath($construction[0].Path) -ne $expectedSource) {
    throw "CallPatch construction is outside HlibCallPatchBackend.cpp"
}

Write-Host "Verified one exported x86 adapter with ret 0Ch and one CallPatch construction."
