> English translation for reference; the contestants received the original Chinese text.

[简体中文](RULES.md)

# Tank Battle · Rules

The exact implementation is `tank.hpp`, also used by the referee. **If this document and `tank.hpp` differ, the header takes precedence.** This document covers game rules and penalties; see `README.md` for tools, interfaces, timing and submission.

## 1. Overview

1v1. Each side has a base and three tanks: one fast, one fire, one armor. Each turn both sides simultaneously command their three tanks, then resolve as in section 5. **Reduce the enemy base to 0 HP to win.** If 300 turns finish without a winner, compare as in section 9.

No randomness or hidden information: identical states and commands always resolve identically. The rules themselves are symmetric between seats.

| Term | Meaning |
|---|---|
| Seat | Side 0 at the bottom (higher y), side 1 at the top. Tank IDs 0/1/2 = side 0 fast/fire/armor; 3/4/5 = side 1 fast/fire/armor |
| Cell | Position (x, y), x rightward, y downward, top-left (0,0). Up/down/left/right = `U`(y−1), `D`(y+1), `L`(x−1), `R`(x+1) |
| Turn | One `step`. `s.turn` counts completed turns, initially 0; first resolution is turn 1. End of turn T means section 5C of turn T |
| Substep | Two per turn. Bullets travel 1 cell per substep, 2 per turn |
| On board | A tank is present (`alive`); absent tanks occupy no cell and cannot act |
| Hit | Bullet hits tank/base and disappears; tank loses 1 HP (base: section 6) |
| Destroyed | Tank with HP ≤ 0 removed at end of turn |
| Cancellation | Bullets meet and disappear |

## Quick reference: timing and common pitfalls

Each point refers to the detailed sections/examples.

**Turn order** (section 5):

1. **Fire:** at turn start, bullets appear in the firing tanks' own cells. Reloading or at bullet cap means stay instead.
2. **Substeps 1 and 2:** tanks move, all bullets advance 1 cell, then resolve hits, walls and cancellation.
3. **End of turn:** destruction → respawn → pickup → supply → victory check.

**Common pitfalls:**

- Tank and bullet in the same cell at substep end means a hit, whoever arrived first. A tank at 0 HP remains until turn end, acts normally and blocks bullets (5, 11.5, 11.7).
- A base loses at most 1 HP per turn, counting friendly and enemy bullets together; destroying it takes at least 24 turns (6).
- Friendly fire exists: killing a teammate awards the opponent a kill; hitting your base damages it (5, 6, 11.11).
- Respawning destroys an enemy on the spawn regardless of HP (7).
- Stars appear at the end of turns 10, 30, 50…; a tank already there can collect one only at the next turn end (8, 11.12).
- Level-0 fast/armor tanks cannot fire on the turn after firing; fire tanks need no reload. All start with a 1-bullet cap, counted at turn start. At level 1 fast/armor no longer reload; fire tanks have a cap of 2 (3, 11.10).
- `legal_commands` may still fail because of blocking tanks or contested cells (4, 11.6). `s.grid` contains terrain only: when tracing a firing line, check tanks separately with `tank_at`; bullets hit them first (5).

## 2. Map

### 2.1 Terrain

15×15, x and y 0–14. Current terrain: `s.grid[cell_of(x, y)]`.

| Terrain | `Cell` | Character | Tanks | Bullet entering |
|---|---|---|---|---|
| Empty | `EMPTY` | `.` | May enter | Hits a tank there, otherwise continues |
| Brick | `BRICK` | `#` | Cannot enter | Bullet disappears; brick becomes empty (any bullet) |
| Steel | `STEEL` | `@` | Cannot enter | Bullet disappears; a level-2 tank's bullet also clears the steel |
| Water | `WATER` | `~` | Cannot enter | Passes through |
| Base | `BASE0` `BASE1` | `0` `1` | Neither side may enter | Bullet disappears; base damage (6) |
| Off-board | `OUTSIDE` | | Cannot enter | Bullet disappears |

Destroyed walls never return. Spawns and supply points are just markings on empty cells; they block neither tanks nor bullets.

### 2.2 Fixed layout

| | Side 0 (bottom) | Side 1 (top) | State field |
|---|---|---|---|
| Base | (7,14) | (7,0) | `s.base_pos[seat]` |
| Enclosure (5 ordinary brick cells) | (6,14) (8,14) (6,13) (7,13) (8,13) | (6,0) (8,0) (6,1) (7,1) (8,1) | |
| Three spawns | (5,12) (7,12) (9,12) | (5,2) (7,2) (9,2) | `s.spawn[tank_id]` |
| Three shared supply points (midline y=7) | (2,7) (7,7) (12,7) | | `s.supply[0..2]` |

The seed determines which type uses which spawn. Initially all six tanks are at their spawns at level 0; no bullets or stars.

Example generated map (seed 1, brick-heavy). Side 1 fast/fire/armor spawn at (7,2) (9,2) (5,2); side 0 at (7,12) (5,12) (9,12):

```text
    x=012345678901234      (units digit of x)
y= 0  ...####1#.#.##.
y= 1  .#....###.##...
y= 2  .#..........@@#
y= 3  #..........#..#
y= 4  #....#....###.#
y= 5  .~~#.#...###..#
y= 6  ...#.##.@....##
y= 7  ##.#...........      midline
y= 8  ...#.##.@....##
y= 9  .~~#.#...###..#
y=10  #....#....###.#
y=11  #..........#..#
y=12  .#..........@@#
y=13  .#....###.##...
y=14  ...####0#.#.##.
```

### 2.3 Generator guarantees

`generate_map(seed, style, params)` generates maps; `check_layout` checks these guarantees:

1. **Vertical terrain reflection:** (x, y) and (x, 14−y) have the same terrain (bases reflect each other). No left/right symmetry guarantee.
2. **Tank-type placement flips left/right:** if a side-1 type spawns at (x, 2), the same side-0 type spawns at (14−x, 12). Above, side-1 fire is (9,2) and side-0 fire (5,12); opposite (9,2) at (9,12) is side-0 armor. Only the x=7 pair is the same type. As terrain is not left/right symmetric, the same type faces different terrain in the two seats.
3. **Connectivity:** treating bricks as empty (steel/water impassable), both bases, six spawns and three supply points connect. Actual passage may require shooting bricks.
4. **Each base can be shot from at least two of left, right and front.** A direction qualifies when its outward line contains a tank-accessible cell outside the enclosure (empty or brick), with no intervening steel (brick/water do not disqualify it), reachable by an attacker without crossing that base's enclosure (brick treated as empty; steel/water impassable). `base_open_sides(layout, seat)` gives the count.
5. In the outer two rows at each end (y=0,1 and y=13,14), x=3–11 contains no steel/water: only bases, empty or brick.
6. Every spawn and its four orthogonal neighbors start empty, except the cell toward the base from the central spawn, which is enclosure brick. Supply points are empty.

### 2.4 Four styles

| Style | `Style` | Average brick cells (including 10 enclosure cells) | Steel | Water |
|---|---|---:|---:|---:|
| Open | `STYLE_OPEN` (0) | 29 | 4 | 4 |
| Brick-heavy | `STYLE_BRICK` (1) | 74 | 6 | 4 |
| Water-heavy | `STYLE_WATER` (2) | 29 | 4 | 32 |
| Steel-heavy | `STYLE_STEEL` (3) | 34 | 26 | 4 |

Style −1 (default of `make_game(seed)`) cycles by seed: seed modulo 4 is the style index.

**Official maps use the same generator and parameters, with different seeds.**

## 3. Tanks

| Type | ID (side 0 / side 1) | Initial HP | Movement per turn | Bullet cap | Reload |
|---|---|---:|---|---:|---|
| Fast `FAST` | 0 / 3 | 1 | Up to 2 steps | 1 | 1 turn after firing |
| Fire `FIRE` | 1 / 4 | 1 | 1 step | 1 | None |
| Armor `ARMOR` | 2 / 5 | 2 | 1 step | 1 | 1 turn after firing |

- **Bullet cap:** bullets fired by this tank still on board (`bullets_out`) cannot exceed `bullet_cap`.
- **1-turn reload:** firing on turn T prevents firing on T+1; may fire from T+2 (`reload_left` is turns still to wait).
- `can_fire(s, i)`: would a fire command for this tank in the next turn actually emit a bullet?
- Tanks have no facing; move or fire in any direction at any time.

### Upgrades

One star gives one level (8), maximum 2.

| | Level 1 | Level 2 |
|---|---|---|
| Fast, armor | Current HP +2; no reload (bullet cap remains 1) | Current HP +2; subsequent bullets destroy steel |
| Fire | Current HP +2; bullet cap becomes 2 | Same as above |

- Level-1 effects remain at level 2. Upgrades add at most +4 HP. This adds to current HP, including for damaged tanks.
- Steel-breaking ability is set by level **when fired** (bullet `steel` flag); later upgrades do not change existing bullets.
- Reload and cap use the **current** level.
- Destruction resets level and loses all upgrades.

## 4. Commands

One command per tank per turn (`Orders.cmd[FAST]`, `cmd[FIRE]`, `cmd[ARMOR]`).

| Command | Text | Code | Constructor |
|---|---|---|---|
| Stay | `S` | 0 | `STAY` |
| Move one step | `U` `D` `L` `R` | 1–4 | `cmd_move(d)` |
| Fire | `FU` `FD` `FL` `FR` | 5–8 | `cmd_fire(d)` |
| Two steps (fast only) | Two direction letters, e.g. `UL` = up then left | 9–24 | `cmd_move2(d1, d2)` |

Direction `d`: `UP`=0, `DOWN`=1, `LEFT`=2, `RIGHT`=3. Fast may move only one step; its two steps may differ in direction. Firing means no movement.

`legal_commands(s, i, out)` lists meaningful commands, removing those guaranteed equivalent to stay or another command. Listed moves may still fail due to blocking tanks or contested cells.

## 5. Turn resolution

A turn = **A firing → B substep 1 → B substep 2 → C turn end**. This is the key section: these steps determine every outcome.

### A. Fire (turn start)

1. Every on-board tank with a fire command succeeds if both hold:
   - Its on-board bullet count is below its cap, counted **now**: a bullet disappearing later this turn frees its slot only next turn.
   - It is not reloading.

   Success creates a bullet **in the tank's own cell**, recording direction, owning tank and steel-breaking ability. It advances normally in both substeps. Failure treats the command as stay.

### B. Substeps (twice)

2. **Tanks move.** Substep 1: first step of every move command. Substep 2: only fast tanks commanded to move twice take their second step. Movers act simultaneously. A step succeeds only if all three hold:
   - Target terrain is currently empty;
   - No tank occupied it at the **start of this substep**;
   - No other tank is trying to enter it in this substep.

   Otherwise the tank stays. A fast tank's second step is evaluated independently from its then-current position, even if its first step failed.
3. **All bullets advance 1 cell**, including newly fired bullets.
4. **Resolve in this order.** Removed bullets take no further part.
   1. **Bullet swaps:** two bullets swapping positions this substep both disappear.
   2. **Bullet/tank swaps:** a bullet and tank swapping positions counts as hitting that tank.
   3. **Check each remaining bullet's cell:** off-board → disappears; brick → bullet and brick disappear; steel → bullet disappears and steel disappears if the bullet can break it; water → no effect; base → bullet disappears and damages base (6); empty with tank → hit, once per bullet, regardless of side. Walls disappear only after all bullets in this step are processed, so every bullet hitting the same wall this substep disappears.
   4. **Same-cell cancellation:** among surviving bullets, any cell containing two or more loses them all. Direction and side do not matter, including over water.

   Tanks reduced to HP ≤ 0 **remain** until turn end.

### C. Turn end (in order)

5. **Destruction:** remove tanks at HP ≤ 0, reset level, add 1 to the opponent's `kills` regardless of who shot them.
6. **Respawn** (7).
7. **Pickup** (8).
8. **Supply** (8).
9. **Victory check** (9).

### Direct consequences

- Tanks move before bullets; hits resolve after both. Sharing a cell at substep end is a hit whoever arrived first.
- A firing tank cannot be hit by its own newly fired bullet.
- Bullets have friendly fire: teammates and own bases can be hit; friendly bullets cancel too.
- A tank destroyed this turn still executes its full command, including firing and movement.

## 6. Bases

- 24 HP (`s.base_hp[seat]`), no regeneration.
- **Each base loses at most 1 HP per turn.** Combine both substeps and friendly/enemy bullets: only one damages it. Other bullets still disappear. Caps are independent per base. Destruction takes at least 24 turns.
- **Hitting your own base damages it** and consumes its 1-HP allowance for the turn.
- Enclosure bricks are ordinary bricks: destroy them or approach an unblocked side first.
- `s.base_dmg[seat]` records damage inflicted on the enemy base; statistical only. Own-base damage is uncredited. If both sides hit and only 1 HP is lost, the earlier substep gets credit; within the same substep credit goes to the enemy.

## 7. Respawning

| | Rule |
|---|---|
| Time | Destroyed at turn T end → absent on T+1, T+2, T+3 (commands ignored) → **respawn at T+3 end** → acts from T+4. `respawn_in(s, i)` counts turn ends left |
| Position/state | Own `s.spawn[i]`, type's initial HP, level 0, no reload |
| Friendly tank on spawn | Wait; recheck every turn end and respawn when clear |
| Enemy tank on spawn | Destroy it regardless of HP, credit a kill to the respawning side (enemy respawns 3 turns later), and respawn normally |
| Bullets on spawn | Disappear without hits |

- Occupancy is checked after step 5: a spawn vacated by destruction this turn is immediately usable. An enemy arriving on the respawn turn is destroyed; one leaving that turn is safe.
- Previously fired bullets of destroyed tanks continue and still belong to them (11.9).

## 8. Supply and stars

- **Supply:** first batch at turn 10 end, then every 20 turns (30, 50, …, 290), 15 batches per game. Each supply point **without a star** gets one; existing stars do not stack. A tank standing there does not prevent spawning. Query with `star_at(s, cell)`.
- **Pickup** (turn end, before supply): an on-board tank below max level on a starred supply point consumes it: level +1, current HP +2, own `stars_eaten` +1. Either side may use any point.
- As pickup precedes supply, a tank already there on a supply turn collects only at the **next turn end**, if still there. A tank gains at most one level per turn.
- Tanks destroyed this turn do not collect.
- **Level-2 tanks do not collect:** the star stays, does not count as eaten, and may be collected by another tank.

## 9. Victory

At step 9:

1. One base at 0 HP and the other above 0: the surviving side wins.
2. Both reach 0 **on the same turn**, or both survive through turn 300: compare these in order; higher wins at the first difference. All equal means draw.

| Order | Measure | Field |
|---|---|---|
| 1 | Base HP | `s.base_hp` |
| 2 | Kills | `s.kills` |
| 3 | Stars eaten | `s.stars_eaten` |

- Even if a base reaches 0 in substep 1, finish the turn; both may reach 0. Then compare from kills, since HP ties at 0.
- Every opposing tank destruction counts, including friendly fire and enemy-spawn telefrags.
- Score: win 1, draw 0.5, loss 0.
- Afterwards `s.done` is true, `s.winner` is winning seat (−1 draw), and `step` no longer changes state. `reason_text(s)`: `base` for base destruction; `hp` / `kills` / `stars` for the deciding comparison; `draw`. Both bases down adds `both_` (e.g. `both_kills`); query `both_bases_down(s)` too.

## 10. Penalties

| Item | Rule |
|---|---|
| Unit | CPU ms consumed by your program; waiting/suspension excluded |
| First turn | 2000 CPU ms from startup, including `init` and first `decide` |
| Later turns | 50 CPU ms from submitting the previous commands to submitting these |
| Threads | All process threads combined; `budget_left()` sees only its calling thread |
| Wall safeguard | No reply in 4 seconds first turn / 2 seconds later counts as failure |
| CPU overrun | All three tanks stay; one failure. Referee does not wait: continued computation is charged to the next turn, so over 150 CPU ms in one turn means 3 consecutive failures |
| Illegal command | Two-step command for fire/armor: all three stay, one failure |
| Consecutive failures | CPU overruns and illegal commands combined: **3 consecutive failures lose**; a normal turn resets the count |
| Crash/exit | Immediate loss |
| Forfeit turn | Not resolved; game ends with `fault`. Simultaneous forfeits draw |

## 11. Commonly misunderstood cases

Coordinates are (x, y), right means x+1; commands use section 4 text. Every example was verified with `tank.hpp`.

### 11.1 Point-blank fire

A at (5,5) fires `FR`; B is adjacent at (6,5). Bullet starts (5,5), reaches (6,5) in substep 1 and (7,5) in substep 2.

| B's command | After substep 1 | After substep 2 | Result |
|---|---|---|---|
| Stay `S` | Bullet and B at (6,5) | | Hit in substep 1 |
| Toward A `L` | Blocked by A; same as above | | Hit in substep 1 |
| Retreat 1 `R` | B (7,5), bullet (6,5) | Both (7,5) | Hit in substep 2 |
| Sidestep `U` | B (6,4), bullet (6,5) | Bullet (7,5) | Miss, bullet continues |
| Fast retreat 2 `RR` | B (7,5), bullet (6,5) | B (8,5), bullet (7,5) | Miss this turn |
| Fast retreat then sidestep `RU` | B (7,5), bullet (6,5) | B (7,4), bullet (7,5) | Miss |

After retreating 2, the bullet is one cell behind and still approaching: another point-blank case. Next turn, stay or retreat 1 is hit; retreat 2 or sidestep avoids it.

With a one-cell gap (B at (7,5)), staying is hit in substep 2; retreating 1 avoids a hit this turn, leaving the bullet one cell behind.

### 11.2 Firing at each other

A at (5,5) fires `FR`; B to its right on the same row fires `FL`. At any separation the bullets cancel and both tanks are safe:

| B | What happens |
|---|---|
| (6,5), adjacent | Swap in substep 1 |
| (7,5) | Meet at (6,5) in substep 1 |
| (8,5) | Swap in substep 2 |
| (9,5) | Meet in substep 2 |

### 11.3 Fire toward an incoming bullet to cancel it

T at (5,5), bullet approaching leftward on the same row. `FR` cancels it by swap or same-cell meeting before it reaches T, whether it starts at (6,5), (7,5), (8,5) or (9,5).

Only if the shot actually fires. Reloading level-0 fast/armor tanks that fired last turn, and tanks at bullet cap (an earlier shot still elsewhere), treat `FR` as stay and are hit: substep 1 if incoming bullet starts (6,5), substep 2 if (7,5).

### 11.4 Bullets: swaps, meetings and misses

- Perpendicular bullets arriving in the same cell in the same substep cancel. A at (4,5) rightward and B at (6,3) downward both reach (6,5) in substep 2 and disappear.
- Passing through the same cell at different times is not a meeting. Put B at (6,4): it passes (6,5) in substep 1, A arrives in substep 2; both continue.
- Friendly bullets cancel, including over water; three bullets meeting all disappear.
- **A tank in the cell takes every hit, before cancellation.** A at (5,5) `FR`, C at (7,5) `FL`, 2-HP tank at (6,5): both bullets hit in substep 1, losing 2 HP.

### 11.5 Bullets and moving tanks

Bullet at (6,5), T takes one step:

| Bullet direction | T position/command | Result |
|---|---|---|
| Left | (5,5), `R`, toward bullet | T (6,5), bullet (5,5): swap, **hit** |
| Left | (6,6), `U`, enters bullet's current cell from side | Miss: bullet moves to (5,5) that substep |
| Right | (5,5), `R`, following bullet | Miss |

Check where the bullet ends, not starts: bullet (7,5) leftward, T (6,6) `U` → both reach (6,5) in substep 1, hit.

### 11.6 Contested cells, swaps and following

No side or tank has priority.

| Position | Commands | Result |
|---|---|---|
| A (4,5), B (6,5) | A `R`, B `L`, both target (5,5) | Both blocked |
| A (4,5), B (5,5), facing each other | A `R`, B `L`, swapping | Both blocked |
| A (5,5), B behind at (4,5) | Both `R` | A (6,5); B blocked, (5,5) occupied at substep start |
| Same, B fast | A `R`, B `RR` | B's first step fails; second enters vacated (5,5) |
| Fast A (5,5), B (4,5) | A `RR`, B `R` | A (7,5); B only tries substep 1 and fails |
| Fast A (4,5), fast B (8,5) | A `RR`, B `LL` | Both target (6,5) in substep 2, blocked: A (5,5), B (7,5) |

A blocked tank still occupies its old cell, blocking others trying to enter it.

### 11.7 A 0-HP tank remains until turn end

It still occupies its cell, blocks bullets and, if fast, takes its second step.

- **Blocks bullets:** X (1 HP) at (6,5), Y below at (6,6). A (5,5) `FR`, another bullet at (6,3) downward. Substep 1: A hits X to 0 HP. Substep 2: downward bullet reaches (6,5), hits X and disappears. At turn end X is destroyed once (one kill); Y is safe. Without X it would hit Y next turn, substep 1.
- **Occupies cell:** same A/X, fast tank (7,4) `DL`: second step into (6,5) fails because X remains, so it stops at (7,5). Cell clears next turn.
- **Still moves twice:** fast at (6,5) `RR`, hit to 0 at (7,5) in substep 1, still reaches (8,5) in substep 2 and is destroyed there at turn end.
- **Still fires:** firing precedes all hits. A shoots B while C shoots A: A and B may both be destroyed, yet B's shot elsewhere remains on board.

### 11.8 Fast second step can enter a wall destroyed in substep 1

Fast F at (6,5), brick at (7,5); teammate at (7,4) `FD` destroys it in substep 1.

- F `RR`: first step fails (movement before bullets), second enters (7,5).
- F `R`: blocked, stays (6,5).
- If the brick falls only in substep 2 (teammate fires from (7,3)), both `RR` steps fail.

Same for steel destroyed by a level-2 bullet in substep 1. Movement success cannot always be known when issuing commands; `legal_commands` therefore retains two-step commands whose second step targets brick/steel.

### 11.9 Old bullets still occupy slots after respawn

Fire tank (cap 1) at (0,5) fires `FR` on turn 1 and is destroyed that turn. It respawns at turn 4 end, but its bullet leaves the board only on turn 8 substep 1. Fire commands on turns 5,6,7,8 all become stay (even turn 8: count at turn start). It can fire on turn 9.

### 11.10 Reload boundaries

- Level-0 fast/armor: fire T, cannot T+1, can T+2. Fire type may fire every turn if below cap.
- **Failed shots do not restart reload.** Armor fires turn 1; bullet disappears turn 3. Turn 2: reloading, stay. Turn 3: reload finished, but cap reached, stay. Turn 4: may fire.
- **Cap counted at turn start:** even a bullet disappearing in substep 1 blocks the next shot this turn.
- **Level-1 fast/armor no longer reload, but cap remains 1.** Point-blank shots disappearing that turn allow firing every turn; distant shots still flying block it. Level-1 fire has cap 2 and can fire two consecutive turns.
- **Reload uses current level:** level-0 fast fires T, collects a star and reaches level 1 at T end, may fire T+1.
- Respawn clears reload.

### 11.11 Friendly fire and kills

- Teammates lose HP on friendly hits; their destruction awards the **opponent** one kill.
- Enemy respawn destroying you on its spawn awards it a kill.
- More hits than HP in a turn still means only one destruction/kill.
- Own-base damage reduces own HP and credits neither side's `base_dmg`.

### 11.12 Standing on supply when a star spawns

A tank parks at (7,7) before turn 10. At turn 10 end: pickup sees no star, then supply spawns one under it. It collects at turn 11 end; moving away on turn 11 leaves the star.

Another tank entering starred (12,7) on turn 11 collects at that turn end.

### 11.13 Max-level tank on supply

A level-2 tank does not consume the star or change stars eaten. Next batch adds none there, but the other two points spawn normally. Once it moves away, another tank of either side may collect.

## 12. Rule-value reference

Names are `Params` members in `tank.hpp` (`s.P`). **The header takes precedence over documentation.**

| Rule | Name | Default |
|---|---|---|
| Map side length | `size` | 15 |
| Enclosure brick layers | `ring` | 1 |
| Three tanks spawn one cell outside enclosure | `spawn_near` | true |
| Tank-type layout flipped left/right | `spawn_flip` | true |
| Supply points | `supply_points` | 3 |
| Base HP | `base_hp` | 24 |
| Base damage cap per turn | `base_hit_cap` | 1 |
| Fast: HP, steps/turn, bullet cap, reload turns | `types[FAST]`  fields `hp` `speed` `bullets` `reload` | 1, 2, 1, 1 |
| Fire: same fields | `types[FIRE]` | 1, 1, 1, 0 |
| Armor: same fields | `types[ARMOR]` | 2, 1, 1, 1 |
| Max level | `max_level` | 2 |
| HP per level | `level_hp` | 2 |
| Reloading types stop reloading at level 1 | `level1_no_reload` | true |
| Bullet-cap increase at level 1 for non-reloading fire type | `level1_bullets` | 1 |
| Bullets fired from level 2 destroy steel | `level2_steel` | true |
| First star batch, turn end | `supply_first` | 10 |
| Star batch period | `supply_period` | 20 |
| Turn ends after destruction until respawn | `respawn` | 3 |
| Enemy at spawn destroyed on respawn | `respawn_telefrag` | true |
| Bullets at spawn cleared on respawn | `respawn_clear_bullets` | true |
| Bullet swap/same-cell cancellation | `bullet_cancel` | true |
| Turn cap | `max_turns` | 300 |
| Tiebreak items and order | `tiebreak[0..n_tiebreak)` | Base HP, kills, stars eaten (`n_tiebreak` = 3) |
| First-turn budget (CPU ms) | Constant `BUDGET_FIRST_MS` | 2000 |
| Later-turn budget (CPU ms) | Constant `BUDGET_TURN_MS` | 50 |
| Wall safeguard: first/later turn (ms) | Constant `BUDGET_WALL_FIRST_MS`, `BUDGET_WALL_TURN_MS` | 4000, 2000 |
| Consecutive failures to forfeit | Referee constant | 3 |


Other `Params` members (`start_level`, `base_armor`, `base_regen` and the three starting with `proto_`) remain at defaults and have no effect in this contest.
