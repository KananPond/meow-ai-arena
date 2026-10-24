> English translation for reference; the contestants received the original Chinese text.

[简体中文](README.md)

# Tron light cycles

Write a bot: **edit only `submission/bot.cpp`**. Read `RULES.md` first (under 100 lines).

**Time:** starts on extraction. Aim to finish within 45 minutes; collection is forced at 60 minutes. Run `.\run.cmd clock.py` at any time to check elapsed time. Only bot strength counts. Whatever is in `submission/bot.cpp` at collection is your submission; also describe your strategy in a few sentences.

## Writing the bot

```cpp
#include "tron.hpp"
int decide(const tron::State& s) {     // called each turn
    // s.me: you; s.N: player count; s.cell[y][x]: board; s.x[k], s.y[k], s.alive[k]: player states
    return tron::U;                    // return tron::U / R / D / L
}
```

The supplied `submission/bot.cpp` is the simplest legal bot: keep going straight when possible. `bots/greedy.cpp` is the scoring origin (0 points); `bots/random.cpp` moves randomly. You may refer to both. `bots/` also contains two compiled-only sparring bots, excluded from scoring: `sparring` (medium) and `sparring_hard` (strong).

## Commands

Run here in PowerShell / Command Prompt; in bash replace `.\run.cmd` with `sh run.sh`. **No manual compilation:** `.cpp` inputs are compiled and cached automatically; compiler errors are shown directly.

```text
.\run.cmd arena.py check
.\run.cmd arena.py run
.\run.cmd arena.py run --n 1v1 --opp sparring
.\run.cmd arena.py run --opp sparring_hard
.\run.cmd arena.py run --opp greedy,history/old.cpp
.\run.cmd arena.py play submission/bot.cpp sparring --seed 5
.\run.cmd arena.py play submission/bot.cpp greedy greedy sparring
.\run.cmd arena.py show replays/game-xxxx.json 40
```

In order: check compilation and 2/3/4/12-player games in both modes; estimate your score across those player counts (about half a minute); 1v1 only against sparring; use hard sparring; use any comma-separated `.cpp` opponents rotated through other seats; play one formal game showing results, debug output and final board and saving a replay; play 4-player (list 2, 3, 4 or 12 participants); show the board after turn 40 of a replay.

`run` options: `--n` (`1v1`, `3-4`, `12`, `all`, or `2,3,4,12`), `--opp` (default `greedy,sparring`), `--games` (games per player count), `--seed` (another map batch), `--formal` (formal mode), `--workers` (concurrency). Participants may be `greedy`, `random`, `sparring`, `sparring_hard`, or file paths.

## Reading `run` output

One row per player count: games, fraction you finished first, your average rank points, the **greedy replacement's** average, the difference and its 95% interval. The replacement replays the same opponents, maps and seats with greedy in your place: it measures how much stronger you are than greedy. Official scoring uses BT ratings (`RULES.md`, section 3), a different scale but the same direction. The final row shows each type's score ×100 and the weighted total (1v1 50%, 3–4 players 30%, 12 players 20%). If the difference interval crosses 0, there is no clear difference yet; add `--games`. Local opponents differ from official ones, so use scores to compare versions only.

## Fast and formal modes

| | Fast (`run` default) | Formal (`play` default, `run --formal`) |
|---|---|---|
| Execution | All bots in one program, multithreaded | One process per bot, exactly as in official play |
| Budget | Measured, no penalty | Overruns penalized by continuing in the previous direction |
| Errors | Any bot crash stops the batch and identifies the game | Normal penalties; debug output saved in replays |

Bots that ignore time (fixed search effort or no search) produce identical turns in both modes. Search using `budget_left()` varies with machine load. **Keep a fixed-search-effort switch:** enable it for version comparisons in fast mode; disable it before submission and check formal mode (`play` or `run --formal`) for overruns/crashes.

## Other information

- Every `check`, `run` and `play` archives participating sources by content in `history/`, with timestamp and hash in the filename; `history/log.jsonl` records commands and results. Recover old versions there; do not delete it.
- **Replays:** open `viewer.html` in a browser and drag in a replay from `replays/` (space plays, ← → steps, H lists shortcuts).
- Files: `tron.hpp`, rule engine (same as referee); `tron_main.cpp`, main loop (do not edit); `arena.py`, `judge/`, local play/referee; `bots/`, baseline/sparring; `clock.py`, `run.cmd`, `run.sh`, clock/launchers; `build/`, `replays/`, `history/`, generated at runtime.
- Official compile command (usually unnecessary): `g++ -std=c++20 -O2 -static -Wl,--stack,67108864 -I. submission/bot.cpp tron_main.cpp -o bot.exe`
