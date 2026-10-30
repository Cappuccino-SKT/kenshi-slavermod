# Copies the built plugin into the game's mod folder for testing.
# Usage: powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 [-KenshiDir <path>] [-Build]
#   -Build   runs tools\build_plugin.bat first
# The game must be closed: Windows locks the loaded DLL.
# SlaverMod.mod (game data: records such as game starts) always comes from the repository.
# SlaverMod.ini and SlaverMod_races.ini are only copied when the game folder has none, so personal settings survive.
# The previous SlaverMod_trace.log is moved to local\logs (gitignored) with a timestamp.
param(
    [string]$KenshiDir = $(if ($env:KENSHI_DIR) { $env:KENSHI_DIR } else { 'C:\Program Files (x86)\Steam\steamapps\common\Kenshi' }),
    [switch]$Build
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $KenshiDir 'mods\SlaverMod'

if ($Build) {
    & cmd /c "`"$PSScriptRoot\build_plugin.bat`" `"$repo`" SlaverMod"
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}

if (Get-Process kenshi_x64 -ErrorAction SilentlyContinue) {
    throw 'Kenshi is running. Close the game before deploying.'
}

$dll = Join-Path $repo 'build\SlaverMod.dll'
if (-not (Test-Path $dll)) { throw "Not built yet: $dll" }
New-Item -ItemType Directory -Force $dest | Out-Null

$trace = Join-Path $dest 'SlaverMod_trace.log'
if (Test-Path $trace) {
    $logs = Join-Path $repo 'local\logs'
    New-Item -ItemType Directory -Force $logs | Out-Null
    $stamp = (Get-Item $trace).LastWriteTime.ToString('yyyyMMdd_HHmmss')
    Move-Item $trace (Join-Path $logs "trace_$stamp.log") -Force
    Write-Host "Old trace log moved to local\logs\trace_$stamp.log"
}

Copy-Item $dll $dest -Force
Copy-Item (Join-Path $repo 'mod\SlaverMod\RE_Kenshi.json') $dest -Force
Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod.mod') $dest -Force
$ini = Join-Path $dest 'SlaverMod.ini'
if (-not (Test-Path $ini)) { Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod.ini') $dest }
$races = Join-Path $dest 'SlaverMod_races.ini'
if (-not (Test-Path $races)) { Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod_races.ini') $dest }

Write-Host "Deployed to $dest"
