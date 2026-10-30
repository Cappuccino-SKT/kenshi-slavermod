# Reverse engineering

## GOG vs Steam addresses

KenshiLib headers list **GOG 1.0.65** RVAs (`// public RVA = 0x...`). RE_Kenshi maps them at runtime, so code that
uses `KenshiLib::GetRealAddress(&Class::method)` works on Steam. To read the **Steam** executable by hand:

- `Kenshi\RE_Kenshi\RVAs\GOG_1.0.65.br` and `Steam_1.0.65.br`: two arrays of 7630 little-endian `uint32` RVAs that
  share indices. GOG RVA → index in the GOG table → same index in the Steam table → Steam RVA.
- The unpacked Steam executable to disassemble: `Kenshi\RE_Kenshi\kenshi_x64.exe`.

## Helper scripts (`tools/re/`)

All need `pefile` and `capstone`. Install once into a private folder and pass it as the first argument:

```
pip install --target C:\KenshiDev\pylib pefile capstone
python tools\re\dis2.py C:\KenshiDev\pylib Name:0xGOGRVA:200
```

| Script | What it does |
|--------|--------------|
| `dis2.py <pylib> name:gogRVA:maxInstr ...` | Disassembles Steam functions given GOG RVAs; annotates calls with KenshiLib names and RIP-relative strings. |
| `callers.py <pylib> <steamRVA>` | Lists call sites of a Steam function (including E9 thunks). |
| `strrefs.py <pylib> <string> [ctx]` | Finds code that loads an exact C string (`lea rip+x`) and shows nearby calls. Good for locating features by their log/UI text. |
| `rvamap.py <pylib> ...` | GOG ↔ Steam RVA conversion. |
| `kmod.py` | `.mod` reader/writer (see MOD_DATA.md). |
| `starts.py`, `start317.py` | Examples: list / edit game-start records (written for the Second Life of Kenshi patch). |
| `race.py`, `factions.py` | Examples: dump race and faction records from the data files. |
| `il.ps1 <exe> [typeFilter]` | Dumps strings, fields and calls used by each method of a .NET executable (used on the FCS). |

Paths inside the scripts point to the Steam install and to `C:\KenshiDev\deps`; adjust if they move.

## Known findings

- `ContextMenuGUI::optionSelected` (Steam) clears three global byte flags with `mov byte ptr [rip+x], 0` at offsets
  0x97, 0x9E, 0xA5. Not clearing them leaves the menu in a half-open state after our option.
- FCS (`forgotten construction set.exe`, .NET): `ModData.Load` reads `<id>` from `_<Mod>.info`; the info dialog's
  Steam ID label keeps its designer text (`123456789`) until refreshed from Steam; `listTags_SelectedIndexChanged`
  rebuilds `tags` from the checked items only (custom tags are lost if any tag is clicked); upload status shows
  "Creating..." for a new item and "Uploading..." for an update.
