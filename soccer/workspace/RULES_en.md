[简体中文](RULES.md)

# 2v2 soccer · rules v1 (reference translation)

Two sides of two players compete for 1,200 turns on an enclosed field with cut corners. More goals wins. No throw-ins, corners, offside or fouls: the ball bounces off walls. Each player has an independent bot. Teammates exchange one byte per turn, with a protocol of their choosing.

The original base document permits reading and copying teammate code. **For this event, the actual opening prompt and team-format supplement superseded that clause: reading ideas and agreeing protocols were allowed, but copying whole programs or substantial blocks was prohibited.** See [format](../packages/format_en.md) and [amendment](../prompt_en.md).

The authoritative implementation is the C++17 header soccer.hpp, used by the referee. If prose differs, the header governs physics.

## 1. Field and objects

All coordinates are integers; x increases rightward, y downward, origin at top left.

| Item | Value |
|---|---|
| Field | 12000 × 8000; each corner cuts off a right triangle with 1000-unit legs |
| Goals | Openings in the middle of left/right walls, y ∈ [3000, 5000] |
| Player | Circle, radius 300 |
| Ball | Circle, radius 150 |

You always see your side attacking right: opponent goal at x=12000, your goal at x=0. The referee transforms positions, velocities and output vectors, so bots need not handle field-side assignment.

## 2. Actions

Output five integers each turn: `AX AY KX KY MSG`.

- Acceleration (AX, AY) is added to your velocity, clamped proportionally to length 40.
- Kick (KX, KY)=(0,0) means no kick. A nonzero kick is effective only if your center is within 500 of the ball at turn start and your cooldown is zero. Its vector is clamped to length 800 and **replaces** ball velocity. Simultaneous valid kicks are individually clamped, summed, then clamped again to 800.
- An effective kick sets cooldown to 5; it is not decremented at that turn's end. A kick at t prevents kicks at t+1 through t+5 (observed cooldowns 5,4,3,2,1); kicking is possible again at t+6. Invalid kicks have no effect, including on cooldown.
- MSG is 0..255, delivered to the teammate only on the next turn. Out-of-range values become zero.

Exact clamping: if x²+y² > m², d=floor(sqrt(x²+y²)), then (x,y)=(x*m/d,y*m/d), with division truncated toward zero.

## 3. Resolution order

1. Collect all four outputs and transform to field coordinates.
2. Resolve kicks using positions at turn start.
3. Add clamped acceleration to each player velocity.
4. Add velocities to player and ball positions.
5. Resolve collisions in this order:
   - Player/player: push overlapping equal-radius circles apart equally; exchange normal velocity with restitution 0.5. All pair corrections use the same pre-collision state and are added simultaneously. For exactly coincident centers, use their pre-move relative position (opposite relative velocity). If that also coincides, opponents separate toward their own goals; teammates separate with seat 0 upward and seat 1 downward. This preserves mirror symmetry.
   - Player/wall: push inside, clear the component toward the wall. Goal openings are walls for players.
   - Ball/player: push the ball outside each player and reflect approaching relative velocity with restitution 0.5, without affecting the player. Resolve simultaneously.
   - Goal: if ball-center y ∈ [3000,5000], x<0 scores for the side attacking left, x>12000 for the side attacking right.
   - Ball/wall if no goal: push inside and reflect the wallward velocity with restitution 0.8. Left/right walls do not exist for the ball within the goal's y range.
6. Apply friction: player velocity ×9/10, ball velocity ×97/100, truncated toward zero. Decrement cooldown except for players who kicked effectively this turn.
7. Increment turn. After a goal, add one point, reset all objects to kickoff positions, and let the conceding side kick off. After 300 turns without an effective kick, reset without changing scores, with the same side as the previous kickoff.

Exact collision arithmetic is in soccer.hpp (`players_collide`, `wall_collide`, `ball_players`). Without collisions, repeated acceleration 40 yields movement 40, 163, 258, 347, 379 at turns 1,5,10,20,30, approaching 391 units per turn (saved velocity 351). Cumulative movement at turns 10/20/30/40 is 1644/4790/8463/12318. An 800-speed kick rolls about 24,600 units without collisions, stopping after 124 turns. These are illustrations, not substitutes for exact simulation.

## 4. Integer arithmetic

Use 64-bit integer operations, division toward zero and integer square roots rounded down. Rational coefficients are applied as multiply then divide. Physics has no floating point or randomness except kickoff positions. Identical states and actions yield bit-identical results. Use `soccer::predict_ball` or copy `soccer::Game` to simulate.

## 5. Kickoff

- Ball at (6000,4000+δb).
- Kicking side: seat 0 stands 500 behind the ball; seat 1 is 2000 from its own goal line at y=4000+δ2.
- Other side: seat 0 is 1500 from midfield toward its own half at y=4000+δ1; seat 1 as above.
- Sides are mirrored. δb, δ1, δ2 ∈ [−600,600] are generated in sequence by the match seed's SplitMix64 at each kickoff. This is the only randomness.
- Velocities and cooldowns reset to zero.

## 6. Result and schedule

After 1,200 turns, more goals wins; equal goals draw. Two games per seed switch field sides, with each team kicking off once. Seeds per pairing and points are announced separately. In the actual schedule, first_kick stays 0 (left side kicks off) while the teams exchange sides. Switching both sides and the first-kick parameter would duplicate the mirrored game.

## 7. Input/output

Read stdin, write stdout, flush each output line, and exit at EOF. Send debugging output only to stderr.

Startup (seven lines; after the first line, numbers only):

```text
SOCCER2V2 1
W H GOAL CHAMFER RP RB
ACC_MAX PF_N PF_D BF_N BF_D
KICK_MAX KICK_REACH KICK_COOLDOWN
E_WALL_N E_WALL_D E_BALL_N E_BALL_D E_PP_N E_PP_D
STEPS STALL MSG_MAX
IDX STEP_MS FIRST_MS
```

Values: `12000 8000 2000 1000 300 150`; `40 9 10 97 100`; `800 500 5`; `4 5 1 2 1 2`; `1200 300 255`; then your within-team index, per-turn CPU budget and startup CPU budget in ms.

Each turn (six lines, own side attacking right):

```text
T LEFT MY_SCORE OPP_SCORE MATE_MSG
BX BY BVX BVY
X Y VX VY CD
X Y VX VY CD
X Y VX VY CD
X Y VX VY CD
```

Player rows are self, teammate, opponent seat 0, opponent seat 1. T begins at 0; LEFT is remaining turns; MATE_MSG is the teammate's previous message, zero at turn 0. CD is remaining kick cooldown.

Full example input (you are side B's seat 0, already transformed to your view):

```text
SOCCER2V2 1
12000 8000 2000 1000 300 150
40 9 10 97 100
800 500 5
4 5 1 2 1 2
1200 300 255
0 10 1000
0 1200 0 0 0
6000 4446 0 0
4500 4405 0 0 0
2000 3813 0 0 0
6500 4446 0 0 0
10000 3813 0 0 0
```

The opponent kicks off. Replying `36 -18 0 0 1` accelerates up/right, does not kick and sends byte 1, visible to the teammate next turn.

## 8. Limits and errors

| Condition | Handling |
|---|---|
| CPU budget | 10 ms each turn, measured from when the previous reply was read until this reply is read; work after replying counts toward the next turn |
| Turn 0 | 1000 ms from process launch, including startup parsing |
| CPU overrun | Replace action with `0 0 0 0 0`; still wait for that reply. At 30 accumulated overruns, terminate the bot and use zero actions thereafter |
| No reply within wall limit | 1 second, or 5 seconds on turn 0; terminate and use zeros thereafter. A reply arriving after the deadline is also late |
| Invalid output | Not exactly five decimal integers, magnitude above 10^9, or line over 1000 characters: zero action for that turn. Blank lines ignored |
| Crash, exit, closed stdout | Zero actions for remaining turns |
| Excess output | More than 64 queued output lines or over 8 MB total stdout: terminate and use zeros thereafter |
| Memory | 1024 MB per process; exceeding it counts as a crash |
| Stderr | Save at most 1 MB per bot per game, discard the rest |

CPU usage across threads adds together. Prefer fixed work or CPU timing over wall-clock cutoffs.

Each bot runs in its own Windows AppContainer. It cannot create child processes, read or write files, access the network or other processes; only minimal environment variables are exposed. The working directory is read-only. Failed forbidden operations return errors rather than automatically forfeiting the game. Local match.py uses this same sandbox.

## 9. Submission and compilation

Submit one C++ source file. Standard library headers and soccer.hpp are allowed; no other local files. The build.py `check_source` precheck rejects:

- System/process headers such as windows.h, names beginning win/nt or ending api.h, unistd.h, process.h, io.h, direct.h, fcntl.h, tlhelp32.h, and system header directories such as sys/.
- Quoted includes other than soccer.hpp; absolute or parent-directory includes; macro-based includes.
- Inline assembly (`asm`, `__asm__`) and `dllimport`.

Standard headers, including bits/stdc++.h, compiler SIMD headers, thread and chrono, are allowed. Comments and string literals do not count. The precheck is advisory protection; the runtime sandbox enforces isolation. The exact accepted/rejected list is in build.py.

Original Windows command (g++ / w64devkit, with soccer.hpp alongside):

```text
g++ -std=c++17 -O2 -static -Wl,--stack,67108864 player.cpp -o player.exe
```

## 10. Engine API

All names are in namespace soccer:

| Name | Purpose |
|---|---|
| Config | Rule parameters, defaults match these rules |
| Game | Complete state: ball, four player positions/velocities/cooldowns, score, turn. step(acts) advances a turn; observe(p) returns that player's view |
| predict_ball(cfg,bx,by,bvx,bvy,n,out) | Ball trajectory for n turns without players, including walls and goals, bit-identical to referee physics |
| tdiv, isqrt, clamp_len | Integer helpers |
| Obs, parse_action, protocol_step | Protocol parsing and output helpers |

To simulate in your view, fill a Game with self=0, teammate=1, opponents=2/3. Your view is side A attacking right. Each action in Game is still in its actor's own view, so opponent x components must be reversed.
