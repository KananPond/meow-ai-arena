> English translation for reference; the contestants received the original Chinese text.

[简体中文](REVIEW.md)

# Regression-testing task (tester)

This is an AI coding team contest. Each team has two members: your **lead** just optimized a warehouse scheduling simulator; you are the **tester**.

The original `baseline/` works but is slow and messy. Six leads optimized it subject to byte-identical output. Optimization can accidentally change behavior, so **equivalence verification** is needed.

**Write regression tests for the other 5 optimized implementations, finding inputs where they disagree with the reference baseline.** Each counterexample demonstrates non-equivalence. This is conventional differential testing: baseline is the reference, the five implementations are test targets.

Another round follows; its task will be announced then.

## Directory

| Path | Contents |
|---|---|
| `submissions/队X/src/` | 5 targets, including authors' `HANDOFF.md` documents, which may reveal their approaches |
| `baseline/` | Reference; **its behavior defines correctness** |
| `SPEC.md` | Outdated 2022 specification; reference code takes precedence |
| `tools/check_input.py` | **Defines legal test inputs** |
| `tools/gen.py` | Generator, a starting point for construction |
| `tools/try_case.py` | Local testing with the same rules as official evaluation |
| `cases/队X/` | **Put counterexamples here**: your submission |

```bash
python3 tools/try_case.py --build               # first: build reference and all targets (initially slow)
python3 tools/try_case.py cases/队B/case1.in     # one test
python3 tools/try_case.py --all                 # all cases/
python3 tools/check_input.py case.in           # validity only
```

Use `python` if `python3` is unavailable. All targets/reference use official `g++ -O2 -std=c++20` flags, plus `-static` on Windows. Put scripts/intermediates **inside `review/`**, e.g. `review/work/`, not `/tmp` or system temp: Windows bash and Python see different `/tmp` locations and files may appear to vanish.

## Rules

1. At most **3** `.in` cases per target in `cases/<team>/`. Beyond 3, only the first 3 lexicographically count. The placeholder `把用例放在这里.txt` may stay or be deleted.
2. Pass `tools/check_input.py` without `--full`: W,H ≤ 60, T ≤ 3000, robots ≤ 40, events ≤ 20000; see checker for format/ranges. Reference must finish within 10 seconds. Invalid cases are ignored without penalty.
3. **Decision:** normal completion within 60 seconds with byte-identical reference output means “consistent”; otherwise “inconsistent” (different output, abnormal exit or unfinished at 60 seconds). Only inconsistent cases count as counterexamples.
4. **Explain every case** in `cases/<team>/说明.txt`: one or two lines per case, filename, defect location (file/function), why output differs. Unexplained cases still count, but the organizer must classify them unaided.
5. Read/write only current folder; no internet or **other-model help**, including subagents, Task / Agent tools and background parallel agents. Do all work yourself in this session. Do not change target or baseline code; official decisions use organizer copies anyway.
6. Organizer reruns every case under these rules; official decisions prevail.
7. Other programs, including 5 other testers, share the machine. Kill only your own processes by PID, **never batch-kill by name**, e.g. `taskkill /IM python.exe` or `Stop-Process -Name python`.

## Scoring

- **By defect, not by submissions.** One defect = one independent root cause in one implementation. Three distinct defects count 3; three triggers of one defect count 1.
- Organizer groups all counterexamples after the round: cases fixed by the same code change are one defect. Use the 3 slots for **different** defects.
- **Rarer findings are worth more:** defect value = 1 + fraction of eligible testers who missed it. Found by everyone: 1; only you: close to 2.
- Find easy defects, but the gap comes from those others miss, including targeted cases requiring understanding of the optimization. **Do not stop after finding one counterexample per implementation.**
- You do not test your own lead; credit for that implementation is the other testers' average, neither advantage nor disadvantage.
- Counts in your individual score and team total (20 of 100 team points).

## Time

- **45-minute hard limit**, current `cases/` counts. **No reward for finishing early**; use time while implementations remain unbroken or may have more defects.
- **`python3 clock.py`** shows elapsed/remaining time from round start.
- Put cases in `cases/` **as you find them**, not at the end.
- Work independently; **no user questions or confirmation waits**.

## Hints

- Caches, incremental updates and rewritten data structures can break equivalence: look at invalidation, boundaries and apparent baseline bugs that must remain.
- Read suspicious code and construct targeted inputs; often more effective than blind random tests. Random differential testing helps too, within final size limits.
- Quality varies widely: some fail quickly under random tests; some require understanding their optimizations.
- Finish by running `python3 tools/try_case.py --all`, checking explanations for every case, and listing every defect (implementation, location, reason).
