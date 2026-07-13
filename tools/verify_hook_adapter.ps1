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

$disassembly = @(& $dumpbin /NOLOGO /DISASM $Binary 2>&1)
if ($LASTEXITCODE -ne 0 -or $disassembly.Count -eq 0) {
    throw "dumpbin /DISASM failed"
}

$resolvedBinary = (Resolve-Path -LiteralPath $Binary).Path
$binaryBytes = [IO.File]::ReadAllBytes($resolvedBinary)
if ($binaryBytes.Length -lt 0x40) {
    throw "ASI is too small to contain a PE header"
}
$peOffset = [BitConverter]::ToInt32($binaryBytes, 0x3C)
if ($peOffset -lt 0 -or $peOffset + 24 -gt $binaryBytes.Length -or
    [BitConverter]::ToUInt32($binaryBytes, $peOffset) -ne 0x00004550) {
    throw "ASI has an invalid PE header"
}
$sectionCount = [BitConverter]::ToUInt16($binaryBytes, $peOffset + 6)
$optionalSize = [BitConverter]::ToUInt16($binaryBytes, $peOffset + 20)
$sectionTable = $peOffset + 24 + $optionalSize
$adapterOffset = $null
$availableBytes = 0
for ($index = 0; $index -lt $sectionCount; ++$index) {
    $section = $sectionTable + 40 * $index
    if ($section + 40 -gt $binaryBytes.Length) {
        throw "ASI section table is truncated"
    }
    $virtualSize = [BitConverter]::ToUInt32($binaryBytes, $section + 8)
    $virtualAddress = [BitConverter]::ToUInt32($binaryBytes, $section + 12)
    $rawSize = [BitConverter]::ToUInt32($binaryBytes, $section + 16)
    $rawPointer = [BitConverter]::ToUInt32($binaryBytes, $section + 20)
    $mappedSize = [Math]::Max([uint64]$virtualSize, [uint64]$rawSize)
    if ($adapterRva -ge $virtualAddress -and
        $adapterRva -lt [uint64]$virtualAddress + $mappedSize) {
        $delta = $adapterRva - $virtualAddress
        if ($delta -ge $rawSize) {
            throw "Adapter RVA is not backed by file bytes"
        }
        $adapterOffset = [uint64]$rawPointer + $delta
        $availableBytes = [uint64]$rawSize - $delta
        break
    }
}
if ($null -eq $adapterOffset -or $adapterOffset -ge $binaryBytes.Length) {
    throw "Adapter export RVA does not map to one PE section"
}
$scanLength = [Math]::Min([uint64]256, $availableBytes)
if ($scanLength -lt 3 -or $adapterOffset + $scanLength -gt $binaryBytes.Length) {
    throw "Adapter machine-code window is unavailable"
}
$verifiedReturn = $false
for ($index = 0; $index -le $scanLength - 3; ++$index) {
    $opcodeOffset = [int]($adapterOffset + $index)
    $opcode = $binaryBytes[$opcodeOffset]
    if ($opcode -eq 0xC3) {
        throw "Adapter reaches a plain ret before ret 0Ch"
    }
    if ($opcode -eq 0xC2) {
        $cleanup = [BitConverter]::ToUInt16(
            $binaryBytes, $opcodeOffset + 1)
        if ($cleanup -ne 0x000C) {
            throw ("Adapter reaches ret {0:X}h instead of ret 0Ch" -f $cleanup)
        }
        $verifiedReturn = $true
        break
    }
}
if (-not $verifiedReturn) {
    throw "Adapter has no ret 0Ch in its bounded machine-code window"
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
