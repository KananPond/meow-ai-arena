[简体中文](README.md)

# Episode 7 · Legacy-code optimization: prompt and defect reports

| Path | Contents |
|---|---|
| `提示词_主程.txt` | Exact opening sentence sent unchanged to all 13 sessions (253 Chinese characters, no trailing newline) |
| `astra-review/round1/` | First GPT-6 Astra inspection: organizer task `检查任务.txt`, overall `report.md`, final reply `final.txt`, per-target `findings/<name>/README.md`, and explanations inside counterexample subdirectories |
| `astra-review/round2/` | Second inspection; per-target `findings/<name>/report.md`, timeout explanations under `findings/<name>/timeout_candidates/**/README.md` |

No separate contestant package: it is episode-5 round-1 `../../workspace/`. Original `workspace.zip` SHA-256: `49cc19d2486aaf9b9904bf4a76dce756f38a229e4ed57ce85d85c680532e0f33`, the same file the six episode-5 leads received. Each folder held only this zip when the prompt arrived.

Unlike episode-5 round-1 `../../prompt.md`, this is a solo prompt: no teams/teammates or attack/defense wording.

## Astra inspections

Both were organizer-launched GPT-6 Astra Codex CLI sessions (`codex exec`, no permission prompts). The working directory held only copies of the 12 timed submissions, baseline, old docs, original task and checking tools. Doubao's new submission was excluded.

| | Round 1 | Round 2 |
|---|---|---|
| Effort | max | ultra |
| Time | ~17 minutes | ~46 minutes |
| Scope | Correctness only: different output, crash, compilation failure. Official timing was running on the machine, so no performance/timeout tests, at most 2 targets concurrently | Focus on timeout defects, referencing the first round and historic cases (`learning/`, omitted). Subagents allowed, at most 4 targets concurrently |
| Self-reported findings | Pangu 2 output defects, Mercury 1, LongCat compilation failure; none found in other 9 | 5 new performance issues (Pangu 2, Qwen 1, Mercury 1, Haiku 1), 3 mechanisms; no new correctness issue |

**Model conclusions do not count directly.** Organizer independently reran each candidate ([rules](../../results/ep7/counterexamples/README_en.md)); only confirmed cases entered the pool. Of 53 candidates, 50 legal, 18 confirmed on targets: Haiku 3, Mercury 4, Pangu 7, Qwen 4. Two were excluded when baseline exceeded 10 seconds in unified testing. Counterexamples do not change placement.

Reports are Astra's original text, only path-sanitized: organizer-local absolute paths replaced with placeholders, internal event prefixes with `ep7`. Many links target omitted `work/`, `build/`, `tools/`, `learning/` and input/output/log files inside `findings/`, so they do not open. These large working files are omitted; confirmed inputs are in `../../results/ep7/counterexamples/`. “No scoring” in both prompts addresses Astra: findings are recorded without changing placement. Model reports remain in their original language.
