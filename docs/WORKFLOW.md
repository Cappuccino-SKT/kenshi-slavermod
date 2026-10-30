# Workflow

## Folders

| Path | Role |
|------|------|
| `C:\Users\Felipe\Desktop\SlaverMod` | **Development folder** and git repository (remote: `github.com/Cappuccino-SKT/kenshi-slavermod`). All code changes happen here. |
| `C:\KenshiDev\deps\KenshiLib_Examples_deps` | KenshiLib + Boost 1.60 (headers and libs used by the build). |
| `C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC` and `C:\Program Files\Microsoft SDKs\Windows\v7.1` | VC++ 2010 x64 compiler (Windows SDK 7.1 + KB2519277). |
| `C:\Program Files (x86)\Steam\steamapps\common\Kenshi\mods\SlaverMod` | **Test copy** inside the game. Also the folder the Forgotten Construction Set (FCS) uploads to the Workshop. |
| `C:\KenshiDev\SlaverMod_pessoal` | Parking spot for the personal ini/origins while uploading to the Workshop. |
| `local\` (in the repo, gitignored) | Personal scratch: old trace logs moved here by the deploy script. |

Files in the game's mod folder that are **not** in git: the personal `SlaverMod.ini`, the `saves\` folder (per-save
slave data, v1.3+), `SlaverMod_origins.ini` (v1.0–1.2),
`SlaverMod_trace.log`, `_SlaverMod.info` (Workshop id + tags) and `_SlaverMod.img` (preview; source in `workshop/`).

## Build

```bat
tools\build_plugin.bat . SlaverMod
```

Run it from the repo root (or pass the full repo path). It compiles every `*.cpp` in the root, so the code can be split
into several `.cpp` files without touching the script. Output: `build\SlaverMod.dll`.
`ModConfigMenu.h` must be in the repo root (gitignored; copy from the MCM Workshop item 3813761584, `sdk` folder).

From the bash tool: `cmd //c "C:\\Users\\Felipe\\Desktop\\SlaverMod\\tools\\build_plugin.bat C:\\Users\\Felipe\\Desktop\\SlaverMod SlaverMod"`.

## Deploy to the game (test)

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -Build
```

- Refuses to run while `kenshi_x64` is open (the DLL is locked).
- Copies `SlaverMod.dll`, `RE_Kenshi.json` and `SlaverMod.mod`; copies `SlaverMod.ini` only when the game folder has none.
- Moves the previous `SlaverMod_trace.log` to `local\logs\trace_<timestamp>.log`.
- New ini keys: when a new option is added, also add it by hand to the personal ini in the game folder
  (the plugin uses the default when a key is missing, so this is only needed to change it).

## Logs

- `mods\SlaverMod\SlaverMod_trace.log` — the plugin's own log, only with `Debug=1`. Starts with `startPlugin vX.Y` and a `settings:` line.
- `Kenshi\RE_Kenshi_log.txt` — RE_Kenshi loader log; plugin load errors and `ErrorLog()` lines.
- The update hook logs `time step: N units in 20 real seconds` once per session (the hook's time unit is not real seconds; ~4.4 units per 20 s at normal speed).

## Release

1. Bump the version in `startPlugin()` (`Trace("startPlugin vX.Y")`), README and `workshop/description.txt` if features changed.
2. Commit, push, then `gh release create vX.Y dist/SlaverMod-vX.Y.zip` (zip = `SlaverMod/` with dll, RE_Kenshi.json, .mod, public .ini and `SlaverMod_races.ini`).
3. Workshop update (Felipe does it in the FCS):
   - Before: copy the personal `SlaverMod.ini` to `C:\KenshiDev\SlaverMod_pessoal`, move `SlaverMod_origins.ini`, the `saves\` folder and logs there, put the public ini files (`mod/SlaverMod/SlaverMod.ini` and `SlaverMod_races.ini`) in the game folder.
   - In the FCS info dialog: the Steam ID box shows a placeholder `123456789` until refreshed; the upload still uses `<id>` from `_SlaverMod.info` (3828708454). The status bar must say "Uploading...", not "Creating...".
   - **Do not click any tag** in that dialog: it rebuilds the tag list from the checkboxes and drops the custom `RE_Kenshi` tag (kept only in `_SlaverMod.info`).
   - The FCS replaces the Workshop description with the short `.mod` description: paste `workshop/description.txt` on the Workshop page again after every upload.
   - After: restore the personal ini and origins.
