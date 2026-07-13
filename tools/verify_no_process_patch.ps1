param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,
    [string]$SourceRoot = "."
)

$ErrorActionPreference = "Stop"

$binaryPath = (Resolve-Path -LiteralPath $Binary -ErrorAction Stop).Path
$rootPath = (Resolve-Path -LiteralPath $SourceRoot -ErrorAction Stop).Path
$cmakePath = Join-Path $rootPath "CMakeLists.txt"
if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
    throw "CMakeLists.txt is missing below SourceRoot: $rootPath"
}

$bytes = [IO.File]::ReadAllBytes($binaryPath)
if ($bytes.Length -lt 0x40) {
    throw "ASI is too small to contain a PE header"
}
$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
if ($peOffset -lt 0 -or $peOffset + 26 -gt $bytes.Length -or
    [BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x00004550) {
    throw "ASI has an invalid PE header"
}
$machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
$optionalMagic = [BitConverter]::ToUInt16($bytes, $peOffset + 24)
if ($machine -ne 0x014c -or $optionalMagic -ne 0x010b) {
    throw ("Expected PE32 IMAGE_FILE_MACHINE_I386, found machine=0x{0:X4} optional=0x{1:X4}" -f `
        $machine, $optionalMagic)
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

$exports = @(& $dumpbin /NOLOGO /EXPORTS $binaryPath 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /EXPORTS failed"
}
$stopExports = @($exports | Where-Object {
    $_ -match "\bCampaignCompletionDebugStop\b"
})
if ($stopExports.Count -ne 1) {
    throw "Expected exactly one CampaignCompletionDebugStop export, found $($stopExports.Count)"
}

$cmake = [IO.File]::ReadAllText($cmakePath)
$targetMatch = [regex]::Match(
    $cmake,
    '(?s)add_library\s*\(\s*CampaignCompletionDebug\s+SHARED(?<body>.*?)\)')
if (-not $targetMatch.Success) {
    throw "CampaignCompletionDebug SHARED target block is missing"
}
$targetBody = $targetMatch.Groups['body'].Value
$requiredNativeFiles = @(
    'src/native/NativeEventAdmission.cpp',
    'src/native/NativeEventRegistration.cpp',
    'src/native/NativeVictoryEventSubscriber.cpp',
    'src/victory/VictoryEventProbe.cpp'
)
foreach ($file in $requiredNativeFiles) {
    $count = [regex]::Matches(
        $targetBody, [regex]::Escape($file),
        [Text.RegularExpressions.RegexOptions]::IgnoreCase).Count
    if ($count -ne 1) {
        throw "Required native event source must be linked exactly once: $file (found $count)"
    }
}
$admittedNativeSources = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($file in $requiredNativeFiles) {
    if ($file.StartsWith('src/native/', [StringComparison]::OrdinalIgnoreCase)) {
        [void]$admittedNativeSources.Add($file)
    }
}
$linkedNativeSources = [regex]::Matches(
    $targetBody, '(?im)^\s*(?<path>src/native/[^\s\)]+\.(?:cpp|cxx|cc))\s*$')
foreach ($match in $linkedNativeSources) {
    $file = $match.Groups['path'].Value
    if (-not $admittedNativeSources.Contains($file)) {
        throw "Unadmitted native source is linked into CampaignCompletionDebug: $file"
    }
}
$linkedHookSource = [regex]::Match(
    $targetBody, '(?im)^\s*src/hook/[^\s\)]+\.(?:cpp|cxx|cc)\s*$')
if ($linkedHookSource.Success) {
    throw "A src/hook source is linked into CampaignCompletionDebug: $($linkedHookSource.Value.Trim())"
}
$hookEraFiles = @(
    'src/hook/HookSiteLayout.cpp',
    'src/hook/FixedMapLoadHook.cpp',
    'src/hook/HlibCallPatchBackend.cpp',
    'src/hook/MsvcX86WideString.cpp'
)
foreach ($file in $hookEraFiles) {
    if ($targetBody.IndexOf($file, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "Hook-era source is linked into CampaignCompletionDebug: $file"
    }
}

$sourceMatches = [regex]::Matches(
    $targetBody,
    '(?m)^\s*(?<path>src/[^\s\)]+\.(?:cpp|cxx|cc))\s*$')
if ($sourceMatches.Count -eq 0) {
    throw "CampaignCompletionDebug target has no production sources"
}

$queue = [Collections.Generic.Queue[string]]::new()
$visited = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($match in $sourceMatches) {
    $relative = $match.Groups['path'].Value.Replace('/', [IO.Path]::DirectorySeparatorChar)
    $source = [IO.Path]::GetFullPath((Join-Path $rootPath $relative))
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Target source is missing: $source"
    }
    $queue.Enqueue($source)
    $header = [IO.Path]::ChangeExtension($source, '.h')
    if (Test-Path -LiteralPath $header -PathType Leaf) {
        $queue.Enqueue($header)
    }
}

$forbiddenPatchPatterns = @(
    '\bGameDefaultGameEndCheck\b',
    '\bDefaultGameEndCheck\b',
    '\bVICTORY_CONDITION_CHECK\b',
    '\bCStateVictoryScreen\b',
    '\blua_setglobal\b',
    '\blua_settable\b',
    '\bWriteProcessMemory\b',
    '\bVirtualProtect\b',
    'hlib\s*::\s*(?:CallPatch|JmpPatch|NopPatch)',
    '\b(?:CallPatch|JmpPatch|NopPatch)\b',
    '\bFixedMapLoadHook\b',
    '\bHlibCallPatchBackend\b',
    '\bHookSiteLayout\b',
    '\bMsvcX86WideString\b',
    '\bMinHook\b',
    '\bMH_(?:CreateHook|EnableHook|DisableHook|RemoveHook)\b',
    '\bDetour(?:Attach|Detach|TransactionBegin|TransactionCommit)\b'
)
$srcRoot = [IO.Path]::GetFullPath((Join-Path $rootPath 'src'))
while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()
    if (-not $visited.Add($file)) {
        continue
    }
    $content = [IO.File]::ReadAllText($file)
    foreach ($pattern in $forbiddenPatchPatterns) {
        if ([regex]::IsMatch($content, $pattern,
                [Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
            throw "Forbidden behavior token '$pattern' found in target source: $file"
        }
    }

    foreach ($include in [regex]::Matches(
            $content, '(?m)^\s*#\s*include\s*"(?<path>[^"]+)"')) {
        $includePath = $include.Groups['path'].Value.Replace(
            '/', [IO.Path]::DirectorySeparatorChar)
        $candidates = @(
            [IO.Path]::GetFullPath((Join-Path $srcRoot $includePath)),
            [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($file)) $includePath))
        )
        foreach ($candidate in $candidates) {
            if ($candidate.StartsWith($srcRoot,
                    [StringComparison]::OrdinalIgnoreCase) -and
                (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                $queue.Enqueue($candidate)
                break
            }
        }
    }
}

$bridgePath = Join-Path $rootPath "src/lua/SuLuaMapBridge.cpp"
if (-not (Test-Path -LiteralPath $bridgePath -PathType Leaf)) {
    throw "SU Lua bridge source is missing: $bridgePath"
}
$bridge = [IO.File]::ReadAllText($bridgePath)
$forbiddenLuaWrites = @(
    'lua_setglobal', 'lua_rawsetglobal', 'lua_settable',
    'lua_rawsettable', 'lua_dostring', 'lua_dofile', 'lua_dobuffer'
)
foreach ($api in $forbiddenLuaWrites) {
    if ($bridge.IndexOf($api, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "Forbidden Lua write or script API found in bridge: $api"
    }
}

Write-Host "Verified PE32 native event calibration ASI, zero process patches, and read-only Lua bridge."
