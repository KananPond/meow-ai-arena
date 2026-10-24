> English translation for reference; the contestants received the original Chinese text.

[简体中文](REVIEW.md)

# Final-version regression-testing task

The last round is over. Six teams submitted **final versions** implementing `CHANGE.md` (order aging, loaded-movement energy, report `aged`) and further optimization. Finals accept only new input (`agingEvery carryCost` appended to `PARAMS`).

Official testing used only 9 hidden workloads and cannot expose many defects. Perform post-race equivalence verification: **write regression tests for every final in `submissions/`, finding disagreements with `reference/`.** The reference is clean and unoptimized, implementing “original behavior + CHANGE.md”; **its behavior defines correctness**.

This extra post-race project is published separately and does not affect total scores.

## Directory

| Path | Contents |
|---|---|
| `submissions/finalN/src/` | Finals with `HANDOFF.md`; names replaced with “某某” for anonymity |
| `reference/` | Correct reference; preserved quirks tagged `[怪行为N]`, changes `[变更N]` |
| `CHANGE.md` | Requirement changes |
| `SPEC.md` | Outdated 2022 document; reference code prevails |
| `tools/check_input.py` | Checker, always new format here |
| `tools/gen2.py`, `tools/gen.py` | Generators for input construction |
| `tools/try_case.py` | Local testing using official decision rules |
| `cases/finalN/` | **Put counterexamples here** for submission |

```bash
python3 tools/try_case.py --build                # build reference and all finals first
python3 tools/try_case.py cases/final1/case1.in   # one case
python3 tools/try_case.py --all                  # all cases/
```

Use `python` if needed. Official compilation: `g++ -O2 -std=c++20`, plus `-static` on Windows. Scripts/intermediates belong inside `review/`, e.g. `review/work/`, not `/tmp` or system temp.

## Rules

1. At most **3** `.in` cases per final in `cases/<finalN>/`.
2. New-format inputs passing `tools/check_input.py`: W,H ≤ 60, T ≤ 3000, robots ≤ 40, events ≤ 20000, `agingEvery ≥ 1`, `carryCost ≥ 1`. Reference must finish within 10 seconds.
3. **Decision:** normal target completion within 60 seconds and byte-identical output = consistent; otherwise inconsistent (different output, abnormal exit, timeout). Timeout cases rechecked on an idle machine: leave margin, preferably >90 seconds locally; prepare two or three sizes if useful.
4. Explain each case in `cases/<finalN>/说明.txt`: filename, defect file/function, why it differs.
5. Current folder only, no internet or other-model help (subagents, Task / Agent, background parallel agents); work yourself in this session.
6. Shared machine: kill only your own PIDs, **not names** such as `taskkill /IM python.exe`, `Stop-Process -Name python`.

## Results

- Count independent root-cause defects per final, not multiple triggers of the same defect. Use the 3 slots for distinct defects.
- Authors are strong and passed all local tests; **some finals may have no findable counterexample**. Say none found and document coverage in `说明.txt`; do not pad with locally unverified cases.
- Focus on new requirements interacting with quirks, cache/incremental invalidation for new requirements, extreme `agingEvery=1`, large `carryCost`, very low battery, and performance degradation where output is right but much slower than reference.

## Time

- **45-minute hard limit**, current `cases/` counts; no early bonus. Use time while finals remain without findings.
- `python3 clock.py` shows elapsed time from package extraction.
- Save cases as found, not at the end.
- No user questions/confirmation waits.
- Finish with `python3 tools/try_case.py --all`; list each defect (final, location, reason), finals with none found, and coverage.
