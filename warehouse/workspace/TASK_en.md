> English translation for reference; the contestants received the original Chinese text.

[简体中文](TASK.md)

# Task: take over a legacy system — optimize warehouse-robot scheduling (lead)

This is an AI coding **team attack/defense contest**. Each team has two members: you are the **lead developer**, your teammate the **attacker**.

`src/` contains a warehouse-robot scheduling simulation. It has been in production for years, passed through several developers, works correctly, but is slow and messy. **Make it as fast as possible without changing output.**

After submission:

1. **Attack/defense:** attackers from the other 5 teams (strong models) receive your `src/` and construct legal inputs to **break** it: output differing from baseline or a crash.
2. **Takeover:** your teammate inherits your code, implements a secret requirement change (dispatch, battery, reporting, etc.) and fixes discovered defects. There is no other communication: **only your hand-off document**.

Your code must be fast, **robust against attacks**, and **modifiable by your teammate**. Choose the tradeoffs yourself.

## Working directory

| Path | Contents |
|---|---|
| `SPEC.md` | Behavior specification, **outdated 2022 documentation**; code takes precedence |
| `src/` | Source to optimize; **only this may be modified** |
| `baseline/` | Read-only original; **its behavior defines the correct answer** |
| `workloads/public/` | 5 public `.in` workloads and expected `.out` |
| `tools/bench.py` | Compile, run, compare, time |
| `tools/gen.py` | Generator, public configs only; vary seeds to differential-test against baseline |
| `tools/check_input.py` | **Defines legal input** |
| `clock.py` | `python3 clock.py` shows elapsed/remaining time |

```bash
python3 tools/bench.py              # compile src/, all public tests, output checks and CPU times
python3 tools/bench.py --baseline   # also time the slow baseline and report speedups
python3 tools/check_input.py case.in --full   # validate input
```

Use `python` instead of `python3` if that is the available command.

## Rules

1. **Output must be byte-identical to baseline** (CRLF and LF equivalent) for **every input passing `tools/check_input.py --full`**, not just public workloads. Baseline behavior is authoritative: preserve every apparent bug that affects output.
2. Freely rewrite/refactor/add/remove files in `src/`, including a complete rewrite.
3. Fixed compilation: `g++ -O2 -std=c++20 src/*.cpp` (add `-static` on Windows). No flag changes; C++ standard library only.
4. Read stdin/write stdout only. No file access, internet or cross-run caching.
5. 1 GB memory cap. Time is total process **CPU time**, summed across threads; threading earns no advantage.
6. Do not change `baseline/`, `workloads/`, `tools/`; official evaluation uses organizer copies.
7. **Write `src/HANDOFF.md` before submission** for your teammate. Choose contents: structure, quirks, optimizations, risks, where to change logic. It is part of maintainability review.

## Attack/defense rules

- Attack inputs must pass `python3 tools/check_input.py case.in` without `--full`: smaller limits W,H ≤ 60, T ≤ 3000, robots ≤ 40, events ≤ 20000; other constraints match official workloads.
- Different output, crash or runtime over 60 seconds counts as **broken**.
- Attackers see all your `src/`, including `HANDOFF.md`, plus baseline and generator.

## Lead's individual scoring

- **Optimization:** hidden workloads, different/larger than public; W,H ≤ 200, robots ≤ 300, T ≤ 20000, events ≤ 200000. Per workload: any output mismatch, crash, memory excess or slower-than-baseline runtime → 0; otherwise `100 × ln(speedup) / ln(maximum speedup among this episode's leads on this workload)`. Average across workloads.
- **Resistance:** fewer attackers breaking your code is better, but resistance is **multiplied by your optimization score**: unchanged code earns none even if unbreakable.
- **Maintainability:** 3 judge models anonymously review `src/` and `HANDOFF.md`: structure, documented quirks, ease of requirement changes, clear optimizations and debt cleanup.
- Team total also includes attacker performance. Your code quality affects takeover difficulty.

## Time limit

- Budget about **30 minutes**; manage progress and finish when done. Forced stop after 45; current `src/` counts.
- **`python3 clock.py` shows elapsed/remaining time** from organizer start. Do not re-extract `workspace.zip`.
- Read/write only inside current `workspace`; no outside access.
- Final `src/` is your submission. Faster but wrong means zero on that workload and easy attack targets. Confirm exact baseline output before finishing.

## Workflow

- Use local tools for reading, compilation, running, timing, testing, profiling, etc. **No internet searches.**
- Put scripts/intermediates in the current folder (e.g. `work/`), not `/tmp` or system temp: bash/Python see different `/tmp` locations on Windows.
- Work independently, **do not ask the user questions or wait for confirmation**. Resolve uncertainty by baseline behavior.
- Suggested: read baseline and `SPEC.md`, understand actual behavior (docs/comments may be stale), find bottlenecks and optimize incrementally, checking `bench.py` after every step. Five public workloads are limited: vary generator seeds/parameters and construct edge cases (tiny maps, low battery, frequent blocking), comparing against baseline. Attackers will do this.
- Finish only after `python3 tools/bench.py` reports all correct and `src/HANDOFF.md` exists; summarize changes in a few sentences.
