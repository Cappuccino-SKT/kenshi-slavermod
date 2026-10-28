# SlaverMod — player-owned slaves for Kenshi

A [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) plugin that lets the player's faction own slaves, using Kenshi's native slave system.

**Languages:** English (default) and **Brazilian Portuguese** (`Language=pt`, or *Options > MCM > SlaverMod > Language*).
*Traduzido para português do Brasil — [veja a seção em português](#português-do-brasil).*

## Features

- **Enslave / Free from the right-click menu.** Select one of your members and hold the right mouse button over the target. **Enslave** appears for an unconscious, chained or caged enemy (or one of your own members); **Free** appears for one of your slaves. The selected member becomes the owner.
- **Action with approach.** The owner walks to the target and kneels (first-aid animation) while the game's progress bar fills. The action is cancelled if the owner is knocked out, the target dies or wakes up, or the owner cannot get there.
- **Native slave state** (`setSlaveState`), saved with the game.
- **Locked shackles** (`Prisoner Shackles`; the hive version for hivers). Clothes in the way go to the inventory (or the ground). Freeing removes the shackles.
- **Slaves cannot use weapons.** Any equipped weapon is dropped.
- **New slaves follow and guard** whoever enslaved them (native `BODYGUARD` order, or follow only).
- **Relations.** Enslaving someone from another faction lowers your relation with it (default −5). Freeing gives a little back (default +2, always less than the loss). The origin faction is kept in `SlaverMod_origins.ini`.
- **Rebellion.** Every 7 in-game days:
  - free = number of free members × highest strength among them
  - slaves = number of slaves × highest strength among them
  - if the slaves are stronger, chance = (slaves − free) ÷ slaves (max. 90%)
  - on a rebellion, slaves with **strength 20+** join the **Anti-Slavers**, lose their shackles, get a worst-quality **Iron Club** and attack your free members.
- **In-game settings** with [ModConfigMenu](https://steamcommunity.com/sharedfiles/filedetails/?id=3813761584) (optional): *Options > MCM > SlaverMod*.
- **Debug mode** (off by default): test hotkeys **CTRL+K** (enslave/free the selected character) and **CTRL+SHIFT+R** (rebellion check right away).

## Requirements

- Kenshi 1.0.65 (tested on the Steam version).
- RE_Kenshi 0.3.x (includes KenshiLib).
- Optional: ModConfigMenu, to change the settings in game.

Works with the base game: every item and faction it uses is vanilla. Also tested with *Second Life of Kenshi*.

## Installation

1. Copy the `SlaverMod` folder (`SlaverMod.dll`, `SlaverMod.mod`, `RE_Kenshi.json`, `SlaverMod.ini`) into `Kenshi/mods/`, or subscribe on the Steam Workshop.
2. Enable **SlaverMod** in the launcher's mod list.
3. Adjust the options in `SlaverMod.ini` (commented) or in *Options > MCM*. For Portuguese, set `Language=pt`.

## Building

RE_Kenshi plugins need the **Visual C++ 2010 x64** compiler (toolset v100).

1. Install the **Windows SDK 7.1** (it ships the VC++ 2010 compiler) and the **KB2519277** update.
2. Get [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps) (KenshiLib + Boost 1.60).
3. Copy `ModConfigMenu.h` from the `sdk` folder of ModConfigMenu (Workshop 3813761584) to the root of this repository. It is not included here because it belongs to another author.
4. Run:

   ```bat
   tools\build_plugin.bat . SlaverMod
   ```

   Default paths are at the top of the `.bat` and can be overridden with `KENSHI_VC`, `KENSHI_SDK` and `KENSHI_DEPS`. `tools/include_fix/ammintrin.h` is an empty header that the SDK 7.1 + KB2519277 `intrin.h` asks for.
5. The DLL ends up in `build\SlaverMod.dll`. Copy it into the mod folder together with the files in `mod/SlaverMod/`.

## Technical notes (for whoever continues it)

- **Hooks:**
  - `GameWorld::_NV_mainLoop_GPUSensitiveStuff`: per-frame logic.
  - `ContextMenuGUI::show`: adds the Enslave/Free row to the order menu by copying the last row's geometry. The click uses `eventMouseButtonReleased`, like the game.
- **Closing the menu:** after an option, the game clears three flags in `ContextMenuGUI::optionSelected`. The plugin finds them by reading three `mov byte ptr [rip+x], 0` instructions at offsets 0x97/0x9E/0xA5 of that function. These offsets were measured on the **Steam** build; if the byte pattern does not match (e.g. GOG), the plugin just skips clearing them.
- **Items:** `RootObjectFactory::createItem` refuses to build weapons from the `WEAPON` record. Weapons are built from the **manufacturer**, then the weapon type and the quality model: `createItem(manufacturer, hand(), type, model, 0, 0)`.
- **Rebels:** the squad made with `Faction::createNewEmptyActivePlatoon` has no AI package, so the plugin gives `addGoal(MELEE_ATTACK, target)` periodically.
- **Headers:** do not include `kenshi/Building/Building.h` together with `Platoon.h`; both define `BuildingDesignation`.
- **Addresses:** the RVAs in the KenshiLib headers are GOG 1.0.65. To disassemble the Steam executable, map them with `RE_Kenshi/RVAs/GOG_1.0.65.br` and `Steam_1.0.65.br`, which share the same indices.
- **Text:** every visible string goes through `T("English", "Português")`.
- **Log:** with `Debug=1` the plugin writes `SlaverMod_trace.log` in the mod folder.

## Known limitations

- The war with the Anti-Slavers on a rebellion (`RebellionHostile=1`) is permanent.
- `SlaverMod_origins.ini` is per installation, not per save. Slaves made before that file existed give no relation when freed.
- The action time (`ActionSeconds`) is in the game hook's time unit, not real seconds.
- The ModConfigMenu page changes language only after the game restarts (the mod's messages switch at once).

## License

MIT — see [LICENSE](LICENSE). Use, change and republish it freely.

---

## Português do Brasil

Plugin [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) que deixa a facção do jogador ter escravos de verdade, usando o sistema de escravidão nativo do jogo. **O mod é traduzido para português do Brasil:** use `Language=pt` no `SlaverMod.ini`, ou *Opções > MCM > SlaverMod > Idioma*. O padrão é inglês.

### O que faz

- **Escravizar e libertar pelo menu do botão direito.** Selecione um membro seu e segure o botão direito sobre o alvo. Aparece **Escravizar** para um inimigo desmaiado, acorrentado ou preso (ou para um membro seu), e **Libertar** para um escravo seu. O membro selecionado vira o dono.
- **Ação com aproximação.** O dono anda até o alvo e agacha (animação de primeiros socorros), com a barra de progresso do jogo. A ação é cancelada se o dono desmaiar, se o alvo morrer ou acordar, ou se o dono não conseguir chegar.
- **Estado de escravo nativo** (`setSlaveState`), salvo junto com o jogo.
- **Grilhões trancados** (`Prisoner Shackles`; versão de colmeia para hivers). A roupa que ocupa o lugar deles vai para o inventário (ou para o chão). Ao libertar, os grilhões somem.
- **Escravos não usam armas.** Qualquer arma equipada é largada.
- **O recém-escravizado segue e protege** quem o escravizou (ordem nativa `BODYGUARD`, ou só seguir).
- **Relações.** Escravizar alguém de outra facção reduz a relação com ela (padrão −5). Libertar devolve um pouco (padrão +2, sempre menos que a perda). A facção de origem fica em `SlaverMod_origins.ini`.
- **Rebelião.** A cada 7 dias de jogo:
  - livres = nº de membros livres × maior força entre eles
  - escravos = nº de escravos × maior força entre eles
  - se os escravos forem mais fortes, chance = (escravos − livres) ÷ escravos (máx. 90%)
  - na rebelião, os escravos com **força 20+** vão para os **Abolicionistas**, perdem os grilhões, recebem um **Porrete de Ferro** da pior qualidade e atacam seus membros livres.
- **Configuração dentro do jogo** com o [ModConfigMenu](https://steamcommunity.com/sharedfiles/filedetails/?id=3813761584) (opcional): *Opções > MCM > SlaverMod*.
- **Modo debug** (desligado por padrão): atalhos de teste **CTRL+K** (escravizar ou libertar o personagem selecionado) e **CTRL+SHIFT+R** (verificar rebelião na hora).

### Requisitos

- Kenshi 1.0.65 (testado na versão Steam).
- RE_Kenshi 0.3.x (inclui o KenshiLib).
- Opcional: ModConfigMenu, para mudar as opções dentro do jogo.

Funciona com o jogo base, porque todos os itens e facções que usa são do jogo original. Também foi testado com o *Second Life of Kenshi*.

### Instalação

1. Copie a pasta `SlaverMod` para `Kenshi/mods/`, ou assine o mod na Oficina Steam.
2. Ative **SlaverMod** na lista de mods do launcher.
3. Para português, coloque `Language=pt` no `SlaverMod.ini`, ou escolha *Português (Brasil)* em *Opções > MCM > SlaverMod > Idioma*. Se quiser, mude também `SquadName` para `Escravos`.

### Compilar

Os passos estão na seção *Building* acima: Windows SDK 7.1 + KB2519277, KenshiLib_Examples_deps, `ModConfigMenu.h` copiado da pasta `sdk` do ModConfigMenu, e depois `tools\build_plugin.bat . SlaverMod`. As notas técnicas também estão acima, em inglês.

### Licença

MIT. Use, modifique e republique à vontade.
