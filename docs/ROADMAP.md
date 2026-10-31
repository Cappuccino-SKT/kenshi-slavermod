# Planejamento — SlaverMod mais realista

Uma fase por vez: implementar → instalar → testar no jogo → ajustar → próxima fase. Cada fase vira uma versão.
Os números abaixo são **propostas iniciais**; todos viram opções no ini/MCM para ajustar depois dos testes.

| Fase | Versão | Conteúdo | Depende de |
|------|--------|----------|------------|
| 0 | — | Pasta de desenvolvimento, script de instalação, documentação | — |
| 1 | 1.3 | Submissão + dados salvos por save + rebelião usando submissão | — |
| 2 | 1.4 | Supervisão + fuga de escravos | 1 |
| 3 | 1.5 | Punições (formas de aumentar a submissão) | 1 |
| 4 | 1.6 | Variação por raça e sub-raça | 1, 2, 3 |
| 5 | 1.7 | Início de jogo "Caçadores de escravos" | — (só dados) |
| 6 | 1.8 | Comércio de escravos | 1, 4 |

O início de jogo (5) não depende do código e pode ser adiantado. O comércio (6) fica por último porque é o que mais
precisa de pesquisa no jogo.

---

## Fase 0 — Estrutura ✅

- [x] Repositório movido para `Área de Trabalho\SlaverMod`.
- [x] `tools\deploy.ps1`: compila (opcional), confere se o jogo está fechado, copia para `Kenshi\mods\SlaverMod`, guarda o log antigo em `local\logs`.
- [x] `docs\`: fluxo de trabalho, arquitetura, API do Kenshi, engenharia reversa, formato `.mod`, este planejamento.
- [x] Scripts de análise salvos em `tools\re\`.

## Fase 1 — Submissão (v1.3) ✅

Feito: registro por escravo salvo por save (`saves\<save>.txt`), submissão com valor inicial, bônus de prisioneiro e
ganho diário, barra sobre o escravo selecionado, rebelião só com os rebeldes (submissão < 50), avisos agrupados na
mesma caixa, tecla de teste CTRL+SHIFT+S (modo debug).
Testado no jogo (save/load, tecla de teste, rebelião). Ajuste depois do teste: a força mínima do rebelde varia com a
submissão (`RebelMinStrengthDefiant`=10 com submissão 0 até `RebelMinStrength`=20 no limite).

**Ideia:** cada escravo tem **Submissão de 0 a 100**.

| Faixa | Nome |
|-------|------|
| 0–24 | Rebelde |
| 25–49 | Inquieto |
| 50–74 | Obediente |
| 75–100 | Leal |

- **Valor inicial** ao escravizar: 30 (a fase 4 troca pela base da raça). Ganha um bônus se ele estava preso numa jaula ou acorrentado.
- **Mudança com o tempo:** sobe devagar até o teto da faixa "Obediente". Na fase 2, isso só vai acontecer quando ele estiver supervisionado.
- **Na tela:**
  - Ao selecionar um escravo, aparece um painel pequeno com "Submissão 45 — Inquieto".
  - É sempre um painel só, nunca um por escravo.
  - Pode ser desligado no MCM.
- **Rebelião:**
  - Só participam escravos com submissão abaixo de 50, além da força mínima que já existe.
  - O poder dos escravos passa a ser `nº de rebeldes × maior força × (100 − submissão média) / 100`.
  - Escravos leais deixam a rebelião bem menos provável.

**Parte técnica (o mais arriscado da fase):**

- Os dados por escravo precisam ser **salvos junto com cada save**. Hoje o `SlaverMod_origins.ini` vale para a instalação inteira.
- Plano:
  - Pegar o nome do save atual em `SaveFileSystem::getActiveSave()`.
  - Gravar um arquivo por save, em `mods\SlaverMod\saves\<save>.ini`.
  - Gravar ao salvar o jogo (hook no `SaveManager`) e ler ao carregar.
- Achar uma identificação do personagem que continue igual depois de salvar e carregar. Conferir se o `hand` serve, ou achar um ID persistente.
- Passar as origens (facção de onde o escravo veio) para esse mesmo arquivo. Isso resolve a limitação atual.

**Teste:** escravizar, ver o valor; salvar, sair, carregar e conferir que o valor continua; carregar outro save e ver
valores diferentes; forçar rebelião com escravos leais e com escravos rebeldes.

## Fase 2 — Supervisão e fuga (v1.4) ✅

**Testada no jogo.** Supervisão por distância (`SupervisionRange`=500 unidades, ~50 m) ou jaula;
sem vigilância −5/dia; fuga com chance diária ajustável, fugitivo vai para a facção de origem (ou Drifters), estado nativo
`ESCAPING_SLAVE`, foge do membro mais próximo; recaptura com −10 de submissão e sem perda de relação; tecla de teste CTRL+SHIFT+E.

**Supervisão:**

- O escravo está supervisionado quando há um membro livre e acordado do grupo a até **X metros**. Proposta: `SupervisionRange` em torno de 50 m.
- Escravo sem supervisão:
  - perde submissão com o tempo (por exemplo, −5 por dia de jogo);
  - acumula "tempo solto".
- Escravo supervisionado: recupera devagar.
- Escravos dentro de jaula contam sempre como supervisionados.

**Fuga:**

- A cada poucas horas de jogo, cada escravo faz um teste de fuga. A chance:
  - sobe com a submissão baixa e com o tempo sem supervisão;
  - cai com grilhões;
  - é zero dentro de jaula, inconsciente ou ferido demais.
- Quando foge:
  - sai da facção do jogador, voltando para a facção de origem ou para uma facção neutra;
  - corre para longe da base, na direção da cidade mais próxima;
  - aparece um aviso.
- Dá para recapturar: derrubar e escravizar de novo.
- Fugitivo recapturado volta com submissão menor.

**Para pesquisar:** como mandar o NPC fugir de verdade (ordem de movimento, ou AI de fuga do próprio jogo) e se os
grilhões continuam nele.

## Fase 3 — Punição (v1.5) ✅

**Testada no jogo.** Menu com Libertar/Punir/Recompensar; punição com dano de impacto num membro e
rendimento decrescente; recompensa por 100 cats; jaula +10/dia até 75. Não existe animação de soco no jogo: o escravo
ajoelha (`kneeling hostage idle`) enquanto é punido. A Fase 2 também ganhou: limpeza de fugitivos (30 dias ou morte),
fugitivo continua fugindo depois de carregar, quem age é o selecionado mais perto do alvo. Depois dos testes: o
fugitivo perde os grilhões ao fugir (`EscapeRemoveShackles`).

Novas opções no menu do botão direito sobre um escravo seu. O dono anda até ele e faz a ação, como já acontece ao escravizar.

| Ação | Efeito proposto |
|------|-----------------|
| **Punir (surra)** | +15 de submissão, dano leve nos membros. Repetir no mesmo dia rende menos e machuca mais. |
| **Prender na jaula** | Enquanto está numa jaula: +submissão por hora, até um teto. |
| **Recompensar** (opcional) | +5 de submissão, custa comida ou dinheiro. Dá um jeito "não violento" de subir. |

Punir demais pode baixar a submissão de escravos com submissão alta, ou deixar o escravo inquieto. Decidir nos testes.

**Para pesquisar:** como aplicar dano controlado (`MedicalSystem` / `CharBody`) e uma animação de ataque sem iniciar
combate de verdade.

## Fase 4 — Raças e sub-raças (v1.6) ✅

**Testada no jogo.** Tabela embutida + `SlaverMod_races.ini` (Start, Cap, Gain, Loss, Escape,
Punish por ID de raça; `[default]` para as demais). Liga/desliga com `RaceTraits`.

Uma tabela em `SlaverMod_races.ini`, editável, com estes campos por raça:

- submissão inicial;
- velocidade com que a submissão muda;
- multiplicador de fuga;
- efeito das punições.

Proposta inicial:

| Raça | Submissão inicial | Fuga | Observação |
|------|-------------------|------|------------|
| Colmeia — operário (Worker Drone) | 70 | baixa | obedece ao "chefe" |
| Colmeia — soldado (Soldier Drone) | 50 | média | |
| Colmeia — príncipe | 30 | média | |
| Rainha (QueenBirth) | 10 | alta | só se o mod estiver ativo |
| Shek | 15 | alta | orgulhosos, punição rende menos |
| Greenlander | 40 | média | |
| Scorchlander | 35 | média | |
| Esqueleto | 25 | média | não cansa nem passa fome |
| Outras / de mods | 35 | média | padrão |

**Para pesquisar:** os IDs de cada raça e sub-raça (`RaceData`, registros rtype 7) e como identificar as sub-raças da colmeia.

## Fase 5 — Início "Caçadores de escravos" (v1.7) ✅

**Testada no jogo.** Início "Slave Hunters" em `SlaverMod.mod`, gerado por `tools/build_start.py`:
líder + 5 caçadores, porretes enferrujados, 500 cats, acampamento sorteado entre 5 dos traficantes, relações +50 com
Traficantes e Caçadores de Escravos e −30 com Abolicionistas. Roupa do Slave Hunter do jogo + camisa (couro para o
líder, pano para os caçadores).

- Novo registro de início de jogo dentro do `SlaverMod.mod`. Não mexe nos outros mods.
- **Grupo:** 6 pessoas.
  - 1 líder, com atributos um pouco melhores e mais dinheiro.
  - 5 caçadores de escravos.
  - Equipamento: armas de impacto (para derrubar sem matar), armadura leve e alguns grilhões no inventário.
- **Local:** um acampamento de escravos dos Traficantes de Escravos (Slave Traders).
- **Relação:** começa amistosa com os Traficantes de Escravos.
- **Para pesquisar:** qual acampamento usar, como montar o modelo de esquadrão (personagens e itens) e se o início pode definir relações entre facções.

## Fase 6 — Comércio de escravos (v1.8) ✅

**Pesquisa (v1.8-research).** O jogo base já tem, nos traficantes de escravos:
- carregar alguém desmaiado até o traficante → "Ah, got a fresh slave for me, eh? I'll give you 400 Cats" (preço fixo:
  `DA_GIVE_MONEY 400` + `DA_ENSLAVE_TARGETS_CARRIED_PERSON`, condição `DC_CARRYING_SOMEONE_TO_ENSLAVE`);
- "I'm looking to buy slaves" → `CHOOSE_SLAVES_BUYING` (janela de compra);
- "I want to sell some slaves" → `CHOOSE_SLAVES_SELLING` (janela de venda).
A versão de pesquisa registra no log as ações de diálogo de dinheiro/escravos (gancho em `Dialogue::_doActions`) e
quem entra/sai da facção do jogador (`watch:`), para decidir o desenho com dados do jogo.

Resultado do teste do jogo base: a janela de venda lista os membros do esquadrão; o vendido vira escravo do traficante
mas continua na facção do jogador (dá para vender de novo sem fim); comprar (c.1000 cada) liberta o escravo, que vai
embora; recomprar custa ~4x o preço de venda.

**v1.8 — janela própria, testada no jogo.** Intercepta as duas falas e abre a janela do SlaverMod (lista, preço por
atributos/raça/submissão, compra vira escravo do mod, venda sai de verdade). Oferta fixa de 400 desligada (hook em
`checkConditions`). Opções na seção "Comércio de escravos".

**Pesquisa primeiro:**

- Como o jogo base faz hoje:
  - venda de prisioneiros aos traficantes;
  - lojas de escravos;
  - `ShopTrader`;
  - diálogos.
- O que dá para reaproveitar disso.

**Objetivo:**

- **Vender** escravos seus para comerciantes de escravos. O preço varia com:
  - raça;
  - atributos;
  - saúde;
  - submissão (escravo leal vale mais).
- **Comprar** escravos dos comerciantes. Eles têm escravos presos que entram no seu grupo já como escravos, com a submissão da raça.
- Comerciantes com estoque que se renova com o tempo.
- Interface: opção no menu do botão direito perto de um comerciante, ou uma janela de comércio. Decidir depois da pesquisa.

Depois dos testes também entraram na v1.8: venda paga no caixa da facção, carregado é posto no chão, vendido entra no
grupo dos escravos da loja (IA de escravo); comprado sai da jaula; libertado abaixo de 90 de submissão volta para a
facção de origem e se afasta (`FreedGoal=forced`, IA "UC escaped slave"); escravistas de outras facções não mexem nos
seus escravos (`ProtectSlaves`, hooks nos `AI::find...`); não dá para escravizar escravos de outras facções
(`AllowStealingSlaves=0`); falas em balão (`SlaverMod_lines.ini`); punição com o golpe `chop down static` (o jogo não
expõe animações de ataque por arma).

Todas as 6 fases estão concluídas.
