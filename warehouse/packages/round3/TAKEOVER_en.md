> English translation for reference; the contestants received the original Chinese text.

[简体中文](TAKEOVER.md)

# Takeover task (tester · round 3)

Regression testing is over. **Take over your own lead's code.** `workspace/src/` contains it and `src/HANDOFF.md` is your lead's hand-off.

**This round primarily tests optimization:** how fast the changed program runs on large workloads. Correctness is prerequisite; speed is the main scoring item. In order:

1. **Fix** other teams' counterexamples in `workloads2/regression/`, where your lead disagreed with baseline.
2. **Implement** `CHANGE.md`.
3. **Optimize (main focus)** the changed program. Changes may invalidate prior optimizations (fixed-priority ordering, cached dispatch results); bolting on requirements can be correct but slow and require redesign.

## Permission

Modify `src/` freely, from small fixes to refactoring or full rewrite. If unsalvageable, **restart from `baseline/`**, the original messy code whose behavior is authoritative.

## Directory

| Path | Contents |
|---|---|
| `CHANGE.md` | Requirement change: **read carefully** |
| `src/` | Lead's code + `HANDOFF.md`; **only this may be modified** |
| `baseline/` | Original, pre-change behavior |
| `SPEC.md` | Outdated 2022 specification; baseline prevails |
| `workloads2/public/`, `workloads2/edge/` | 4 small public workloads and 30 edge cases with outputs: **correctness only, too small for speed** |
| `workloads2/big/` | **3 near-official-scale workloads with outputs for speed**; `reference_cpu.json` gives unoptimized reference CPU times on this machine |
| `workloads2/regression/` | Team's prior counterexamples with expected outputs |
| `tools/bench2.py` | Compile, run, byte-compare; `--big` adds large workloads and speedups |
| `tools/gen2.py` | More new-format inputs, no outputs; includes big configs |
| `tools/check_input.py` | Validity checker, `--p2` for new format |
| `workloads/public/`, `tools/bench.py` | Pre-change workloads/tools for comparing old baseline behavior |

```bash
python3 tools/bench2.py              # small public + edge + regression, seconds
python3 tools/bench2.py --big         # 3 large workloads, reference speedups, same algorithm as official scoring
python3 tools/bench2.py --skip-edge   # only 4 small public workloads
```

Use `python` if `python3` is unavailable.

**Large workloads:** speedup = reference CPU time / your CPU time. Reference is clean but wholly unoptimized; ~1× means essentially unoptimized. Official hidden workloads have comparable/larger sizes (T up to 12000), different parameters, and more types (low battery, long, mixed). Official timeout is reference time ×3 +10 seconds.

**Test cost:** default `bench2.py` takes a few to a dozen seconds, so rerun freely. Optimized `--big` takes a dozen or so seconds; a workload slower than reference is stopped early (reference times ~34,68,128 seconds). While slow, start with `--only p2_big_flood` rather than repeating all. Baseline is slow: keep custom random differential inputs small.

**Regression cases:** converted from old format by appending `agingEvery=1000000000`, `carryCost=1`. These disable changes, isolating the old defect; expected output adds only `aged=0` to reports. Missing/empty directory means no counterexample against your lead last round.

Some are **performance counterexamples** (filename or prior decision says timeout): output correct but much slower than baseline, unfinished in 60 seconds. Fix these too: finish within 60 seconds with correct output; `bench2.py` uses that limit for regression cases. Consider whether the same optimization degradation affects large workloads.

Your lead only supports old input, so initial `bench2.py` may fail everything. This is normal until new-format support is added.

## Rules

- Only **new-format** inputs required (two appended `PARAMS` values). Every input passing `tools/check_input.py --full --p2` must match “baseline + CHANGE.md” byte for byte.
- Same compile command, standard-library-only, stdin/stdout-only, CPU timing (threads confer no advantage), 1 GB cap.
- Current folder only; no internet, **other models/subagents**, Task / Agent or background parallel agents. Work yourself in this session.
- Other programs, including 5 other testers, share the machine. Kill only your own PIDs, **not process names** such as `taskkill /IM python.exe`, `Stop-Process -Name python`.
- Scripts/intermediates in current folder, e.g. `work/`, not `/tmp` or system temp (Windows bash/Python see different `/tmp`).

## Scoring

This round accounts for 50 of 100 team points:

| Component | Team weight | Calculation |
|---|---|---|
| **Takeover speed** | **20** | Hidden changed workloads: mismatch, abnormal exit or timeout → 0; otherwise `100 × ln(speedup) / ln(field maximum speedup on this workload)`, relative to reference. Average workloads |
| Correctness | 20 | Correct hidden fraction (3/4) + fixed team regression fraction (1/4) |
| Maintainability | 10 | Anonymous model review of final `src/`, including `HANDOFF.md` |

**Individual ranking** has two speed measures: takeover speed (absolute final speed) and **value added**, comparing final to lead's submission on the same maps/events as lead's hidden set, with just two new parameters. A fast lead gives less headroom but a better speed starting point; a slow lead reverses this. Each is 20% of the individual score.

- Speed uses logarithmic comparison to field maximum: 5× versus someone else's 50× earns only about 40%. Implementing requirements without optimization makes both speed and value-added nearly 0.
- **Correctness is the gate:** fast but wrong gives zero speed points on affected workloads. After every optimization run default `bench2.py`, then `--big`.
- Append to `src/HANDOFF.md`: changes, current optimization mechanisms and invalidation conditions.

## Time

- **45-minute hard limit**, current `src/` counts. **No early-finish bonus.**
- Fix and implement quickly, then **spend remaining time optimizing**, saving 5 minutes for a full check. Continue while `--big` has clear headroom.
- **Do not leave `src/` uncompilable or wrong for long**. Before large changes save a passing copy in `work/`.
- **`python3 clock.py` in `workspace/`** shows elapsed/remaining time from round start.
- Work independently; no user questions/confirmation waits.
- Finish after all default tests pass; report the three `--big` speedups and briefly explain changes, including rewrites/large changes, optimizations and reasons.
