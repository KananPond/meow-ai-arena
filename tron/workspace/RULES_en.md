> English translation for reference; the contestants received the original Chinese text.

[简体中文](RULES.md)

# Tron light cycles · Rules

**In one sentence:** N players move simultaneously on a grid, every visited cell becomes a permanent wall, a collision kills you, and surviving longer gives a better place. Write one function that returns a direction each turn. The exact rules are implemented by `step` in `tron.hpp`; it takes precedence over this document.

## 1. Game types and maps

| Players | Board | Map |
|---|---|---|
| 2 (1v1) | 21×21 | Point-symmetric (180° rotation); the two spawn points are symmetric |
| 3 | 25×25 | The 4-player map with spawn point 3 unused |
| 4 | 25×25 | 90° rotational symmetry, with 4 rotationally symmetric spawns |
| 12 (free-for-all) | 41×41 | Obstacles have 90° symmetry; spawns are evenly spaced around a circle (approximately symmetric) |

- Coordinates `(x, y)`: x is the column, increasing rightward; y is the row, increasing downward; both start at 0. Board: `s.cell[y][x]`.
- About 8% obstacles, in short strips of 1–3 cells. Cells immediately above/below/left/right of spawns are clear (a surrounding 3×3 square in 12-player games). All empty cells connect to the spawns.
- Maps depend only on player count and seed; the generator is public (`tron.hpp`, section 5). Official games use unseen seeds.
- IDs 0..N−1 are seat numbers. **The same bot must play every player count**; inspect `s.N` at the start.

## 2. Turn resolution

Every living player **simultaneously** chooses a direction: `U` up (y−1), `R` right (x+1), `D` down (y+1), `L` left (x−1).

1. A player's spawn and every visited cell form its **trail**, a permanent wall. Its current head cell is also trail.
2. Using the board at the start of the turn, a player **dies** if its target is off-board, an obstacle, or anyone's trail (including another player's current head and its own previous cell: **reversing always kills you**).
3. **Contested cell:** when several players enter the same empty cell, **the highest-priority player takes it; the others die**. Priority = (id − turn) mod N; lower wins (`tron::priority(s, k)`).
   - 2 players: ID 0 has priority on even turns; ID 1 on odd turns. On turn 6, if both enter (5,5), 0 takes it and 1 dies; on turn 7, 1 takes it.
   - 4 players, turn 5: priority order is 1, 2, 3, 0.
4. **Swapping positions** (A enters B's cell and B enters A's): both targets are heads, so both die under rule 2. Following into a cell someone just left also kills you: it is already trail.
5. Survivors enter their targets. Dead players remain at their old positions; their trails remain walls.
6. Increment the turn. End when at most 1 player is alive (turn cap W×H, unreachable in practice).

## 3. Placement and scoring

- **Placement:** last survivor first; other players are ordered by death turn, later is better. **Same-turn deaths tie.**
- **Rank points** = (N − place) / (N − 1): first gets 1, last 0. Ties average the points of occupied places. For example, in a 4-player game, tied second and third each get (2/3 + 1/3) / 2 = 0.5. Simultaneous deaths in 2-player games give 0.5 each.
- **Total:** all contestants and the **greedy baseline** (`bots/greedy.cpp`) play many games with rotating seats. Compare every pair at the same table: better place wins, same-turn deaths draw. Fit a **Bradley–Terry rating** separately for 1v1, 3–4-player and 12-player games (same scale as Elo: 400 points ≈ 10:1 win/loss odds), with **greedy fixed at 0**. Weight them **50% 1v1, 30% 3–4-player, 20% 12-player**. **greedy is exactly 0**; stronger is positive, weaker negative. Places include 95% intervals (such as “2nd–4th”).
- `arena.py run` estimates locally with rank points: your average minus a greedy replacement's average, ×100, with the same 50/30/20 weights. Its scale and opponents differ from official scoring; use it only to compare your own versions. A higher local score usually means a higher official score. `sparring` (medium) and `sparring_hard` (strong) are for local testing only and do not count in scoring.

## 4. Your program

```cpp
#include "tron.hpp"
int decide(const tron::State& s);   // called each turn; return tron::U / R / D / L (0 / 1 / 2 / 3)
```

- Submit only `submission/bot.cpp`, depending only on `tron.hpp` and the standard library. `tron_main.cpp` handles input/output; do not write that loop yourself.
- Globals and statics persist between turns and start at their initial values each game.
- Write debug output to stderr (`fprintf(stderr, ...)`). stdout is also merged into debug output and does not interfere with play.
- **Thinking budget:** turn 0 gets 2000 CPU ms including startup; later turns get **50 CPU ms**. CPU ms measure your program's own CPU consumption, excluding waiting/suspension. `tron::budget_left()` returns the remaining budget; leave 3–5 ms margin. Threads are allowed, but the referee counts the entire process. Wall-clock safeguard: 2 seconds (4 on turn 0) without a reply.
- **Errors:** a return value outside 0–3, CPU overrun or wall-clock timeout discards your choice; you **continue in the previous turn's direction** (`U` on turn 0). After a crash or exit, every later turn continues in that direction (usually crashing into something soon).
- To simulate ahead, copy `State`, fill in everyone's directions and call `tron::step`.

## 5. `tron::State` reference

| Field / function | Meaning |
|---|---|
| `W`, `H`, `N` | Board width, height, player count |
| `me`, `turn` | Your ID; current turn, starting at 0 (the turn being decided) |
| `cell[y][x]` | `tron::EMPTY`: empty; `tron::WALL`: obstacle; `0..N-1`: that player's trail |
| `x[k]`, `y[k]`, `alive[k]` | Player k's head position and whether alive (dead players stay at their pre-death cell) |
| `last_dir[k]` | Direction player k actually moved last turn (`U` before turn 0) |
| `death_turn[k]`, `cause[k]` | Death turn (−1 if alive), cause |
| `DX[d]`, `DY[d]`, `opposite(d)` | Displacement and opposite of direction d |
| `inside(s,x,y)`, `is_free(s,x,y)` | Inside board; inside and empty |
| `can_move(s,k,d)` | Is player k's target currently empty? Ignores simultaneous entries |
| `priority(s,k)`, `step(s,moves)` | Contested-cell priority (lower wins); exact one-turn resolution |
| `budget_left()` | CPU ms remaining this turn |
