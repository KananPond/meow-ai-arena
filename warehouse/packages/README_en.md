[简体中文](README.md)

# Round-2, round-3 and post-race packages

The fixed round-1 package is preserved in `../workspace/`. Rounds 2 and 3 differed by team (other teams' or own team's code) and were assembled during the event. This directory holds the fixed portions; actual additions are listed below. Assembly scripts are in `../referee/`.

| Path | Contents |
|---|---|
| `round2/review/` | Fixed Round 2 testing package: `REVIEW.md`, old `SPEC.md`, tools for validity, generation and local `try_case.py` decisions |
| `round3/TAKEOVER.md` | Round-3 instructions |
| `round3/workspace/` | New round-3 items: `CHANGE.md`, `tools/bench2.py`, `tools/gen2.py`, 4 changed small public workloads, 30 edge cases, 3 big workloads, all with expected outputs |
| `round3/regression/<lead>/` | Valid counterexamples against that team from round 2, converted with expected new-format output. X in `fromX_…` is the finding tester's team; `slow_` means timeout. Team A (Kimi K3) had none, so no directory |
| `retest/REVIEW.md` | Post-race final retest instructions |
| `blind-review/评审说明_同一对话版.md` | Actual instructions received by judges |
| `blind-review/评审说明.md` | Earlier unused one-conversation-per-entry version, retained because `referee/生成盲评包.py` reads it |

## Actual package contents

**Round 2 testing package** (`referee/生成攻防包.py`, extracts to `review/`):

- All of `round2/review/`;
- `clock.py`, generated from `referee/clock_重赛模板.py` with round parameters;
- `baseline/` and `workloads/public/`, same as `../workspace/`;
- `submissions/队X/src/`: other 5 leads' `../submissions/round1/` code. Distributed copies replaced attack-related wording in hand-offs and code comments with neutral terms; official judgments used originals;
- `cases/队X/把用例放在这里.txt`: 5 empty submission directories.

**Round 3 takeover package** (`referee/生成接手包.py`, extracts to `workspace/`):

- `round3/TAKEOVER.md`;
- Round-1 `SPEC.md`, `baseline/`, `workloads/`, `tools/`, replacing `check_input.py` with the neutral-worded round-2 version;
- All of `round3/workspace/`;
- `workloads2/regression/`: team's cases from `round3/regression/<lead>/`;
- `src/`: own lead's `../submissions/round1/<lead>/`; hand-off neutralized, code unchanged;
- `clock.py` with round-3 parameters.

**Post-race final retest** (`referee/生成赛后评审包.py`, extracts to `review/`):

- `retest/REVIEW.md`, `SPEC.md`, `CHANGE.md`, `clock.py`;
- `reference/`: `../referee/参考实现/v2_变更/`;
- `tools/`: `check_input.py`, `measure.py`, `gen.py`, `gen2.py`, `try_case.py` adapted for new format/reference comparison;
- `submissions/finalN/src/`: 5 other teams' finals, randomized IDs (`../results/retest/对照表.json`), model/vendor/tool/team names replaced with “某某”.

**Post-race blind review** (`referee/生成盲评包.py`, `生成赛后评审包.py`; one folder per judge):

- 12 randomized `代码XXX/src/` entries, 6 leads + 6 finals (`../results/blind-review/对照表.json`), anonymized likewise;
- `锚点/屎山基线/` = `../workspace/baseline/`; `锚点/干净参考/` = `../referee/参考实现/v2/`;
- `CHANGE.md`, `评审说明.md` containing the same-conversation instructions, and judge-specific `评审顺序.txt`.

To rebuild, restore directories using [the referee guide](../referee/README_en.md), then run the relevant scripts.

Episode-7 prompt and GPT-6 Astra defect reports are in [ep7/](ep7/README_en.md). That event used `../workspace/`.
