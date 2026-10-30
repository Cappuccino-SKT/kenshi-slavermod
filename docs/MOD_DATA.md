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
| 1 | CHARACTER |
| 7 | RACE |
| 10 | FACTION |
| 13 | TOWN |
| 24 | ANIMATION |
| 52 | SQUAD_TEMPLATE |
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
Several `town` entries = one picked at random (vanilla "Nobodies" and "Holy Nation Citizen" do this).
`faction relations` entries are `(faction, (relation -100..100, 0, 0))`.

Squad template (rtype 52): `leader` `(count)`, `squad`/`squad2` `(min, max; 0 = exactly min)`.
Character (rtype 1): ints `combat stats`, `strength`, `unarmed stats`, `ranged stats`, `stealth stats`,
`stats randomise`, `female chance`, `money min/max`, `NPC class` (9 for player start characters); bools `named`,
`unique`; lists `clothing` `(qty, chance%)`, `weapons` `(qty, slot 0=hip 1=back, chance%)`, `weapon level`
(manufacturer), `inventory`, `dialogue package player`, `personality`.
Field meanings: `Kenshi\fcs.def` (the FCS's own definitions, one `[TYPE]` block per record type).

### SlaverMod's start (v1.7)

Built by `tools/build_start.py` (re-run after editing its constants; it replaces its own records):

| sid | rtype | What |
|-----|-------|------|
| `1-SlaverMod.mod` | 64 | "Slave Hunters" start: 500 cats, Hard, relations Slave Traders +50, Slave Hunters +50, Anti-Slavers −30; towns 1073 Slave Farm, 1080 Slave Markets, 1785 Stone Camp, 1533492 South Stone Camp, 1533495 Slave Farm South |
| `2-SlaverMod.mod` | 52 | squad: leader ×1 + hunter ×5 (copy of the Holy Nation Citizen start squad) |
| `3-SlaverMod.mod` | 1 | Slaver Leader (copy of the Holy Citizen start character; Slave Hunter clothes (shorts, sandals, face plates) + Leather Shirt; Iron Club, rusted) |
| `4-SlaverMod.mod` | 1 | Slave Hunter (same, weaker stats, Cloth Shirt) |

Record `id` is 0 and `flags` are copied from the vanilla templates, like the game's own data.

## Useful IDs

| What | sid |
|------|-----|
| Anti-Slavers faction | `58229-Dialogue.mod` |
| Iron Club (weapon) | `52295-rebirth.mod` |
| Manufacturer "Unknown" | `912-gamedata.base` |
| Weapon model "Rusted Junk" | `913-gamedata.base` |
| Prisoner Shackles / hive version | `SHACKLES` / `SHACKLES-BODY` |
