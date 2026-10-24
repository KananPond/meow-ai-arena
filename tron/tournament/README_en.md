[简体中文](README.md)

# Final event scripts (archive)

| File | Contents |
|---|---|
| `tourney.py` | Final scheduling, common compilation, CPU affinity, game execution, imports, scoring, tables, replays and in-game statistics; byte-identical to the contest version |
| `bt.py` | Bradley–Terry fitting and bootstrap intervals, called by `tourney.py`; byte-identical |
| `hostlib.py` | Shared config, path, log and message-template utilities required by `tourney.py`; byte-identical |
| `赛程_1018终场.json`, `赛程_1020补打.json` | Schedules for the 10-18 final and 10-20 additions: game type, map seed, seats, board, bootstrap block and frozen `digest`. Organizer-local paths replaced with `<…>`; otherwise unchanged. `RESULTS_*.md` says seeds are unpublished, reflecting the earlier plan; they were published at release |
| `final.json` | Config for the 10-20 additions/refit: board rosters, anchor, weights, scheduling parameters and imports. Organizer-local paths replaced with `<…>`, compiler path removed; otherwise unchanged |

Scoring starts in `score()` in `tourney.py` (BT rating per type, Doubao = 0, 50/30/20 weights, 1000 stratified block-bootstrap resamples) and `rank_score()` (old rank-point method, reference only). Pairwise comparisons, exclusions of greedy/filler copies and import filtering are described in the introduction and section 7 of [the official results](../results/RESULTS_正式榜_en.md).

**These files alone cannot reproduce the final:**

- `tourney.py` has no CLI entry point. The organizer's controller called it and also handled starting, timing, sealing, time credits and messages; that controller is not included.
- The final imported the 10-18 final and two fast previews (`../results/预览并入.jsonl.gz`). Recalculation requires restoring their original directory layout according to `imports` in the config.
- Schedules are here (`赛程_*.json`); seeds and seats are also in per-game records. Replay any game using them (see [the results guide](../results/README_en.md)).

To host a new event under these rules, schedule games with the contestant package's `arena.py` (`run_games` calls the referee directly), following `../challenge.py`, and fit ratings with `bt.py`.
