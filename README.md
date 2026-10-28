# SlaverMod — escravos para a facção do jogador (Kenshi)

Plugin [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi) que deixa a facção do jogador ter escravos de verdade, usando o sistema de escravidão nativo do jogo.

*English summary at the end.*

## O que faz

- **Escravizar e libertar pelo menu do botão direito.** Selecione um membro seu e segure o botão direito sobre o alvo: aparece **Escravizar** (inimigo desmaiado, acorrentado ou preso, ou um membro seu) ou **Libertar** (um escravo seu). O membro selecionado vira o dono.
- **Ação com aproximação.** O dono anda até o alvo e agacha (animação de primeiros socorros), com a barra de progresso do jogo. A ação é cancelada se o dono desmaiar, o alvo morrer ou acordar, ou se o dono não conseguir chegar.
- **Estado de escravo nativo** (`setSlaveState`). Persiste no save.
- **Grilhões trancados** (`Prisoner Shackles`; versão de colmeia para hivers). A roupa que ocupa o lugar dos grilhões vai para o inventário (ou para o chão). Ao libertar, os grilhões somem.
- **Escravos não usam armas.** Qualquer arma equipada é largada.
- **Recém-escravizado segue e protege** quem o escravizou (ordem nativa `BODYGUARD`, ou só seguir).
- **Relações.** Escravizar alguém de outra facção reduz a relação com ela (padrão −5). Libertar devolve um pouco (padrão +2, sempre menor que a perda). A facção de origem fica em `SlaverMod_origins.ini`.
- **Rebelião.** A cada 7 dias de jogo:
  - livres = nº de membros livres × maior força entre eles
  - escravos = nº de escravos × maior força entre eles
  - se os escravos forem mais fortes, chance = (escravos − livres) ÷ escravos (máx. 90%)
  - na rebelião, os escravos com **força 20+** vão para os **Abolicionistas (Anti-Slavers)**, perdem os grilhões, recebem um **Porrete de Ferro** de pior qualidade e atacam seus membros livres.
- **Configuração dentro do jogo** com o [ModConfigMenu](https://steamcommunity.com/sharedfiles/filedetails/?id=3813761584) (opcional): *Opções > MCM > SlaverMod*.
- **Modo debug** (desligado por padrão): atalhos de teste **CTRL+K** (escravizar/libertar o selecionado) e **CTRL+SHIFT+R** (verificar rebelião na hora).

## Requisitos

- Kenshi 1.0.65 (testado na versão Steam).
- RE_Kenshi 0.3.x (inclui o KenshiLib).
- Opcional: ModConfigMenu, para mudar as opções dentro do jogo.

Funciona com o jogo base: todos os itens e facções usados são do jogo original. Foi testado junto com o *Second Life of Kenshi*.

## Instalação

1. Copie a pasta `SlaverMod` (com `SlaverMod.dll`, `SlaverMod.mod`, `RE_Kenshi.json` e `SlaverMod.ini`) para `Kenshi/mods/`, ou assine o mod na Oficina Steam.
2. Ative **SlaverMod** na lista de mods do launcher.
3. Ajuste as opções em `SlaverMod.ini` (comentado em português) ou em *Opções > MCM*.

## Compilar

Plugins do RE_Kenshi precisam do compilador **Visual C++ 2010 x64** (toolset v100).

1. Instale o **Windows SDK 7.1** (traz o compilador VC++ 2010) e a atualização **KB2519277**.
2. Baixe o [KenshiLib_Examples_deps](https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps) (KenshiLib + Boost 1.60).
3. Copie `ModConfigMenu.h` da pasta `sdk` do ModConfigMenu (Oficina 3813761584) para a raiz deste repositório. Ele não está incluído aqui por ser de outro autor.
4. Rode:

   ```bat
   tools\build_plugin.bat . SlaverMod
   ```

   Os caminhos padrão ficam no início do `.bat` e podem ser trocados pelas variáveis `KENSHI_VC`, `KENSHI_SDK` e `KENSHI_DEPS`. `tools/include_fix/ammintrin.h` é um arquivo vazio que o `intrin.h` do SDK 7.1 + KB2519277 pede.
5. O DLL sai em `build\SlaverMod.dll`. Copie para a pasta do mod junto com os arquivos de `mod/SlaverMod/`.

## Notas técnicas (para quem for continuar)

- **Ganchos:**
  - `GameWorld::_NV_mainLoop_GPUSensitiveStuff`: lógica por quadro.
  - `ContextMenuGUI::show`: acrescenta a linha Escravizar/Libertar ao menu de ordens, copiando a geometria da última linha. O clique usa `eventMouseButtonReleased`, como o jogo.
- **Fechar o menu:** depois de uma opção, o jogo zera três flags em `ContextMenuGUI::optionSelected`. O plugin acha os endereços lendo três instruções `mov byte ptr [rip+x], 0` nos deslocamentos 0x97/0x9E/0xA5 dessa função. Isso foi medido na versão **Steam**. Se o padrão de bytes não bater (por exemplo na GOG), o plugin só não zera essas flags.
- **Itens:** `RootObjectFactory::createItem` não cria armas a partir do registro `WEAPON`. Para armas, o item base é o **fabricante**, depois o tipo da arma e o modelo de qualidade: `createItem(fabricante, hand(), tipo, modelo, 0, 0)`.
- **Rebeldes:** o esquadrão criado com `Faction::createNewEmptyActivePlatoon` não tem pacote de IA. Por isso o plugin dá `addGoal(MELEE_ATTACK, alvo)` periodicamente.
- **Headers:** não inclua `kenshi/Building/Building.h` junto com `Platoon.h`, porque os dois redefinem `BuildingDesignation`.
- **Endereços:** os RVAs nos headers do KenshiLib são da GOG 1.0.65. Para desmontar o executável Steam, use as tabelas `RE_Kenshi/RVAs/GOG_1.0.65.br` e `Steam_1.0.65.br`, que têm os mesmos índices.
- **Log:** com `Debug=1`, o plugin grava `SlaverMod_trace.log` na pasta do mod.

## Limitações conhecidas

- A guerra com os Abolicionistas na rebelião (`RebellionHostile=1`) é permanente.
- `SlaverMod_origins.ini` é por instalação, não por save. Escravos feitos antes de existir esse arquivo não dão relação ao serem libertados.
- O tempo de ação (`ActionSeconds`) está na unidade de tempo do gancho do jogo, não em segundos reais.

## Licença

MIT — veja [LICENSE](LICENSE). Use, modifique e republique à vontade.

---

## English summary

SlaverMod is a RE_Kenshi plugin that lets the player's faction own slaves using Kenshi's native slave system:

- Enslave/free from the hold-right-click order menu.
- The owner walks over, kneels (first-aid animation) and fills the game's progress bar.
- Native slave state (saved with the game).
- Locked shackles, swapping out clothes in the way.
- Slaves cannot use weapons.
- New slaves bodyguard their enslaver.
- Small relation loss with the victim's faction, and a smaller gain when freed.
- A weekly rebellion check (count × strongest, per side). Rebels with strength 20+ join the Anti-Slavers with a rusted iron club and attack.

All options are in `SlaverMod.ini`, or in game with ModConfigMenu (optional). The test hotkeys (CTRL+K, CTRL+SHIFT+R) only work with `DebugMode=1`. Tested on Kenshi 1.0.65 Steam with RE_Kenshi 0.3.x.

Build: VC++ 2010 x64 (Windows SDK 7.1 + KB2519277), KenshiLib_Examples_deps, and `ModConfigMenu.h` from the MCM sdk folder; then run `tools\build_plugin.bat . SlaverMod`. See *Notas técnicas* above for reverse-engineering notes. MIT licensed.
