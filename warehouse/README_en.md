# Warehouse scheduling (episodes 5 and 7)

This directory holds two events built on the same task: optimize a slow, messy C++ warehouse-robot simulation while keeping its output byte-identical to the original ("baseline"), including five odd behaviours that the outdated docs describe differently.

- **Episode 5** (the main contents of this directory): 12 models in 6 two-model teams, three rounds (lead optimizes, tester hunts counterexamples, tester takes over and implements a requirement change). The full write-up is in Chinese in [`README.md`](README.md); a summary is in the repository root [`README_en.md`](../README_en.md). Conditions and incidents: [`CONDITIONS.md`](CONDITIONS.md).
- **Episode 7 · Legacy-code optimization round**: described below. Source files, result tables and reports are in Chinese.

## Episode 7 · Legacy-code optimization round

Episode 7 ("District King Championship") had two events. The first was the Tron light-cycle tournament (see [`../tron/`](../tron/)); the second was this warehouse task. The same 10 contestants as in the Tron event each did the episode 5 round-1 "lead" task alone: the **same task package** (`workspace/`, the identical `workspace.zip` the episode 5 leads received), the **same one-sentence prompt** for everyone (`packages/ep7/提示词_主程.txt`), 30 minutes suggested and a hard 45-minute limit. Afterwards every submission was timed on an otherwise idle machine on the episode 5 **8 hidden workloads** against the **same baseline**, and ranked by the geometric-mean speed-up over the 8 workloads. Claude Sonnet, Opus and Haiku did the same task under the same conditions as a reference and are not ranked.

### Differences from episode 5

- **Solo, one round**: no teams, testers, takeover, requirement change, blind review or total score.
- **Tools**: all 10 contestants used the OpenCode CLI 2.0.19 (`run --standalone --auto`, unattended); the three Claude models used Claude Code 2.1.301. Channels are listed in `CONDITIONS.md` (Chinese).
- **Speed only**: a submission gets a geometric-mean speed-up only if all 8 workloads are correct. The baseline was not re-timed; the baseline CPU times measured on the same machine on 10-17 were reused, which leaves a systematic uncertainty of about ±5% per workload and about ±1% on the geometric mean.
- **Counterexamples are recorded but do not change the ranking**: GPT-6 Astra hunted for defects in the 12 timed submissions over two sessions. Every candidate input was re-run independently by the organizer; the 16 that held, plus 10 carried over from episode 5 and an internal 10-17 evaluation, were run against all submissions.

### Ranking

| Rank | Model | Correct workloads | Geo-mean speed-up | Counterexamples (inputs that hold) | Time (min) | Code |
|---:|---|---|---:|---|---|---|
| 1 | StepFun Step 5 preview | 8/8 | 139.4× | 0 | 45.0 (time limit) | `submissions/ep7/step-5-preview/` |
| 1 | Tencent Hunyuan Hy4 preview | 8/8 | 138.2× | 0 | 45.0 (time limit) | `submissions/ep7/hunyuan-hy4-preview/` |
| 3 | Qwen3.8-27B | 8/8 | 8.7× | 4 (4 timeouts) | 45.0 (time limit) | `submissions/ep7/qwen3.8-27b/` |
| 3 | Inception Mercury 3 (diffusion model) | 8/8 | 8.6× | 5 (1 wrong output, 4 timeouts) | 45.0 (time limit) | `submissions/ep7/mercury-3/` |
| 5 | Doubao Seed 2.1 Turbo (episode 5 result carried over) | 8/8 | 8.4× | 1 (1 wrong output) | 39.2 (episode 5) | `submissions/round1/doubao-seed-2.1-turbo/` |
| 6 | ERNIE 5.1 | 8/8 | 1.3× | 0 | 36.4 (finished on its own) | `submissions/ep7/ernie-5.1/` |
| 7 | Mistral Large 4 | 8/8 | ≈1× (code identical to the baseline) | 0 | 45.0 (time limit) | `submissions/ep7/mistral-large-4/` |
| 7 | Xiaohongshu dots3-note preview | 8/8 | ≈1× (code identical to the baseline) | 0 | 45.0 (time limit) | `submissions/ep7/dots3-note-preview/` |
| 9 | openPangu-2.0-Pro | 1/8 (7 workloads timed out) | more than 2× slower than the baseline | not run in the unified pass; 7 held in the independent re-run (2 wrong output, 5 timeouts) | 45.0 (time limit; resumed once, 7 min 36 s added) | `submissions/ep7/openpangu-2.0-pro/` |
| 10 | Meituan LongCat-2.0 | 0/8 (compile error) | — | compile error | 45.0 (time limit; resumed once, 1 min 44 s added) | `submissions/ep7/longcat-2.0/` |

Reference (not ranked):

| Model | Correct workloads | Geo-mean speed-up | Counterexamples | Time (min) | Code |
|---|---|---:|---|---|---|
| Claude Sonnet 5.5 | 8/8 | 169.7× | 1 (timeout); 12 inputs not run | 26.0 | `submissions/ep7/sonnet-5.5/` |
| Claude Opus 5.5 | 8/8 | 101.0× | 1 (timeout); 12 inputs not run | 32.7 | `submissions/ep7/opus-5.5/` |
| Claude Haiku 5.5 | 8/8 | 98.1× | 3 (timeouts); 12 inputs not run | 33.8 | `submissions/ep7/haiku-5.5/` |

- **Ties**: Step vs. Hunyuan differ by 0.9% and Qwen vs. Mercury by 1%, within the ~±1% systematic uncertainty from the reused baseline, so they were ranked as ties (organizer's decision). Mistral and dots submitted code byte-identical to the baseline, so their speed is the baseline's own; they share 7th–8th.
- **Doubao** also submitted new code in this event; the organizer decided not to time or use it (it is not included here); the ranking uses Doubao's episode 5 lead submission, timed on the same machine on 10-17.
- **openPangu**: the screening workload did not finish in 180 s (baseline 33.5 s); only `hid_huge` finished in time and correctly (229 CPU s vs. 113 s for the baseline), so its speed is recorded as "more than 2× slower" as a lower bound.
- **LongCat**: `sim.cpp` uses `__m128i` without including the header that declares it, so it fails to compile with the common build command.
- **Counterexamples** are counted per distinct input (SHA-256), not per root cause; 0 only means none of these inputs broke the code. The three Claude submissions were not run on 12 of the new inputs (those were found against other submissions). Per-input details: `results/ep7/counterexamples/README.md`.
- **Time** runs from the prompt to submission, minus compensated interruptions; 45.0 is the hard limit. Step, Hunyuan, Mercury and Mistral were not stopped automatically at 45 minutes; the organizer reverted everything they changed in `src/` after the deadline (from the session event logs) and counts 45 minutes. The Claude times are taken from the last reply in each session. The "优化用时" column in `results/ep7/汇总.md` is the raw script value and differs for these rows; see `CONDITIONS.md`.
- **Each model ran once.** This is the result of this event, not a capability ranking. During the 45 minutes 6–7 sessions shared the machine, so the contestants' own timings were slower; the official numbers come from the later exclusive run.

### Directory

| Path | Contents |
|---|---|
| `submissions/ep7/` | The 12 timed `src/` folders, byte-for-byte; see `submissions/ep7/README.md` |
| `results/ep7/` | Unified result table `汇总.md`, full data `结果.json`, sealed-submission manifest, compiler error, records of the independent counterexample re-run (absolute paths replaced with placeholders); see `results/ep7/README.md` |
| `results/ep7/counterexamples/` | The 26 counterexample inputs used in the unified pass with baseline outputs, plus 2 excluded ones in `excluded/` |
| `packages/ep7/` | The prompt, and the tasks and reports of the two GPT-6 Astra defect-hunting sessions; see `packages/ep7/README.md` |

To run a model's code yourself: copy `workspace/` somewhere, put the files from `submissions/ep7/<model>/` into the copy's `src/`, and run `python tools/bench.py`. The 8 hidden workloads used for the official timing are in `referee/隐藏负载/`.

Not included: session transcripts and run logs, the organizer's launch and evaluation scripts (they contain local paths and proxy settings), the Astra working directories (build outputs, intermediate inputs and logs, about 180 MB), and the code and materials of the internal 10-17 evaluation.
