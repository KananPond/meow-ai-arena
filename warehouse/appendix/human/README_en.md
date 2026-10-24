[简体中文](README.md)

# Human team

Two human contestants also did the round-1 lead task using the same package (`../../workspace/`). Neither entered rounds 2–3. Their submitted code/hand-offs and separately measured post-race results are here.

**These results do not count officially** and appear on none of the three boards. Numbers below were measured later on the same contest PC under round-1 methodology, in a separate measurement from the AI results. Neither human had a 45-minute limit.

## Contestants

- **能工智人** (Bilibili: 能工智人No1145): provincial second prize in informatics competition, no prior engineering-code experience. Independently modified one version in about 2 hours, and made another with Gemini.
- **Cat:** C++ beginner. Five work sessions recorded at the start of his hand-off total 20 hours 11 minutes.

## Directory

| Folder | Contents |
|---|---|
| `能工智人/` | Independent version, 5 code files and `HANDOFF.md` |
| `能工智人+Gemini/` | Gemini-assisted version, 5 code files and `HANDOFF.md` |
| `Cat/` | 4 code files and `HANDOFF.md`; removed `old_stuff.cpp` |

All three accept old input only (`../../workspace/workloads/`). Same compilation as round 1: `g++ -O2 -std=c++20 *.cpp`, plus `-static` on Windows. Copy `../../workspace/` elsewhere, replace its `src/` with a submission, run `python tools/bench.py`.

## Post-race results (unscored)

Round-1 timing: 8 hidden workloads, CPU time, median of 3 runs, only 1 if a run exceeds 10 seconds. All 28 round-2 tester cases were run once and compared with baseline.

| Code | 8 hidden workloads | Geo-mean speedup over baseline | Speed S1 (normalized to official AI maximum per workload) | Peak memory over 8 workloads | 28 round-2 cases |
|---|---|---:|---:|---:|---|
| 能工智人 | All correct | 1.7× | 9.8 | 16.1 MB | All 28 match baseline |
| 能工智人+Gemini | All correct | 108.8× | 100.2 | 22.0 MB | 23 match, 5 differ |
| Cat | All correct | 2.6× | 24.0 | 17.4 MB | All 28 match baseline |

| Code | medium | huge | crowded | dynamic | flood | lowbattery | long | mixed |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 能工智人 | 2.0× | 1.0× | 1.9× | 1.2× | 4.6× | 1.1× | 3.1× | 1.1× |
| 能工智人+Gemini | 185.2× | 79.6× | 178.6× | 17.7× | 819.5× | 85.7× | 323.1× | 18.5× |
| Cat | 1.6× | 22.6× | 1.4× | 1.8× | 1.1× | 3.3× | 1.2× | 4.9× |

- All 5 differing inputs for `能工智人+Gemini` were originally written against Gemini 3.8 Flash (Team C).
- In the same separate measurement the six AI leads were retimed: Gemini 97.7×, MiMo 96.3×, Kimi 83.9×, Doubao 7.9×, DeepSeek 7.8×; GLM timed out on one workload. Official values in `../../results/第1轮_速度成绩.md` were 101.4,101.9,88.3,8.1,7.9×: a few percent measurement variation. S1 uses official workload maxima, so may slightly exceed 100.
- The second table gives per-workload speedups.

## Differences from received files

- Extracted from the humans' archives; code/comments unchanged byte for byte; folders renamed as above.
- Four links containing a Windows username in `能工智人+Gemini/HANDOFF.md` were sanitized to `<user>`; otherwise unchanged.
- `CHANGES.md` and several `work/` scripts mentioned by 能工智人's two hand-offs were absent from received archives and are absent here.
- Hand-off speed/memory figures were measured on contestants' machines, separate from these tables. Hand-offs remain in their original language.
- Published with both contestants' consent. Chats with the organizer omitted.
