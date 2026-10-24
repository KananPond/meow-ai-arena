[中文原文](RULES.md)

English translation for reference; the contestants received the original Chinese text.

# Territory battle: rules and communication protocol

`engine/game.py` is authoritative. This document describes the same behaviour as the code; defer to the code if uncertain.

## 1. Two formats

**The same bot must play both formats.** Distinguish them by the initial player count `N`:

| Format | Players N | Board | Turns | Spawns |
|---|---|---|---|---|
| **1v1** | 2 | 40 × 40 | 600 | Player 1 = player 0 rotated 180° around the board centre |
| **4-player** | 4 | 48 × 48 | 600 | Player k = player 0 rotated clockwise by k × 90° around the board centre |

- Coordinates `(x, y)`: x is the column, increasing right; y is the row, increasing down. Both start at 0.
- The map seed determines player 0's spawn (at least 4 cells from the edge in 1v1, 5 in 4-player). Any two spawns have Manhattan distance at least 16. Initial directions follow the same rotations.
- Initial territory: a **5 × 5** square centred on the spawn.
- Protected area: a **3 × 3** square centred on the spawn. It always belongs to that player and cannot be captured in any way.
- Rules are identical except for player count and board size. **Do not assume only one opponent:** in 4-player games, any of 3 opponents can cut you off.

## 2. Turn sequence

Everyone acts **simultaneously**. Resolve each turn in this order:

1. **Choose direction:** each player sends `U` (up, y−1), `D` (down, y+1), `L` (left, x−1) or `R` (right, x+1).
   - Reversing the current direction is invalid; continue in the current direction.
   - Timeouts or malformed output likewise continue in the current direction.
2. **Wall collisions:** a target outside the board kills the player.
3. **Head collisions:** if two players target the same cell or swap positions (A enters B's current cell while B enters A's), **both die**. Three or more targeting one cell all die.
4. **Cut trails:** if a player's target contains a trail, **the trail's owner dies**. Entering your own trail kills you.
   - Use trails from before this turn's movement.
   - A head outside its territory is part of its trail. Entering an opponent's current head cell while it is outside its territory also cuts it off.
   - The cutter also dies if step 2 or 3 applies to it.
5. **Handle deaths:** clear all trails of dead players, immediately return them to spawn, and restore their initial direction. **Territory remains.** Dead players do not move again this turn.
6. **Move survivors** to their targets.
   - If the target is not their own territory (neutral or enemy land), add it to their trail.
   - If the target is their own territory and they have a trail, capture this turn (below).
7. **Capture:** for each player P capturing this turn:
   - All of P's trail cells become P's territory.
   - **Enclosed area:** treat P's territory and trail as walls, then flood-fill from the board boundary using four-way adjacency. All unreachable non-wall cells become P's territory, including neutral and enemy land.
   - Exception: enemy protected areas never change ownership.
   - For simultaneous captures, calculate every result from the board before capture and apply them simultaneously. A cell enclosed by two or more players retains its original owner.
   - Clear P's trail after capture.
   - Capturing an enemy trail cell changes its ownership to P, but the enemy trail remains and its owner does not die from this.

## 3. Results

- After 600 turns, rank by **territory cell count**, more first; equal counts tie.
  - 1v1: larger territory wins; equal territory draws.
  - 4-player: rank 1–4.
- Only placement matters; **the winning margin has no effect**.
- Forfeits:
  - Crash, process exit, or inability to write to the program.
  - **10** accumulated timeouts or format errors within one game.
  - 1v1: end immediately; the opponent wins (simultaneous forfeits draw).
  - 4-player: the player is immediately eliminated and ranks below all non-forfeiting players; multiple forfeits tie. Clear its trail. It remains at spawn, no longer moves and cannot die again. Its land stays and can be captured except for the protected area. Other players continue.

## 4. Communication protocol

The bot is a separate process reading **stdin** and writing **stdout**. Messages are whitespace-separated text lines ending in `\n`.

### 4.1 Initialization (sent once)

```text
INIT
W H MAX_TURNS N MY_ID MOVE_MS INIT_MS
SPAWN 0 x0 y0
SPAWN 1 x1 y1
... (N SPAWN lines total)
```

- 1v1: `W=40 H=40 MAX_TURNS=600 N=2`; 4-player: `W=48 H=48 MAX_TURNS=600 N=4`. Both use `MOVE_MS=50 INIT_MS=2000`.
- `MY_ID` is your player ID, from 0 to N−1.

### 4.2 Each turn (t = 0, 1, …, MAX_TURNS−1)

```text
TURN t
P 0 x y dir trail_len area deaths
... (N P lines total, players 0 to N−1)
OWNER
<H rows of W characters>
TRAIL
<H rows of W characters>
END
```

- `P`: player ID, head coordinates, current direction (`U/D/L/R`), trail length, territory cell count, cumulative deaths.
- `OWNER`: character x of row y gives the owner of `(x, y)`; `.` is neutral, `0`–`3` are players.
- `TRAIL`: `.` means no trail; `0`–`3` identify the trail owner.
- Turn t's state is **before** move t.
- Eliminated players in 4-player games still have a `P` line: their spawn coordinates and trail length 0.

### 4.3 Response

For every turn received, output one line:

```text
t DIR
```

`t` is the turn number; `DIR` is `U`/`D`/`L`/`R`. **Flush after every response** (`std::endl` or `std::cout.flush()` in C++).

- Limit: **2000 ms** on turn 0 including startup, then **50 ms** per turn, timed from sending the state. The referee has a small tolerance; do not rely on it.
- Timeout: continue in the current direction and count 1 error. Late replies with an earlier turn number are silently discarded.
- Malformed line: count 1 error and continue in the current direction.
- Write all debugging to **stderr**. stdout is for responses only.
- If your program is consistently too slow, its reading falls behind the referee and every subsequent turn times out.

## 5. Opponents and maps

- Maps are determined solely by their seed (spawns and initial directions); there are no obstacles.
- 1v1: each pair plays the same map set, once in each spawn position per map.
- 4-player: each round randomly groups all bots into tables of 4. Each table plays 4 seat rotations on one map, so every bot occupies every seat once.
- Opponents include bots written by other AI models and the two baselines, random and greedy, under `anchors/`.
