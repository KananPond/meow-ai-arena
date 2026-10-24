[中文版](README.md)

# Game archives

| Directory | Original match ID | Time (UTC) | Phases | Messages | Result |
|---|---|---|---:|---:|---|
| [game1/](game1/) | 20261001_0319 | 09-30 19:19 – 23:05 | 22 | 568 | S1906M: 6-power agreed draw |
| [game2/](game2/) | 20261001_1002 | 10-01 02:02 – 07:20 | 33 | 839 | S1909M: 4-power agreed draw |
| [game3/](game3/) | 20261001_1906 | 10-01 11:06 – 10-02 04:38 | 50 | 920 | F1913R: Italy wins solo |

Each game directory contains:

| File | Contents |
|---|---|
| `全文.txt` | All private messages, public statements, orders and adjudications by phase (including bounce and dislodgement annotations); the easiest version to read directly |
| `state.json` | Referee archive: all messages with timestamps, orders and positions before/after adjudication for every phase, draw votes, administrator actions, final result and scores. Identity tokens have been removed |
| `名单.json` | Powers, models, tools, reasoning effort and draw seeds; games 2 and 3 also list the powers each model previously played |
| `scheduler.json` | Scheduler state: sessions, delivery progress, rematch draw seed, original opening text and administrator notices |
| `唤醒记录.jsonl` | One line per wake: power, phase, reason, start/end time, duration, token usage, success status and original error. `cost_usd` is the tool's API-price equivalent; Claude and Codex used subscriptions and were not billed this way. Two game-2 usage records were corrected during play to subtract the previous game's cumulative session usage; their `usage_note` fields explain this |
| `调度.log` | Full scheduler log (Beijing time): wakes, completions, retries, adjudications and freezes |
| `警报.log` | Freezes and alerts |
| `承诺核对.md` | Rule-based first pass for promises and suspected breaches; both false positives and false negatives occur, so use only as leads |
| `越界审计.md` | Post-event boundary-scan checklist for review; flags possible issues without deciding whether a rule was broken (see [CONDITIONS_en.md](../CONDITIONS_en.md)) |

The tools in [referee/](../referee/) can read these directories directly, for example `python -X utf8 统计.py --match ../games/game3`.
