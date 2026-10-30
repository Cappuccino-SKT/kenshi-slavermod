# SlaverMod — developer docs

Notes for whoever works on the code (including the AI assistant that helps maintain it).
Read the file that matches the task before changing anything.

| File | What it covers |
|------|----------------|
| [WORKFLOW.md](WORKFLOW.md) | Folders, build, deploy to the game, logs, testing, releasing (GitHub + Steam Workshop). |
| [ARCHITECTURE.md](ARCHITECTURE.md) | How `SlaverMod.cpp` is organised: hooks, per-frame loop, features, data files. |
| [KENSHI_API.md](KENSHI_API.md) | KenshiLib / game calls that are known to work, with their traps. |
| [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md) | Finding game functions: GOG vs Steam RVAs, disassembly helpers, FCS internals. |
| [MOD_DATA.md](MOD_DATA.md) | The `.mod` file format, `tools/re/kmod.py`, game-start records and useful IDs. |
| [ROADMAP.md](ROADMAP.md) | Planned features split into phases (in Portuguese, the planning language). |

Rules of thumb:

- Every visible string goes through `T("English", "Português")`. English is the default.
- Every new option goes in four places: `Settings` + `LoadSettings()`, the public `mod/SlaverMod/SlaverMod.ini`, `MCM_Describe()`, and the README.
- Test in game before releasing; ask for `SlaverMod_trace.log` (with `Debug=1`) and `RE_Kenshi_log.txt`.
- Keep the docs in step with the code: when a phase lands, update ARCHITECTURE/KENSHI_API and tick it off in ROADMAP.
