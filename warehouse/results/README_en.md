[简体中文](README.md)

# Results and verdicts

Except for this explanatory document, the files are unmodified outputs of competition referee scripts or judge models. Team A–F are defined in [the draw](抽签结果_en.md). “第 5 期” means Episode 5. Paths beginning `赛事/…` or `裁判/…` refer to the organizer's local tree; see the [referee documentation](../referee/README_en.md) for the mapping.

These originals use earlier terminology: “attacker” means tester; “breakthrough” or “attack” means finding counterexamples; “held” means no counterexample was found; “resistance” refers to counterexamples; and “taken down” counts implementations for which a counterexample was found.

| File | Contents |
|---|---|
| `最终成绩.md` / [English](最终成绩_en.md) | Complete results: all three leaderboards, final timings on 9 hidden workloads, baseline ratios, initial versus precise rerun timings, blind review, 7 defects, final retesting and audits. **Authoritative numerical results.** |
| `团队成绩.md`, `团队成绩.json` | Three leaderboards and score components, output by `团队计分.py` |
| `results1.jsonl`, `第1轮_速度成绩.md` | Round 1: 6 lead submissions on 8 hidden workloads; each JSONL line gives status, CPU seconds per run, wall time, and memory; the Markdown file summarizes them |
| `results2.jsonl` | Round 3: 6 final submissions on 9 hidden workloads; `precise` records repetitions and accumulated CPU time |
| `主程基准.jsonl` | Original lead submissions on old-format versions of Round 3 workloads, used for added-value scoring. Team E is absent; see [conditions](../CONDITIONS_en.md) |
| `修复.json` | Number of team regression cases repaired by each final submission |
| `results2_初测.jsonl`, `主程基准_初测.jsonl`, `团队成绩_初测.md`, `团队成绩_初测.json` | Results before the precise rerun. **These are the first timings of the rematch, not the invalidated first competition.** The top three took only 0.03–0.05 seconds (2–3 timer ticks) on light workloads, with several exact ties. A rerun improved precision without changing ranks |
| `抽签结果.md`, `抽签结果.json` | Team draw |
| `历代战绩.json` | Previous results of the 12 models from Episodes 1–4, used to populate prompts. Its source notes are local paths absent from this repository; this original prompt input was preserved |
| `round2/矩阵.json`, `round2/矩阵.md` | Official verdicts on 28 Round 2 cases, reasons, and baseline/submission times. `✖` means at least one case from that tester was upheld for that implementation |
| `round2/缺陷点归类.md`, `round2/缺陷点归类.json` | Manual classification into 7 defects and tester scores |
| `blind-review/汇总.md`, `blind-review/汇总.json` | Final blind-review scores and judges' one-sentence comments |
| `blind-review/<judge>/` | Each judge's 12 `代码XXX.json` files (scores and evidence on 5 dimensions), `总览.md`, `草稿.md`, and follow-up `百分制.json` |
| `blind-review/对照表.json` | Anonymous IDs mapped to teams and stages; hidden from judges during review |
| `retest/判定.md`, `retest/判定.json` | Official final-retest verdicts |
| `retest/<judge>/cases/finalN/` | Two judges' counterexamples and `说明.txt` inspection notes. No `.in` means no counterexample was found; the note records what was checked |
| `retest/对照表.json` | Maps `finalN` to final team submissions |
| `final-check/性能探针.md`, `final-check/性能探针.json` | Performance portion of the final audit |
| `final-check/赛后流水线.log` | Post-contest retest verdicts, precise timing, scoring, two random differential-test rounds (including each final submission's 0-mismatch record), and performance probes. The generating `赛后流水线.sh` batch script is not included |

Two details:

- The opening of `round2/缺陷点归类.md` once incorrectly said 21 of 28 cases were upheld and 7 rejected. It was corrected during open-source preparation to 20 upheld and 8 rejected, matching its tables and `矩阵.json`. A correction note remains; defect classifications and scores are unaffected.
- Blind-review evidence quotes anonymous copies where model/team names became “某某”, so a few quoted words differ from the originals in `../submissions/`.

Episode 7 results, counterexamples, and independent reproduction records are under [ep7](ep7/README_en.md). Those files had paths sanitized and are not byte-identical originals. Model-authored comments and inspection records remain in their original language.
