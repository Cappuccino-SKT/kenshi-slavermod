# Copies the built plugin into the game's mod folder for testing.
# Usage: powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 [-KenshiDir <path>] [-Build]
#   -Build   runs tools\build_plugin.bat first
# The game must be closed: Windows locks the loaded DLL.
# SlaverMod.mod (game data: records such as game starts) and lang\*.txt (translations) always come from the repository.
# SlaverMod.ini (defaults) is always copied; personal settings are in SlaverMod_user.ini, never touched.
# SlaverMod_races.ini and SlaverMod_lines.ini are only copied when the game folder has none.
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
# SlaverMod.ini holds the defaults (like a Workshop update, always replaced); personal settings live in
# SlaverMod_user.ini, which is never shipped. A personal SlaverMod.ini from before 1.17 becomes the user file once.
$ini = Join-Path $dest 'SlaverMod.ini'
$userIni = Join-Path $dest 'SlaverMod_user.ini'
if ((Test-Path $ini) -and -not (Test-Path $userIni)) {
    Copy-Item $ini $userIni
    Write-Host 'Personal SlaverMod.ini kept as SlaverMod_user.ini'
}
Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod.ini') $dest -Force
$races = Join-Path $dest 'SlaverMod_races.ini'
if (-not (Test-Path $races)) { Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod_races.ini') $dest }
$lines = Join-Path $dest 'SlaverMod_lines.ini'
if (-not (Test-Path $lines)) { Copy-Item (Join-Path $repo 'mod\SlaverMod\SlaverMod_lines.ini') $dest }

# translations (lang\*.txt) always come from the repository
$langSrc = Join-Path $repo 'mod\SlaverMod\lang'
$langDest = Join-Path $dest 'lang'
New-Item -ItemType Directory -Force $langDest | Out-Null
Copy-Item (Join-Path $langSrc '*.txt') $langDest -Force

# item meshes, textures and icons (the slave collar, v1.18) always come from the repository
Copy-Item (Join-Path $repo 'mod\SlaverMod\items') $dest -Recurse -Force

Write-Host "Deployed to $dest"
