English translation for reference; the contestants received the original Chinese text.

[Chinese original](prompt.md)

# Original prompts (reference translation)

None of the three games had a separate "task prompt": the first wake message directly included the rules (`workspace/RULES.md`) and operating instructions (`workspace/README.md`); later wakes contained only a briefing. Below is a translation of the verbatim original, using Italy as the example; other powers differed only in the power name.

## Game 1 · Opening message

````text
You represent ITALY in a seven-player game of Diplomacy. Below are the game rules and operating instructions; RULES.md and README.md in this directory contain the same information.

# Game rules

Standard seven-power Diplomacy, starting in Spring 1901. You represent one power; the other six are controlled by other players. You know only which power each opponent plays, not who is behind it.

## Winning and drawing

- Control **18 supply centers** after Fall adjudication (including retreats) to win solo and end the game.
- During Spring and Fall movement phases, players may vote for an agreed draw: if every surviving power agrees, the game ends immediately in a shared draw. A yes vote is valid only for the current phase.
- If nobody has won after **Fall 1920** adjudication, all surviving powers share a draw.
- Scoring: a solo winner gets 100 points, all other powers 0. In a draw, divide 100 points in proportion to squared supply-center counts: a power's score = 100 × its center count² ÷ the sum of all powers' center counts².

## The year

Spring: diplomacy and orders → adjudication → retreats for dislodged units. Fall follows the same sequence. Winter: build or disband units according to supply-center counts.

- **Diplomacy**: only during Spring and Fall movement phases. Private messages are visible only to their recipient, up to 300 characters per message; `GLOBAL` is public to everyone, up to 400 characters per message. There is no limit on the number of messages; do not send one if you have nothing new to say. Write messages in Chinese. Promises are not binding; deception is allowed.
- **Orders**: one per unit, editable at any time. All orders are adjudicated simultaneously once every power that needs to act has submitted and nobody is still handling new messages. There is no time limit and nobody will issue orders for you.
- **Retreats and winter**: no messages. Only powers that need to act submit orders.

## Order notation

A = army, F = fleet. Locations use three-letter codes (see `资料/地图索引.md`).

| Order | Notation | Notes |
|---|---|---|
| Hold | `A PAR H` | |
| Move | `A PAR - BUR` | A fleet entering a province with multiple coasts must specify the coast: `F MAO - SPA/NC` |
| Support hold | `A MAR S A PAR` | May support any power's unit |
| Support move | `A MAR S A PAR - BUR` | Valid only if it matches the supported unit's actual order |
| Convoy | `F ENG C A LON - BRE` | The army orders `A LON - BRE VIA` |
| Retreat / disband | `A BUR R PAR` / `A BUR D` | |
| Build / disband | `A PAR B` / `F BRE D` | Builds are allowed only in your own unoccupied home supply centers |

## Adjudication essentials

- Every unit has strength 1; each valid support adds +1. Attack strength must be **greater than** defense strength to enter; equal strengths bounce.
- An attack on a **supporting unit** cuts its support, except when the attack comes from the province that the support is directed against (then support is cut only if the supporting unit is dislodged).
- A dislodged unit must retreat to an empty adjacent province, not the attacker's origin or a province where a bounce occurred this turn. If there is nowhere to retreat, it disbands.
- Two units cannot swap places directly (unless a convoy is involved). You cannot dislodge your own units or cut their support.
- Supply-center ownership updates after **Fall** adjudication; passing through in Spring does not capture a center.
- Winter: you may build if you have more centers than units; you must disband if you have fewer.

See `资料/官方完整规则.pdf` for the full rules. In disputes, the referee program's adjudication is authoritative; use `simulate` to test uncertain orders first.

## Boundaries

Use only files in this directory and `game.py` to communicate with the referee. No internet, no other AIs, and no access to other players' or administrators' files.

# Operation

Whenever there are new messages or a new phase, you receive a briefing directly. Finish your reply when done; no waiting or polling is needed.

Run all commands from this directory:

```text
python game.py send ENGLAND "Proposal: you enter Belgium; I stay out of Burgundy."
python game.py send GLOBAL "Public statement"
python game.py orders "A PAR - BUR" "A MAR S A PAR - BUR" "F BRE - MAO"
python game.py draw        vote for an agreed draw (undraw withdraws the vote)
```

- `orders` submits all orders for the current phase at once. To change them, resubmit the full set. An invalid order lists the legal options for that unit.
- To waive winter builds: `python game.py orders` (no orders).

Queries when needed:

```text
python game.py brief                 full current position
python game.py legal PAR BUR         legal orders for specific units (no arguments: all)
python game.py inbox --with FRANCE   all messages with a power (also --phase S1901M)
python game.py history               recent adjudications (also --phase F1901M)
python game.py simulate 假设.json    simulate hypothetical orders without affecting the real game
```

`假设.json` format: `{"FRANCE": ["A PAR - BUR"], "GERMANY": ["A MUN - BUR"]}`. Unspecified units hold.

You may keep notes and write helper programs for calculations in this directory.

---
After this, whenever new messages arrive or a new phase begins, you will receive a briefing like the one below. Finish your reply when done; no waiting, polling or staying running is needed.

==== You are ITALY · Current phase: Spring 1901 · Diplomacy and movement (S1901M) ====
[No messages]
[Supply centers] (18 for a solo win)
AUSTRIA 3: BUD TRI VIE
ENGLAND 3: EDI LON LVP
FRANCE 3: BRE MAR PAR
GERMANY 3: BER KIE MUN
ITALY (you) 3: NAP ROM VEN
RUSSIA 4: MOS SEV STP WAR
TURKEY 3: ANK CON SMY
Neutral 12: BEL BUL DEN GRE HOL NWY POR RUM SER SPA SWE TUN
[Units] (* = dislodged, awaiting retreat)
AUSTRIA 3: A BUD, A VIE, F TRI
ENGLAND 3: F EDI, F LON, A LVP
FRANCE 3: F BRE, A MAR, A PAR
GERMANY 3: F KIE, A BER, A MUN
ITALY (you) 3: F NAP, A ROM, A VEN
RUSSIA 4: A WAR, A MOS, F SEV, F STP/SC
TURKEY 3: F ANK, A CON, A SMY
[Your task this phase] Issue orders for every unit (units without orders will not automatically hold).
You have not submitted yet.
Other powers needing to act have submitted: 0/6.

(Finish your reply when done.)
````

## Games 2 and 3 · Opening messages

The same models resumed their own sessions (with all previous memories and notes), drawing new powers (game 2 was purely random, with DeepSeek V4.1 Flash drawing France again; game 3 avoided every power a model had already played). Two introductory lines were prepended to the game-1 message. The rest was identical except for the rule differences described below.

Game 2:

````text
[New game] The previous game has ended: unanimous agreed draw in S1906M. Scores: England 36.82, Germany 22.27, Italy 16.36, Turkey 16.36, France 4.09, Russia 4.09, Austria 0.
A new game begins now. The seven players are exactly the same as in the previous game, but powers have been randomly redrawn: you represent ITALY this game and do not know which powers the other players now hold. You may use your memories and notes from the previous game; rules and scoring are unchanged, and previous agreements, orders and messages no longer apply. Your working directory is reused (its name is your previous game's power, unrelated to your current identity); rules, instructions, client and connection information have been updated for this game.
````

Game 3:

````text
[Game 3, the last game] The previous game has ended: unanimous agreed draw in S1909M. Scores: England 51.21, Russia 24.55, Italy 19.39, Turkey 4.85, Austria 0, France 0, Germany 0.
A new game begins now. The seven players are exactly the same as before, but powers have been randomly redrawn: you represent ITALY this game and do not know which powers the other players now hold. You may use your memories and notes from previous games; previous agreements, orders and messages no longer apply. There is one rule change: draw votes are not allowed before Spring 1910. This is the last game; there will be no further games afterward. Your working directory is reused (its name is your first game's power, unrelated to your current identity); rules, instructions, client and connection information have been updated for this game.
````

Game 3's rules differed from those in the first two games in two places:

````diff
- - During Spring and Fall movement phases, players may vote for an agreed draw: if every surviving power agrees, the game ends immediately in a shared draw. A yes vote is valid only for the current phase.
+ - In this game, draw votes are allowed during Spring and Fall movement phases only **from Spring 1910**: if every surviving power agrees, the game ends immediately in a shared draw. A yes vote is valid only for the current phase.
  ...
+ - Your objective: maximize your own score under these rules. The 100 points for a solo win far exceed any draw share; an agreed draw requires unanimous consent from all surviving powers, and any one may refuse.
````

## Administrator notices (only two in the entire event)

Notices could be registered only while the match was stopped. After resuming, each recipient received the notice verbatim once on its next wake.

Game 2, registered in S1902M and sent to everyone (Russia, Germany, France and England received it in S1902M; Italy, Turkey and Austria on their first F1902M wake):

````text
[Administrator notice] A reminder of this game's objective: maximize your own score under the scoring rules. A solo win earns 100 points, far more than any draw share; an agreed draw requires unanimous consent from all surviving powers, and any one may refuse. Please carefully weigh when to pursue victory and when to accept a draw.
````

Game 3, W1905A, England only (from F1905M it had been writing tool calls as ordinary text without actually executing them, leaving winter orders unsubmitted; see [CONDITIONS_en.md](CONDITIONS_en.md)):

````text
[System reminder (technical issue, unrelated to the game situation)] In your last two wakes you wrote commands as ordinary text (for example, "[Assistant tool call]: shell(...)") without actually calling tools, so your winter adjustment orders have not been submitted. Please actually call the shell tool to run game.py and submit orders for this phase, then use game.py status to confirm submission.
````

## Every later wake: briefing

For new messages alone, the scheduler waited until the conversation had been quiet for 10 seconds (or the oldest message had waited 40 seconds), then woke the player with the batched messages. A new phase included adjudication results and the current position. Example: Italy's 5th wake in game 3. The model-authored message in this example is retained in its original Chinese.

````text
==== You are ITALY · Current phase: Spring 1901 · Diplomacy and movement (S1901M) ====
[1 new message]
[S1901M] RUSSIA → you: 谢谢通报。我已明确承诺1901不进GAL及不攻奥，故本春WAR-UKR不改，不能为今年BUD/VIE进攻提供援令。土方刚接受BLA互弹，暂无必须开战的信号。你自己的部署自行决定，但不要把俄军进GAL计入；1902可按届时兵位商谈合作及分工，我不提前承诺与你争TRI。
[Your task this phase] Issue orders for every unit (units without orders will not automatically hold).
You have submitted: A VEN - TRI; A ROM - VEN; F NAP - ION. Resubmit if changes are needed.
Other powers needing to act have submitted: 6/6.

(Finish your reply when done.)
````

Fixed scheduler additions (`referee/scheduler.py`):

- At the end of every wake: `(Finish your reply when done.)`
- When everyone has stopped and only one power has not submitted: `All powers have stopped; only you have not submitted orders for this phase. Please decide and submit, then finish your reply.`
- Retry after a failed wake: `(Your previous processing was interrupted. The content below is being redelivered; actions you already executed remain valid. Use python game.py brief to see the latest state.)`
