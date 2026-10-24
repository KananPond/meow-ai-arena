[简体中文](README.md)

# Episode 7 — Legacy Code Optimization: submissions

Submissions for the second event of Episode 7, the regional championship. Each model independently attempted the Episode 5 Round 1 lead programmer task with the same `../../workspace/` package and no teammate. Each folder is the collected `src/`, copied byte for byte without organizer edits. Compile with `g++ -O2 -std=c++20 *.cpp` (add `-static` on Windows); as in the evaluation, only top-level `.cpp` files are compiled.

The organizer's internal folder names included an event prefix (for example, `…_Step`). Result files in `../../results/ep7/` use `ep7_<short-name>`; the table maps those names to folders.

| Folder | Model | Name in result files | Rank | Correct out of 8 | Geometric mean speedup | Handoff document |
|---|---|---|---:|---|---:|---|
| `step-5-preview/` | 阶跃 Step 5 preview | `ep7_Step` | 1 (tied) | 8/8 | 139.4× | Yes |
| `hunyuan-hy4-preview/` | 腾讯混元 Hy4 preview | `ep7_Hunyuan` | 1 (tied) | 8/8 | 138.2× | Yes |
| `qwen3.8-27b/` | 通义千问 Qwen3.8-27B | `ep7_Qwen27B` | 3 (tied) | 8/8 | 8.7× | No |
| `mercury-3/` | Inception Mercury 3 (diffusion model) | `ep7_Mercury` | 3 (tied) | 8/8 | 8.6× | No (the post-deadline version was removed; see `../../CONDITIONS_en.md`) |
| （`../round1/doubao-seed-2.1-turbo/`） | 豆包 Seed 2.1 Turbo | Original Episode 5 lead submission | 5 | 8/8 | 8.4× | Yes (Episode 5 code; earlier result reused) |
| `ernie-5.1/` | 文心 ERNIE 5.1 | `ep7_Wenxin` | 6 | 8/8 | 1.3× | Yes |
| `mistral-large-4/` | Mistral Large 4 | `ep7_Mistral` | 7 (tied) | 8/8 | About 1× (identical to baseline) | No (the post-deadline version was removed) |
| `dots3-note-preview/` | 小红书 dots3-note preview | `ep7_Dots` | 7 (tied) | 8/8 | About 1× (identical to baseline) | No |
| `openpangu-2.0-pro/` | 盘古 openPangu-2.0-Pro | `ep7_Pangu` | 9 | 1/8 (7 other workloads timed out) | Over 2× slower than baseline | No |
| `longcat-2.0/` | 美团 LongCat-2.0 | `ep7_LongCat` | 10 | 0/8 (compilation failed) | — | No |
| `sonnet-5.5/` | Claude Sonnet 5.5 | `ep7_Sonnet` | Reference; unranked | 8/8 | 169.7× | Yes |
| `opus-5.5/` | Claude Opus 5.5 | `ep7_Opus` | Reference; unranked | 8/8 | 101.0× | Yes |
| `haiku-5.5/` | Claude Haiku 5.5 | `ep7_Haiku` | Reference; unranked | 8/8 | 98.1× | Yes |

- All 5 files in `mistral-large-4/` and `dots3-note-preview/` match `../../workspace/baseline/` byte for byte (identical SHA-256). They were treated as baseline-identical without separate runs.
- `longcat-2.0/` fails to compile with the command above: line 70 of `sim.cpp` uses `__m128i` without the required header. The original compiler output is in `../../results/ep7/编译错误.json`.
- Doubao's rank reuses its Episode 5 lead programmer result; see `../round1/doubao-seed-2.1-turbo/`. At the user's decision, its new submission for this event was neither timed nor included here.
- The three Claude models used the same package, prompt, and time limit. They are reference entries and do not participate in the rankings.
- See [the Episode 7 section](../../README_en.md) for ranks, counterexamples, and timing details.
