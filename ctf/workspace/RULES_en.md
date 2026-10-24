[中文原文](RULES.md)

English translation for reference; the contestants received the original Chinese text.

# Multi-side Capture the Flag · Rules v1.0

2–15 sides share one map; each side's program controls 3 units. Bring neutral flags back to your base for 1 point each. After 400 turns, rank by score; equal scores tie.

## Map

- Square grid: side length 23 for 2 sides, 31 for 3–5, 41 for 6–10, 49 for 11–15. The boundary is wall, with scattered single-cell pillars also treated as walls. The full map is public.
- **Bases:** each side has a 3×3 area. Bases lie on a ring at equal distance from the centre, with roughly equal spacing between neighbours. Any unit may enter any base.
- **Flag spots:** one in front of each base and one at the exact map centre, for “number of sides + 1” spots. All bases have equal distance to their own flag spot and to the centre.
- **Flags:** max(2, ⌈number of sides / 2⌉), belonging to no side. At initialization and every return, each flag randomly appears on a flag spot containing no flag. This randomness is independent of map generation and cannot be predicted.

## Each turn

Each unit chooses one **move** (up/down/left/right/stay) and one **action** (attack/pick up/drop/do nothing). All programs submit simultaneously; resolve in this order:

1. Return flags and respawn dead units whose timers expire
2. Movement
3. Attacks and deaths
4. Pickups
5. Drops
6. Flag deliveries

### Movement

Move one cell up/down/left/right or stay. Moving into a wall means staying. No two units occupy the same cell after resolution:

- Several units target the same cell: all stay.
- Entering a cell another unit is leaving is allowed; if that unit fails to leave, you stay too.
- Two-unit swaps: allowed within one side; both stay if on different sides.
- Movement cycles of three or more units are allowed.

### Attacks

- Specify the target enemy unit's id; range is checked using both units' positions after movement.
- Range: sum of horizontal and vertical distance at most 2. Walls block only in two cases: a straight attack across one intervening cell requires that cell not be a wall; a diagonal attack (difference 1 on each axis) requires at least one of the two corner cells not be a wall. Adjacent attacks are unaffected by walls.
- Each hit deals 34 damage. Units have 100 health and no natural regeneration; 3 hits always kill.
- All attacks take effect simultaneously; units may kill each other on the same turn.
- Flag carriers cannot attack. A unit can neither attack nor be attacked on the turn it respawns.

### Flags

- Pickup: take a flag on your cell. At most one flag per unit; if several share the cell, take the lowest-numbered one.
- If a carrier dies, its flag drops on that cell and can be collected by any side.
- Drop: voluntarily put down your flag. Pickups resolve before drops, so a teammate can pick it up no earlier than the next turn.
- A flag left on the ground for 15 turns returns to a random empty flag spot.
- A carrier standing in its own base at turn end delivers the flag for 1 point. The flag leaves play for 8 turns, then reappears at a random empty flag spot.

### Death and respawn

10 turns after death, respawn at your base with full health, preferring an empty cell nearer the map centre. If the base is full (including enemy occupants), postpone until a cell is free. You may issue commands to a unit due to respawn; it can move and pick up on its respawn turn.

## Program interface

- **Language:** C++20, standard library only. Submit one file, `submission/bot.cpp`, compiled with `g++ -std=c++20 -O2` (plus `-static` on Windows).
- **Communication:** read a text block from stdin every turn and write one response line to stdout, immediately flushing (for example with `endl`). The program persists throughout the game and may store state in memory. Write debugging to stderr.
- **Time:** 2 seconds first turn, including startup, then 50 ms each turn, timed from sending input to receiving the complete response. Timeout or malformed output makes all units stay for that turn; late responses are discarded. After 3 consecutive failures, or process exit, all units stay for the rest of the game.
- Communicate with the referee only through stdin/stdout. No file access, network access or launching other programs.

The `submission/bot.cpp` starter has full input parsing you can reuse.

### Input

The first turn begins with initialization; each turn then supplies a block ending in `END`. Numbers are space-separated; each item below occupies one line:

```text
INIT side_count my_side map_side total_turns health damage respawn_delay uncollected_flag_return_delay delivered_flag_cooldown
MAP
(map_side rows; # wall, . empty)
BASES side_count
x y                    (one side's base centre per line; base is the centred 3×3)
SPOTS spot_count
x y                    (one flag spot per line)
```

```text
TURN t                 (turn about to resolve, starting at 1)
SCORE scores_for_each_side
UNITS unit_count
U id side x y health carried_flag respawn_turn
FLAGS flag_count
F id state x y carrier_unit return_turn
EVENTS event_count
E type ...             (events from the previous turn, below)
END
```

- Top-left is (0, 0), x increases right and y down. Side k has unit ids 3k, 3k+1 and 3k+2.
- **Units:** dead units have x,y = -1; no carried flag = -1; living units have respawn_turn = -1.
- **Flags:** state is `home` (at a flag spot), `dropped` (on the ground), `carried`, or `cooldown` (out of play after delivery). The latter two have x,y = -1. No carrier or no return turn is represented by -1 in the corresponding field.
- If a respawn or return turn equals t, it happens at the start of this turn.

| Event | Format |
|---|---|
| Flag return | `E return flag` |
| Respawn | `E respawn unit x y` |
| Hit | `E attack attacker target` |
| Death | `E death unit x y dropped_flag killer_count killers...` (no dropped flag = -1) |
| Pickup / drop | `E pickup unit flag`, `E drop unit flag` |
| Delivery | `E capture unit side flag` |

### Output

One line: turn number, then one move and action for each of your 3 units in increasing id order:

```text
t move action move action move action [# comment;comment;comment]
```

- **Move:** `U` up, `D` down, `L` left, `R` right, `S` stay.
- **Action:** `-` do nothing, `P` pick up, `X` drop, or a number to attack the unit with that id.
- **Comments** (optional): after `#`, semicolon-separated for the 3 units in order, at most 20 characters each; shown only in replays. Example: `# grab;escort;intercept`.
- Example: `37 R 4 U P S -` means turn 37: first unit moves right and attacks unit 4, second moves up and picks up, third stays.
- Include dead units too; their commands are ignored unless they respawn this turn. Wrong turn number, wrong field count or undefined letters are format errors.

## Official tournament

- Opponents are programs written by other models plus one medium-strength baseline.
- Covers side counts from 2 to the full field, each weighted equally. Map seeds are not disclosed; the same group rotates through every seat on the same map.
- Rank by game score, split placements into pairwise outcomes, fit Bradley–Terry strength, baseline = 0.
