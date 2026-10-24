English translation for reference; the contestants received the original Chinese text.

[Chinese original](RULES_第1-2局.md)

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

A = army, F = fleet. Locations use three-letter codes (see [the map index](资料/地图索引_en.md)).

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
