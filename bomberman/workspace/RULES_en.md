[中文原文](RULES.md)

English translation for reference; the contestants received the original Chinese text.

# Bomberman rules v0.5-public-state (public)

## Objective and actions

Each player controls one unit and aims for the highest combined survival and kill score. Every turn, all living players see the same complete public state and independently choose U/D/L/R/S (up/down/left/right/wait) and a bomb bit 0/1. There is no attack button, equipment, items, health or respawn. Explosions, lingering flames and closing walls eliminate units.

Coordinates start at 0, with (0,0) at the top left, x rightward and y downward. `#` is a permanent wall, `+` a destructible crate, `.` empty ground. Players cannot overlap. Adjacent players may swap positions simultaneously, and longer movement cycles are allowed. Collisions do not kill. Moves into blocked terrain leave the player in place; direct reversals are allowed.

For all player counts: capacity 2 bombs, range 3 cells excluding the centre, fuse 4 turns; flames cover the explosion turn and the next turn. Absolute action turns t start at 0; a bomb placed on t explodes in the explosion phase of t+4. Bombs are placed at the original position before moving; bombing and moving can happen on the same turn.

## Parameters by player count

| Players | Board | Turn limit | First closing-wall action turn | Turns between layers |
|---:|---:|---:|---:|---:|
|2|13×13|300|150|20|
|3|15×15|340|175|20|
|4|17×17|450|250|25|
|5|19×19|475|225|25|
|6|21×21|525|250|25|
|7|21×21|550|275|25|
|8|23×23|600|300|25|
|9|25×25|650|325|25|
|10|25×25|675|350|25|

2 and 4 players use the fixed parameters above. For other counts p: board side `2*ceil(sqrt(14*p))+1`, first closure `150+25*(p-2)`, turn limit `first_closure+interval*(side//2)+25`. Interval: 20 for 3 players, otherwise 25. Eventually no survivable cells remain, so normal games usually end before the turn limit.

## Maps

The outer ring is permanent wall, as are interior cells where both x and y are even. The generator places crates on remaining cells with probability 0.42, except within Manhattan distance 2 of a spawn. Every spawn has at least two initially walkable exits; paths are connected after removing crates. Crate locations are public from the start; there is no randomness during play.

2-player spawns are opposite corners and the map has 180° symmetry. For 4 players, spawns are at the four corners with 90° symmetry. Other counts use equally spaced points, with integer rounding, on the ring of odd-coordinate positions along the inner boundary. The seed selects the starting point; crates are generated independently. These counts do not guarantee fully equivalent spawns within one game. See `make_map()`.

## Exact turn phases

Input describes the start of the turn, before any of these phases. Resolve in order:

1. **Cleanup and closing walls:** remove expired flames. Eliminate all players with protocol errors/timeouts on the same turn. On a closure turn, cells whose distance from the outer boundary is at most the new layer number become permanent walls. First closure is layer 1; the original outer wall is layer 0. Players on new walls die immediately and cannot move away that turn. Bombs on new walls are removed without exploding; crates and flames there are removed too.
2. **Place bombs:** try at each player's pre-move position. A player must be alive, have no bomb at that position, and have fewer than 2 active bombs. If multiple eligible players request a bomb on the same cell, all requests fail, avoiding ID bias. Ineligible requests do not participate in conflicts. Failed placement does not affect movement or count as a protocol error. Old bombs due to explode this turn still occupy capacity.
3. **Movement:** move one cell or wait; all players are processed simultaneously. Walls, crates and bombs block entry, leaving the player in place. A player may wait on a bomb already beneath them or just placed, or leave it; after leaving they cannot re-enter while the bomb remains. After terrain checks, if several players target the same cell (including a waiting player's cell), all stay. Then anyone targeting a cell occupied by a player already staying must also stay; propagate until stable. All remaining moves succeed simultaneously, including swaps, cycles and following a departing player. No ID-based priority.
4. **Explosions:** bombs whose fuse expires or that sit in old flames explode, extending up to 3 cells in each of four directions. Permanent walls block without burning. Crate cells burn and the crates are destroyed, but block further propagation. Bomb cells burn, immediately trigger that bomb, and block that ray. Every explosion uses the crate/bomb layout at the start of this phase: destroying a crate does not let another flame pass through it this turn, and an already exploded bomb still blocks other rays in this phase.
5. **Flames and elimination:** remove detonated bombs and destroyed crates. Flames are lethal on explosion turn t and t+1. After movement, players on old or new flames are eliminated simultaneously. Dead players' bombs remain and keep counting down unless later removed by closing walls. Then increment the turn number.

Only check for game end after all phases finish. A lone survivor after closure still faces this turn's explosions; no one may survive. Players dying in different phases of the same turn still tie.

## End and ranking

End when at most 1 player remains alive or the turn limit is reached. Game score = survival score + 0.5 × kill credit.

- Survival score: 1 point per opponent outlived; 0.5 each for opponents eliminated on the same turn or also surviving at game end. Final survivors count as eliminated later than all dead players. For n players, survival rank r and t players tied at that rank: `n-r-(t-1)/2`.
- Kill credit: attribute active flames on the victim's cell to the original bomb owners, excluding the victim, and split one kill equally among distinct owners. Multiple flames from one owner count once. Chain reactions do not transfer ownership of downstream bombs; old flames retain their source; dead owners can still receive credit. Closing walls, protocol errors and deaths caused solely by one's own flames award no kill credit.
- Rank by game score descending, ties shared, with no additional tiebreaker. Rank = `1 + number of players with strictly higher scores`. The referee compares exact fractions. The last survivor is not guaranteed first place.

Example with five players: second by survival gets 3 survival points; three unshared kills add 1.5, totalling 4.5. A last survivor with no kills gets 4.

Bombs placed and crates destroyed are display statistics only. If multiple players hit a crate on the same turn, each receives one crate-destruction count.

Official scoring converts game ranks into pairwise outcomes, ties as half a win, and fits Bradley–Terry strength. Every player count has equal total weight; the public baseline is anchored at 0, without rescaling to the episode's highest score.

The nine contestants and one public baseline in this event play 2-, 5- and 10-player games, each count weighted 1/3, with 20 maps per count. Two-player games use all pairs and both seats; five-player games use balanced groups and forward/reverse seat rotations; ten-player games rotate all entries forward and backward. Total: 2600 games, 960 per entry. Every pair meets 80, 90 or 100 times in five-player games; all entries have equal appearances and seat counts.

BT display rating = `400×log10(contestant strength/baseline strength)`. Below-baseline ratings are negative. After weighted aggregation, each pair that met receives one virtual draw, giving finite estimates even for all-win or all-loss records.

## Text protocol

Once after process startup:

```text
INIT side_length player_count my_id turn_limit fuse range capacity flame_turns first_closure_turn closure_interval
```

Every turn:

```text
TURN t
BOARD
... (side_length rows, characters # + .)
P id x y alive
... (player_count P lines; alive is 0/1, IDs start at 0)
BOMBS k
B owner x y explode_at
... (k B lines; explode_at is an absolute action turn)
FLAMES f
F x y last_lethal_turn
... (f F lines; last lethal turn is inclusive)
STATUS player_count
S id death_turn kill_numerator kill_denominator
... (player_count S lines; living players have death_turn -1, kill credit is an exact fraction)
SOURCES s
O x y owner last_lethal_turn
... (s O lines; currently active flame sources per cell, one line per owner)
END
```

STATUS gives cumulative kill credit and elimination turns as of this input. SOURCES retains each owner's flame expiry separately. All contestants receive the same public state; INIT identifies your own ID. Read through END before replying.

Reply with exactly one line, single-space separated: `t U 0`. Uppercase U/D/L/R/S; 0/1; end with a newline and flush. LF and CRLF are supported; at most 256 bytes per line. Do not output future turns in advance or debugging on stdout. Invalid output or timeout immediately eliminates you.

First turn: 2000 ms, then 50 ms per turn. Timed from starting to send input until the complete response line arrives, including I/O and system scheduling. Process creation is excluded from the first-turn budget.

Timeout, process exit or output-protocol violation eliminates the program.
