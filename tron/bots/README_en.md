[简体中文](README.md)

# Sealed code from each model

These are the models' original contest submissions, unmodified by the organizer. Their sha256 hashes match the frozen `../results/清单.json`. greedy and random are in `../workspace/bots/`; sources for both sparring bots are in `../sparring/`.

## Official contestants

Places and totals are from the official board (Doubao anchored at 0).

| File | Model | Short name | Official place | Total | Lines |
|---|---|---|---|---:|---:|
| `mistral-large-4.cpp` | Mistral Large 4 | Mistral | 1st | 102 | 431 |
| `longcat-2.0.cpp` | Meituan LongCat-2.0 | LongCat | 2nd–3rd | 10 | 276 |
| `doubao-seed-evolving.cpp` | Doubao-Seed-Evolving | Doubao | 2nd–4th | 0 (anchor) | 776 |
| `step-5-preview.cpp` | StepFun Step 5 preview | Step | 3rd–4th | −25 | 565 |
| `openpangu-2.0-pro.cpp` | openPangu-2.0-Pro | Pangu | 5th | −208 | 218 |
| `hunyuan-hy4-preview.cpp` | Tencent Hunyuan Hy4 preview | Hunyuan | 6th | −240 | 504 |
| `qwen3.8-27b.cpp` | Qwen3.8-27B | Qwen27B | 7th–9th | −290 | 746 |
| `mercury-3.cpp` | Inception Mercury 3 | Mercury | 7th–9th | −297 | 371 |
| `ernie-5.1.cpp` | ERNIE 5.1 | Wenxin | 7th–9th | −304 | 131 |
| `dots3-note-preview.cpp` | Xiaohongshu dots3-note preview | Dots | 10th | −440 | 115 |

| File | Model | Short name | Strong-board place | Total | Lines |
|---|---|---|---|---:|---:|
| `reference/gpt-6-astra-ultra-fast.cpp` | GPT-6 Astra (ultra reasoning + fast tier, Codex) | AstraUF | 1st | 313 | 253 |
| `reference/deepseek-v4.1-flash-dsh.cpp` | DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) | DSNew | 2nd–3rd | 128 | 514 |

The last two entries are reference contestants in `reference/`, on the strong-opponent board only. Short names are used in result tables, replays and configs.

Doubao's session changed its file after sealing. The sealed version governs under the rules and is the version included here.

Use these as opponents with `../workspace/arena.py`; for example, from `workspace/`, run `python arena.py play submission/bot.cpp ../bots/mistral-large-4.cpp --seed 5`, or `python arena.py run --opp ../bots/step-5-preview.cpp,../bots/hunyuan-hy4-preview.cpp`.
