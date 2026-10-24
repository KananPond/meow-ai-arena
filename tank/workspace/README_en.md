> English translation for reference; the contestants received the original Chinese text.

[简体中文](README.md)

# Tank Battle

Write `submission/bot.cpp`: 1v1, each side has three tank types guarding a base. Full rules: `RULES.md` (starts with a quick reference); exact implementation: `tank.hpp`, used by the referee and authoritative if they differ.

Two halves, clock starting at extraction. Each half is 30 minutes, earliest submission minute 20, forced collection minute 45. Only strength counts; early submission gives no benefit. After the first half everyone's bots play a round, you receive the half-time package, then continue improving.

**Submit:** only `submission/bot.cpp`. Its contents at each half's end are that half's version. Also describe your strategy in a few sentences.

## Writing the bot

Define both functions in `bot.cpp`:

```cpp
#include "tank.hpp"

// Once per game. s: initial state; me: seat (0 bottom, 1 top).
void init(const tank::State& s, int me) {}

// Once per turn; commands for fast, fire and armor tanks.
tank::Orders decide(const tank::State& s, int me) {
    tank::Orders orders;                                  // default: all stay
    for (int type = 0; type < 3; ++type) {
        const int i = tank::tank_id(me, type);            // tank ID
        tank::Cmd list[tank::N_CMDS];
        const int n = tank::legal_commands(s, i, list);   // meaningful commands; first is always stay
        orders.cmd[type] = list[n - 1];
    }
    return orders;
}
```

- No input/output code needed: state is passed as `tank::State`. Copy it and call `tank::step` to simulate ahead.
- Globals/statics persist between turns and start at initial values each game.
- Debug to stderr (`fprintf(stderr, ...)` or `std::cerr`). `play` shows the last lines; full output is saved by turn in replays. stdout is also merged into debug output without interfering with play.
- No dependencies beyond `tank.hpp` and the standard library. To change engine behavior, copy the needed code into `bot.cpp`. `<windows.h>` is allowed, before or after `tank.hpp`.
- Illegal commands (e.g. two steps for a non-fast tank) make all three tanks stay and count as a failure.

### Thinking budget

- First turn: 2000 CPU ms for startup, `init` and first `decide`; later 50 CPU ms per turn. This is **your program's own CPU time**, excluding suspension/input waiting.
- `tank::budget_left()` returns remaining CPU ms as a floating-point number. Example: `while (tank::budget_left() > 3) { /* search another layer */ }`. The referee measures the entire process externally and sees a fraction of a ms more; leave 2–3 ms margin.
- Overruns make all three tanks stay and count as failure. **3 consecutive failures lose**, counting overruns and illegal commands together. Spending over 150 CPU ms in one turn means three consecutive failures. Crash/exit loses immediately.
- Threads are allowed; all threads' CPU time is combined. `budget_left()` sees only its calling thread.
- Wall-clock safeguard: no reply in 2 seconds (4 first turn) counts as failure.

## Compile and play

No manual compilation: `.cpp` inputs compile automatically into `build/`, cached until source changes; errors show compiler output. Run here in PowerShell/Command Prompt; in bash replace `.\run.cmd` with `sh run.sh`.

```text
.\run.cmd clock.py
.\run.cmd arena.py check
.\run.cmd arena.py play submission/bot.cpp submission/bot.cpp --seed 7 --out first
.\run.cmd arena.py show replays/first.json 20 --around 2
.\run.cmd arena.py fast submission/bot.cpp submission/bot.cpp --games 200
.\run.cmd arena.py batch submission/bot.cpp submission/bot.cpp --games 20
.\run.cmd arena.py vs submission/bot.cpp submission/bot.cpp --games 200
.\run.cmd arena.py expand replays/first.json
.\run.cmd arena.py html replays/first.json
```

Participants may be `.cpp` or compiled programs; one program may play both sides.

| Command | Action |
|---|---|
| `clock.py` | Reports e.g. first half, 12.3 minutes used; earliest submission minute 20 (7.7 left), half duration 30, forced collection 45. Switches to second half after half-time extraction |
| `check` | Checks Python, compiler, compilation of `submission/bot.cpp`, and completion of one game |
| `play A B` | One formal game, A seat 0, B seat 1. Prints result, budgets, faults, last debug lines; saves `replays/NAME.json` and `.html`. `--seed` selects map; `--style` is `auto` (seed-based), `open`, `brick`, `water`, `steel` |
| `show REPLAY TURN` | Terminal board after that turn: tanks, bullets, commands, events and debug output. `--around 2` includes two turns either side |
| `fast A B` | Fast batch, source only |
| `batch A B` | Formal batch |
| `vs NEW OLD` | Compare versions; fast if both are source, formal otherwise. Override with `--mode fast` / `--mode formal` |
| `expand REPLAY` | Full per-turn states/events in `NAME.frames.json` for scripting |
| `html REPLAY` | Convert an existing replay (e.g. half-time) to `NAME.html` |

Batch commands (`fast`, `batch`, `vs`) play both seats per map: `--games 200` means 100 maps, consecutive seeds from `--seed` (default 1); `--workers` sets concurrency. **The final output line summarizes** opponents, W-D-L, score rate ± uncertainty, games and faults. Replay a game with `play`, same seed and seats.

### Formal and fast modes

| | Formal (`play`, `batch`) | Fast (`fast`) |
|---|---|---|
| Execution | Exactly as official play: separate process per bot | Both bots in one program, direct calls |
| Speed | Simple bots: tens of games/second per worker | Simple bots: hundreds/second per worker |
| Budget | Overruns penalized | Measured only |
| Errors | Crashes, overruns, illegal commands penalized/recorded | Illegal commands penalized; any crash, hang or exception stops the batch, identifying bot/game |
| Debug | Saved in replays | Hidden |
| Participants | Source or executable | Source only |

### Reproducibility

The game has no randomness; what matters is how search stops:

- **Bots ignoring time** (fixed depth/nodes or no search) are deterministic: same map/programs/seats give the same fast game every time, identical turn for turn to formal mode if no overrun. Concurrency/load do not affect them; use fast mode for large comparisons.
- **Bots stopping by `budget_left()`** search different amounts depending on machine state. Single vs. batch, fast vs. formal, low vs. high concurrency can all differ; more concurrency increases variation, which gets mixed into comparisons.

**Keep a fixed-search-effort switch for comparisons, then submit using the budget.** For example:

```cpp
#define FIXED_NODES 0     // comparisons: fixed count, e.g. 20000; submission: 0, stop by budget
static bool keep_searching(long nodes) {
    return FIXED_NODES ? nodes < FIXED_NODES : tank::budget_left() > 3;
}
```

Save both versions as `.cpp`, use the same fixed value, compare with `vs` (fast). Once chosen, restore 0 in `submission/bot.cpp`, then check formal mode (`play` or low-concurrency `batch`) for overruns/crashes. Judge proximity to the 50 CPU ms limit using single-game `play` usage.

### Reading `vs`

**These numbers illustrate formatting and are invented, not real results:**

```text
tank-cpp-0.1  fast mode  new vs old  200 games (100 maps, seeds 1–100, both seats per map)  8 workers  1.2 seconds
  new: W 96 D 20 L 84   score rate 53.0% ± 4.1% (95% interval, by map)
  new seat 0: W 50 D 9 L 41    seat 1: W 46 D 11 L 43
  37 of 100 maps had turn-for-turn identical seat-swapped games (no difference between programs); 63 informative maps
  … (ending reasons, both bots' usage)
Summary: new vs old  96-20-84 (W-D-L)  score rate 53.0% ± 4.1%  200 games  faults: none
```

- Score rate: win 1, draw 0.5, from the first/new program's view. Uncertainty is a 95% interval by map (both seat-swapped games are one sample). Only if rate minus uncertainty exceeds 50% can you call the new version stronger; otherwise add games.
- Identical maps provide no information: small changes often produce identical moves, making the swapped games the same game. With few informative maps, a rate near 50% does not establish equal strength; try `--seed 1001` or more games.
- Seats are listed separately: wins concentrated in one seat often reflect maps/type placement rather than version differences.

## Allowed and prohibited

- **Any amount of local parallel play is allowed:** any `--workers`, multiple `arena.py` commands (more concurrency slows games and reduces time-limited search depth).
- **Half-time opponents may be sparring partners or automatic parameter-tuning targets:** batch games and scripted parameter sweeps are allowed.
- **Do not decompile opponents.** Use them only for matches.

## Replays and old versions

- Open `replays/NAME.html` generated by `play`/`html` offline in a browser: animated board, tank states/commands, events, usage, faults and debug text. Space plays/pauses, arrows step, `[` `]` previous/next key event, `H` shortcuts; append `?turn=37` to open turn 37. Or open `viewer.html` and drag in `*.frames.json` from `expand`.
- For scripted analysis, read expanded JSON; for exact turn values use `show`.
- Every `check`, `play`, `batch`, `fast`, `vs` archives sources by content in `history/` (timestamp/hash filename, duplicate content saved once), and appends time, command, versions and summary to `history/log.jsonl`. Recover versions and match history there. Do not delete it.

## Half time

After all first-half versions play a round, extract the half-time package into `workspace/halftime/`:

- First-half standings.
- Your own game replays, same format as local `play`; use `show` / `expand` directly.
- Other contestants' executables in `halftime/opponents/`, without source: formal mode only (`play`, `batch`, `vs --mode formal`). Replace filenames as needed:

```text
.\run.cmd arena.py play submission/bot.cpp halftime/opponents/p01.exe --seed 3
.\run.cmd arena.py batch submission/bot.cpp halftime/opponents/p01.exe --games 20
```

Second-half timing starts when the package is extracted.

## Files

| File | Purpose |
|---|---|
| `RULES.md` | Rules |
| `tank.hpp` | Single-header engine included by your bot: state, exact one-turn `step`, legal commands, map generator and budget query |
| `submission/bot.cpp` | Your bot, initially a compiling template with legal actions |
| `baseline.exe` | Medium-strength compiled-only baseline; formal mode only, e.g. `arena.py batch submission/bot.cpp baseline.exe` |
| `tank_main.cpp` | Input, function calls, output, timing; do not edit; identical in official play |
| `arena.py`, `viewer.html` | Local play/review and browser replay viewer |
| `engine.py`, `core.py`, `judge/` | Referee, identical to official play |
| `clock.py`, `run.cmd`, `run.sh` | Clock and launchers finding Python and running scripts |
| `build/`, `replays/`, `history/` | Generated compile cache, replays and version archives |

Official compilation (usually unnecessary; `arena.py` handles it):

```text
g++ -std=c++20 -O2 -static -I. submission/bot.cpp tank_main.cpp -o bot.exe
```
