[简体中文](README.md)

# Reproduce the official tournaments

| File | Contents |
|---|---|
| `tournament.py`, `bt.py`, `schedule.py` | This episode's platform: schedule freezing, compilation, CPU affinity, per-batch speed checks, resumable play, Bradley–Terry ratings. Byte-identical to the contest versions. Rewritten for 1v1; separate from the root `arena/` multiplayer platform used since episode 3 |
| `halftime.json`, `final.json`, `progress.json` | Half-time, final and progress configs. Program paths are repository-relative; `seed` is the original randomly drawn schedule seed. The local compiler path was removed: unset, it checks `CXX`, then g++ in `PATH`. If w64devkit is only under the root `tools/`, set `CXX` first (see `../README.md`). Names such as `Astra` and `Opus@1` are unchanged: schedules sort by name, so renaming changes them |
| `sparring.cpp` | Organizer-written search reference, named `sparring` in the tables. This is the actual compiled source, with 4 macros prepended by the event script to fix search effort per turn and make results independent of machine state. Not given to contestants |

Run here (the final took about 70 minutes on the contest PC; interrupt and repeat the same command to resume):

```bash
python tournament.py plan final.json        # freeze schedule into run-final/赛程.json
python tournament.py run  run-final         # play
python tournament.py rate run-final         # results: run-final/成绩.md
```

Replace `final` with `halftime` or `progress` for the other tournaments. Improvement requires fitting progress and half-time games together, done by the organizer's event script during the contest; see [the supplementary tables](../results/附表_en.md) for the method.

- **The contestant-package path must be ASCII-only.** `tournament.py` rejects non-ASCII `kit` paths: Windows g++ cannot open them under some locale settings. Copy the entire `tank/` folder to an ASCII-only path if needed.
- Maps, seats and pairings can be recreated exactly. Before release, all three configs reproduced the original `games_sha` fingerprints.
- Per-game results may vary. The game has no randomness, but many bots stop searching with `budget_left()`, so search depth depends on machine state. Before release, the first 352 final games were replayed on the contest PC: 338 matched turn for turn and 347 had the same winner. Overall standings should agree; adjacent tied places may swap.
- By default only performance cores are used, with one physical core per bot (`pin: big`); concurrency depends on local core count. Inspect it with `python tournament.py cpus final.json`. The limit is 50 CPU ms; a busy machine gives time-limited search less depth.
