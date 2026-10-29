[简体中文](README.md)

# Two leaderboards and per-game results

Use the [team board](团队榜_en.md) and [individual board](个人榜_en.md), finalized after the October 29 rerun. No combined overall score is published.

| Folder | Event | Games | Seeds per pairing |
|---|---|---:|---:|
| `halftime/` | First-half bots | 2,112 | 32 |
| `final/` | Final team event | 2,904 | 44 |
| `individual/` | Each model's player + partner | 3,264 | 16 |

Each folder contains `results.json.gz` (all statistics and per-game records) and `programs.json` (actual program selection, source/executable hashes and fallback notes). Half-time and final folders also preserve their original Chinese `成绩.md` summary; the two boards linked above are the complete public final standings. Original JSONL rows are already represented in `games`, so that duplicate is omitted.

Top-level JSON fields: `standings`, `games`, `programs`, `references`, `bt`, `seeds`; individual data also has `teams`. Games contain `id`, `A/B`, `seed`, seed index `k`, `first_kick`, `order`, `seats`, `score`, `goals`, `faults`, `cpu`, `kicks`, `digest`, `time`. Reference teams begin with `ref-` and are unranked.

Paths are relative to `soccer/`; `<build>` replaces historical compiler-output locations and is not a repository path. `replay` describes the full historical set; only files in the [sample manifest](../replays/selection.json) are shipped. Other data and historical hashes are unchanged. Read gzip files with Python's standard `gzip.open`.

From `soccer/`, `python tournament/score.py final` or `individual` recomputes points and goal records; `--bt` additionally refits ratings. `python check_archive.py` verifies archive consistency without modifying results.
