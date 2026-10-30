# Kenshi / KenshiLib calls that are known to work

Target: Kenshi **1.0.65 Steam**, RE_Kenshi 0.3.5, KenshiLib 0.5.0. Compiler: VC++ 2010 x64 (no C++11 beyond
what VS2010 has: `nullptr` and `auto` work, range-for and lambdas-with-captures-in-templates are risky).
Headers: `C:\KenshiDev\deps\KenshiLib_Examples_deps\KenshiLib\Include`.

Global entry point: `ou` (`GameWorld*`, from `kenshi/Globals.h`). Useful members: `ou->player` (`PlayerInterface*`),
`ou->factionMgr`, `ou->gamedata`, `ou->theFactory`, `ou->isPaused()`, `ou->getTimeStamp_inGameHours().getTotalDays()`.

## Hooks

```cpp
KenshiLib::AddHook(KenshiLib::GetRealAddress(&Class::method), my_hook, (void**)&orig);
```

| Hook | Use |
|------|-----|
| `GameWorld::_NV_mainLoop_GPUSensitiveStuff(float time)` | Per-frame logic. Call `orig` first. `time` is a scaled step, not seconds. |
| `ContextMenuGUI::show(const lektor<int>&, const std::string&, bool)` | Hold-right-click order menu; add rows after `orig`. |
| `SaveManager::saveGame` / `loadGame` / `newGame` | Per-save data (see Persistence). |

Candidates not tried yet: `Character` damage functions in `CharBody.h` / `MedicalSystem.h`.

## Characters and the player

| Need | Call |
|------|------|
| All player characters | `ou->player->getAllPlayerCharacters()` → `lektor<Character*>` (`size()`, `stuff[i]`) |
| Selected character | `ou->player->selectedCharacter.getCharacter()` (see `SelectedPlayerCharacter()`) |
| Selected member nearest to a point | `ou->player->getNearestSelectedCharacterTo(pos)` (with a group selected, `selectedCharacter` can be far away) |
| A faction's nearest town | `f->warMgr->getNearestTown(pos)` (`kenshi/FactionWarMgr.h`; null for factions without towns, e.g. Drifters) |
| Squad home town | `platoon->ownerships.setHomeTown(town, SQ_ROAMING)` |
| Faction | `c->getFaction()`, `ou->player->getFaction()`, `ou->factionMgr->getFactionByStringID(sid)` |
| Alive / awake | `c->isDead()`, `c->medical.isUnconcious()` |
| Prisoner | `c->inSomething == IN_PRISON`, `c->isChainedMode()`, `c->setChainedMode(false, hand)` |
| Position / size | `c->getPosition()` (`Ogre::Vector3`), see `ReachDistance()` |
| Strength | `c->getStats()->getStat(STAT_STRENGTH, true)` (see `StrengthOf`) |
| Race | `c->getRace()` (`RaceData*`, `kenshi/RaceData.h`) — not used yet; check its `data->stringID`. |
| Handle | `hand h(c)`; `h.getCharacter()`; `h.index`, `h.serial` |

## Slave state

```cpp
c->getStateBroadcast()->setSlaveState(IS_SLAVE);   // or NOT_SLAVE; the flag the game itself uses
c->changeSlaveOwner(hand(owner));                   // only records the owner
c->isSlave() == IS_SLAVE
c->setSlaveAIJob(bool)                              // native slave-camp work behaviour (experimental)
```

Recruiting a prisoner into the player's faction, inside a given squad:

```cpp
ou->player->setCurrentPlatoon(squad->me);   // squad = ActivePlatoon*, created with ou->player->createSquad()
lektor<Character*> list;  list.stuff = arr; list.count = 1; list.maxSize = 1;
ou->player->recruit(list, false);
list.stuff = nullptr; list.count = 0; list.maxSize = 0;   // do not let lektor free the stack array
ou->player->setCurrentPlatoon(previous);
```

## Distances

World units: a human's radius is about 4 (logged by the approach code), so 1 unit is roughly 10 cm.
Defaults: reach 20, supervision 500, runaway safe distance 1500, rebel chase 1000.

## Orders and AI

```cpp
c->addOrder(0, MOVE_CUS_ORDERED, 0, false, true, pos);           // walk to a point (player-style order)
c->addOrder(0, BODYGUARD, owner, false, true, owner->getPosition()); // or FOLLOW_PLAYER_ORDER
c->addGoal(MELEE_ATTACK, target);                                  // AI goal; squads with no AI package need it re-issued
c->addGoal(RUN_AWAY_FORCED, chaser);                               // flee (v1.4; also RUN_AWAY, TOTAL_ESCAPE, RUN_AWAY_HOMETOWN — pick with EscapeGoal)
```

`SlaveStateEnum`: `NOT_SLAVE`, `IS_SLAVE`, `ESCAPING_SLAVE`, `EX_SLAVE`. Runaways get `ESCAPING_SLAVE`; the game has
events for spotted escaped slaves (`EV_ESCAPED_EX_SLAVE_SPOTTED`) and a dialog condition `DC_IS_ESCAPED_SLAVE`.

Factions useful for slavery (`rebirth.mod` unless noted): Slave Traders `1084-gamedata.base`, Slave Hunters
`55101-rebirth.mod`, Slaves `57740-rebirth.mod`, Rebirth Slaves `49392-rebirth.mod`, Drifters `51707-rebirth.mod`,
Anti-Slavers `58229-Dialogue.mod`. Full list: `rtype 10` records in `rebirth.mod`/`Dialogue.mod`.

## Factions and relations

```cpp
f->relations->getFactionRelation(pf);       // -100..100
f->relations->affectRelations(pf, amount, 1.0f);
f->relations->isEnemy(pf); f->relations->declareWar(pf);
Platoon* p = f->createNewEmptyActivePlatoon(0, false, pos);   // new squad of faction f
c->setFaction(f, p->getActivePlatoon());                      // move a character into it
```

## Items

```cpp
GameData* gd = ou->gamedata.getData("SHACKLES", ARMOUR);
Item* it = ou->theFactory->createItem(gd, hand(), 0, 0, 0, 0);
c->inventory->addItem(it, 1, dropOnFail, destroyOnFail);
c->inventory->equipItem(it);
c->inventory->getAllSectionsOfType(sections, it->slotType);     // what is worn in that slot
c->inventory->removeItemDontDestroy_returnsItem(item, qty, false);
c->inventory->removeItemAutoDestroy(item, qty);
c->inventory->hasWeaponEquipped();
```

**Weapons:** `createItem` with a `WEAPON` record returns null. Build from the manufacturer:
`createItem(getData(maker, WEAPON_MANUFACTURER), hand(), getData(weapon, WEAPON), getData(model, MATERIAL_SPECS_WEAPON), 0, 0)`.
Defaults used: Iron Club `52295-rebirth.mod`, maker "Unknown" `912-gamedata.base`, model "Rusted Junk" `913-gamedata.base`.

Shackles: `Prisoner Shackles` (`SHACKLES`) and the hive version (`SHACKLES-BODY`), slot 9 (boots). Clothes in that
slot must be removed first, or `equipItem` fails.

## Damage and money

```cpp
#include <kenshi/Damages.h>   // needed to construct Damages
MedicalSystem::HealthPartStatus* limb = c->medical.leftArm;   // also rightArm, leftLeg, rightLeg; getPart(...)
if (!limb->isRobotic()) limb->applyDamage(Damages(cut, blunt, pierce, bleedMult, armourPenetration));
int cats = c->getMoney();  c->takeMoney(n);   // player characters: the faction's money
```

Animation records (rtype 24) readable from `rebirth.mod`/`Dialogue.mod` include `medic`, `kneeling hostage idle`,
`idle_stand_confident`, `carry me`...; there is no punch/kick animation record (unarmed hits are combat techniques).

## Animation and progress bar

```cpp
c->getAnimationClass()->runAnimation("medic", 1.0f, 1.0f);   // call every frame to keep it running
c->getAnimationClass()->stopAnimation("medic");
FloatingProgressBar* bar = gui->createFloatingProgressBar();   // gui = ForgottenGUI
bar->setCaption(text); bar->setProgress(0..1); bar->setPosition(c->getPosition());
gui->destroy(bar);
```

## MyGUI

- Create on the root: `MyGUI::Gui::getInstancePtr()->createWidget<T>(skin, x, y, w, h, align, layer, name)`.
- Skins seen working: `Kenshi_FloatingPanelSkin`, `Kenshi_GenericTextBoxFlat`, `Kenshi_Button1`, `PanelEmpty`,
  `Kenshi_WindowCX`, `Kenshi_MultiListBox`. Layers: `Popup`, `Window`.
- The squad bar is the TabControl whose name ends in `SquadPanel`.
- The notice font has no accented letters: keep on-screen text ASCII.

## Order menu (ContextMenuGUI)

- After `orig`, `menu->optionsList` holds one child per row: `PanelEmpty` row with a `Kenshi_Button1` button.
- New row = copy of the last row's geometry, one row lower; grow `getMainWidget()` and `optionsList` by one row height.
- The game's buttons react to `eventMouseButtonReleased`; use the same event.
- The target: `menu->contextMenuTarget` (`hand`).
- Closing after our option: `ou->player->contextMenu.showContextMenu(false, 0)`, then clear the three byte flags that
  `optionSelected` clears. They are found at startup by reading `C6 05 disp32 00` at offsets 0x97/0x9E/0xA5 of
  `ContextMenuGUI::optionSelected` (Steam build); on mismatch they are skipped.

## Headers that conflict

- `kenshi/gui/ContextMenu.h` with `PlayerInterface.h` → copy the class locally.
- `kenshi/Building/Building.h` with `Platoon.h` → `BuildingDesignation` redefined. Read fields by offset instead
  (e.g. `Building::specialFunction` at +0x158) and declare needed methods locally.
- `kenshi/Town.h` includes `Building/Building.h` → same conflict. Keep `TownBase` forward-declared; it derives from `RootObject` first, so `reinterpret_cast<RootObjectBase*>(town)` gives the subject for `addGoal`.
- `OgreConfig.h` not found → add `Include\ogre` to the include path (done in `build_plugin.bat`).

## ModConfigMenu

- Export `extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)`.
- `api->beginMod(api, id, title, iniPath, &ReloadSettings)` then `section`, `toggle`, `integer`, `number`, `choice`, `text`, `hotkey`.
- Strings are UTF-8. Flag `MCM_RESTART` marks options that need a restart. MCM writes with `WritePrivateProfileString`.
- `api->version >= 2` adds `api->info(...)`.

## Persistence (v1.3 — hooks in place, confirm in game)

- `SaveManager` (`kenshi/SaveManager.h`; every member is `public` in the header even where the comment says private):
  - `int saveGame(const std::string& location, const std::string& name)` — hooked; after `orig`, the plugin writes its file for `name`.
  - `int loadGame(const std::string& location, const std::string& name)` — hooked; before `orig`, the plugin resets world state and reads its file.
  - `void newGame(const std::string& startId)` — hooked; clears the records.
  - `SaveManager::getSingleton()->getCurrentGame()` — current save name; used as a fallback when no hook ran.
- `SaveFileSystem::getActiveSave()` also returns the save name (not used).
- Full handle: `hand(index, serial, type, container, containerSerial)` rebuilds one; the plugin stores it as `type.container.containerSerial.index.serial`.
- Character key: `hand` has `toString`/`fromString` and a constructor from loaded state, i.e. the game itself saves
  handles; `index-serial` is used as the key. The record also keeps the name, and a name change is logged.
- Still to confirm with the trace log: that quicksave/autosave go through `saveGame`, that loading from the main menu
  goes through `loadGame`, and that `name` is the plain save name.
