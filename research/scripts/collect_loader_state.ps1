[CmdletBinding(DefaultParameterSetName = 'Observe')]
param(
    [Parameter(ParameterSetName = 'Launch')]
    [switch]$Launch,

    [Parameter(ParameterSetName = 'Observe')]
    [switch]$ObserveOnly,

    [ValidateRange(1, 120)]
    [int]$WaitSeconds = 120
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputPath = Join-Path $workspace 'research\evidence\loader-state.txt'
$launcherPath = 'C:\Program Files\Settlers United\Settlers United.exe'

if ($Launch) {
    if (-not (Test-Path -LiteralPath $launcherPath -PathType Leaf)) {
        throw "Settlers United launcher missing: $launcherPath"
    }
    Start-Process -FilePath $launcherPath | Out-Null
}

$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
do {
    $game = Get-Process -Name 'S4_Main' -ErrorAction SilentlyContinue |
        Sort-Object StartTime -Descending |
        Select-Object -First 1
    if ($game) { break }
    if ($ObserveOnly) { break }
    Start-Sleep -Milliseconds 500
} while ([DateTime]::UtcNow -lt $deadline)

if (-not $game) {
    throw 'S4_Main.exe is not running. Start the game through Settlers United and retry with -ObserveOnly.'
}

$cim = Get-CimInstance Win32_Process -Filter "ProcessId=$($game.Id)"
$accountName = [Environment]::UserName
function Redact([string]$Value) {
    if ($null -eq $Value) { return '' }
    return $Value.Replace($accountName, '${USER}')
}

$moduleLines = [System.Collections.Generic.List[string]]::new()
$moduleError = ''
try {
    foreach ($module in $game.Modules) {
        $path = Redact $module.FileName
        $moduleLines.Add("module=$($module.ModuleName)|path=$path")
    }
} catch {
    $moduleError = Redact $_.Exception.Message
}

if ($moduleLines.Count -eq 0) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class ReadOnlyProcessModules {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, uint processId);

    [DllImport("psapi.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool EnumProcessModulesEx(
        IntPtr process, [Out] IntPtr[] modules, uint bytes,
        out uint bytesNeeded, uint filterFlag);

    [DllImport("psapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern uint GetModuleFileNameEx(
        IntPtr process, IntPtr module, StringBuilder filename, int size);

    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr handle);
}
'@

    $processHandle = [ReadOnlyProcessModules]::OpenProcess(0x410, $false, [uint32]$game.Id)
    if ($processHandle -eq [IntPtr]::Zero) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        $moduleError = "OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ) failed; win32_error=$errorCode. Module load state is Unknown, not absent."
    } else {
        try {
            $modules = New-Object IntPtr[] 2048
            $bytesNeeded = [uint32]0
            $bytesAvailable = [uint32]($modules.Length * [IntPtr]::Size)
            $ok = [ReadOnlyProcessModules]::EnumProcessModulesEx(
                $processHandle, $modules, $bytesAvailable,
                [ref]$bytesNeeded, 3)
            if (-not $ok) {
                $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
                $moduleError = "EnumProcessModulesEx failed; win32_error=$errorCode. Module load state is Unknown, not absent."
            } else {
                $moduleCount = [Math]::Min(
                    [int]($bytesNeeded / [IntPtr]::Size), $modules.Length)
                for ($index = 0; $index -lt $moduleCount; $index++) {
                    $builder = New-Object Text.StringBuilder 32768
                    $length = [ReadOnlyProcessModules]::GetModuleFileNameEx(
                        $processHandle, $modules[$index], $builder, $builder.Capacity)
                    if ($length -gt 0) {
                        $path = Redact $builder.ToString()
                        $name = [IO.Path]::GetFileName($path)
                        $moduleLines.Add("module=$name|path=$path")
                    }
                }
                $moduleError = ''
            }
        } finally {
            [void][ReadOnlyProcessModules]::CloseHandle($processHandle)
        }
    }
}

$sortedModules = $moduleLines | Sort-Object { $_.ToLowerInvariant() } -Unique
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('# Settlers United baseline loader state')
$lines.Add("generated_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))")
$lines.Add("process_name=$($game.ProcessName).exe")
$lines.Add("process_id=$($game.Id)")
$lines.Add("executable_path=$(Redact $cim.ExecutablePath)")
$lines.Add("parent_process_id=$($cim.ParentProcessId)")
$lines.Add("command_line=$(Redact $cim.CommandLine)")
$lines.Add("collection_mode=$(if ($Launch) { 'Launch' } else { 'ObserveOnly' })")
$lines.Add("module_count=$($sortedModules.Count)")
if ($moduleError) { $lines.Add("module_error=$moduleError") }
$lines.Add('')
$sortedModules | ForEach-Object { $lines.Add($_) }
$utf8WithBom = New-Object Text.UTF8Encoding $true
[IO.File]::WriteAllText($outputPath, (($lines -join "`n") + "`n"), $utf8WithBom)
