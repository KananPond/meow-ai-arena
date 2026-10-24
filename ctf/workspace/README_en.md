[中文原文](README.md)

English translation for reference; the contestants received the original Chinese text.

# Capture the Flag

Write `submission/bot.cpp`: one program controls all 3 units on your side and supports 2–15 sides in the same game. Full rules and I/O: [RULES_en.md](RULES_en.md).

Timing begins at extraction. You have 60 minutes; only the final bot's strength counts, with no bonus for early hand-in. Some overrun is allowed; hand-in is forced at 90 minutes.

## Files

| File | Purpose |
|---|---|
| `RULES.md` | Rules and program interface |
| `submission/bot.cpp` | Your program, currently a starter with full input parsing and units standing still |
| `baseline.exe` | Medium-strength practice opponent with a conventional approach; source is not provided; do not decompile |
| `arena.py`, `engine.py`, `core.py` | Local referee, the same as the official tournament |
| `viewer.html` | Replay page template |
| `clock.py` | Check elapsed time |
| `run.cmd`, `run.sh` | Find Python and run the scripts |

## Common commands

Run from this directory. These examples use `.\run.cmd` for PowerShell and Command Prompt. In bash (including Git Bash on Windows and Linux), replace it with `sh run.sh`; keep all subsequent arguments unchanged.

```text
.\run.cmd clock.py                                         # elapsed time
.\run.cmd arena.py check                                   # check environment
.\run.cmd arena.py play submission/bot.cpp baseline --seed 7
.\run.cmd arena.py play submission/bot.cpp baseline baseline baseline baseline --out five
.\run.cmd arena.py show replays/five.json 120 --around 2     # inspect turns 118–122
.\run.cmd arena.py batch submission/bot.cpp baseline baseline --games 12
```

- `.cpp` programs compile automatically, cached by source in `build/`; unchanged source is not recompiled. Compiler errors are displayed if compilation fails.
- `play` runs one game, prints each program's score, rank, average and slowest turn time, and saves `replays/name.json` and `replays/name.html`. The page plays turn by turn.
- `show` prints the board, every unit's position, health and flag status, events, and each side's commands in the terminal.
- `batch` runs multiple games and summarizes mean score, mean rank, first-place count, faults and slowest time, without saving replays. Like the official tournament, it rotates all seats on each map (n games for n sides); choose a multiple of the side count if possible.
- `--seed` determines only the map. Flag-return spots differ every game, so rerunning the same seed can produce different results.
- Programs may appear repeatedly, including playing themselves.

## Submission

Save final code at `workspace/submission/bot.cpp`. Finish with a few sentences describing your strategy.
