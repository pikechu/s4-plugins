param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,
    [string]$SourceRoot = "."
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Binary -PathType Leaf)) {
    throw "Hook adapter binary missing: $Binary"
}

$exports = @(& dumpbin.exe /NOLOGO /EXPORTS $Binary 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /EXPORTS failed"
}
$exportRows = @($exports | Where-Object {
    $_ -match "CampaignCompletionFixedMapAdapter"
})
if ($exportRows.Count -ne 1) {
    throw "Expected exactly one exported CampaignCompletionFixedMapAdapter, found $($exportRows.Count)"
}

$disassembly = @(& dumpbin.exe /NOLOGO /DISASM $Binary 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /DISASM failed"
}
$symbolRows = @(
    for ($index = 0; $index -lt $disassembly.Count; ++$index) {
        if ($disassembly[$index] -match "CampaignCompletionFixedMapAdapter") {
            $index
        }
    }
)
if ($symbolRows.Count -lt 1) {
    throw "Adapter symbol not found in disassembly"
}

$start = $symbolRows[0]
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
    throw "Adapter must contain exactly one 'ret 0Ch'; returns were: $($returns -join ' | ')"
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
