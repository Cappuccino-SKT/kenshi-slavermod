# Kenshi data files (.mod / .base)

## Format (FCS 16/17) — `tools/re/kmod.py`

```python
import kmod
m = kmod.read(path)          # dict: type, header (type 17), lastid, records, tail
kmod.write(m, path)          # lossless round-trip
```

Header (type 17): `version`, `author`, `description`, `dependencies`, `references` (comma-separated mod file names).
Each record: `inst`, `rtype`, `id`, `name`, `sid` (string ID, e.g. `1532934-rebirth.mod`), `flags`, then typed
fields `bools`, `floats`, `ints`, `vec3`, `vec4`, `strings`, `files`, list fields `extra` (`[(listName, [(sid, 12 bytes)])]`)
and `instances`. Values stay as raw bytes (`struct.unpack('<i', ...)`).

Notes:
- `gamedata.base` and `Newwworld.mod` are type 16 and `kmod.read` returns no records for them (format differs);
  most vanilla records that matter here live in `rebirth.mod` and `Dialogue.mod`. Fix the reader before relying on them.
- An override record uses the **same sid** as the original and only lists what it changes; list fields merge across mods.
- Changing another mod's records needs that mod in the `references` header field.
- A local edit to a Workshop item is wiped when Steam re-downloads it; prefer putting records in `SlaverMod.mod`.

## Record types seen

| rtype | Kind |
|-------|------|
| 7 | RACE |
| 64 | NEW_GAME_STARTOFF (game start) |

## Game starts (rtype 64)

Example — vanilla "The Slaves" (`1532934-rebirth.mod`):

| Field | Kind | Value |
|-------|------|-------|
| `money` | int | 0 |
| `start pos X`, `start pos Z` | int | 4000, 4000 |
| `force start pos` | bool | 0 |
| `description` | string | text shown in the start menu |
| `difficulty`, `style` | string | "Hard", "RPG" |
| `force race` | list | allowed races (empty = any) |
| `squad` | list | squad template(s) with the starting characters (`1532935-rebirth.mod`) |
| `town` | list | starting town (`18921-Newwworld.mod`) |

"The Wandering Trader" (`1994-gamedata.base`): `squad` `47928-rebirth.mod`, `town` `55661-rebirth.mod`.
A new start for SlaverMod = new rtype 64 record in `SlaverMod.mod` + its own squad template (characters, items,
faction) + a slave camp as `town`. Details to be worked out in the roadmap phase.

## Useful IDs

| What | sid |
|------|-----|
| Anti-Slavers faction | `58229-Dialogue.mod` |
| Iron Club (weapon) | `52295-rebirth.mod` |
| Manufacturer "Unknown" | `912-gamedata.base` |
| Weapon model "Rusted Junk" | `913-gamedata.base` |
| Prisoner Shackles / hive version | `SHACKLES` / `SHACKLES-BODY` |
