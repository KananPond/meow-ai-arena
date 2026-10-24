[简体中文](成绩.md)

# Final tournament (second-half versions) · Results

14400 games (60 maps, every pair plays both seats per map). Bradley–Terry fit: win 1, draw 0.5, one virtual draw per pair; baseline = 0, 10× stronger = +400. 95% intervals from 1000 whole-map bootstrap resamples.

Tie rule: sort by rating and inspect the 95% interval of each adjacent pair's rating difference (computed directly within each resample). If it includes 0, the games cannot distinguish the pair and they are marked tied; ties may form a chain. Individual intervals show uncertainty relative to baseline and do not determine ties: overlapping individual intervals do not establish a tie.

| Place | Contestant | Rating | 95% interval | W | D | L | Score rate | Forfeits | Games with faults | Mean turns | Per-turn CPU ms: mean / max |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---|
| 1 | GPT-6 Astra (Astra) | 507 | 468 ~ 550 | 1556 | 4 | 240 | 86.6% | 0 | 0 | 54 | 8.2 / 20.9 |
| — | sparring (organizer reference) | 489 | 460 ~ 521 | 1534 | 4 | 262 | 85.3% | 0 | 0 | 59 | 10.1 / 20.5 |
| 2 | GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 467 | 432 ~ 503 | 1509 | 0 | 291 | 83.8% | 0 | 0 | 54 | 15.7 / 41.4 |
| 3 | Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 355 | 325 ~ 389 | 1351 | 3 | 446 | 75.1% | 0 | 0 | 56 | 20.3 / 39.3 |
| 4 | GPT-6.1 Sol (Sol) | 319 | 286 ~ 353 | 1295 | 1 | 504 | 72.0% | 0 | 0 | 59 | 4.0 / 12.4 |
| 5 | Claude Opus 5.5 (Opus) | 259 | 224 ~ 295 | 1196 | 2 | 602 | 66.5% | 0 | 0 | 57 | 0.5 / 2.0 |
| 6 (6–10 tied) | Muse Wonder Full (Muse) | 55 | 22 ~ 88 | 823 | 6 | 971 | 45.9% | 0 | 0 | 81 | 5.1 / 38.7 |
| 7 (6–10 tied) | Xiaomi MiMo V2.6 Pro (MiMo) | 29 | 2 ~ 59 | 773 | 4 | 1023 | 43.1% | 0 | 0 | 63 | 27.1 / 36.3 |
| 8 (6–10 tied) | Grok 4.7 (Grok) | 26 | -3 ~ 55 | 768 | 1 | 1031 | 42.7% | 0 | 0 | 79 | 14.9 / 42.9 |
| — | baseline (baseline) | 0 | — | 718 | 3 | 1079 | 40.0% | 0 | 0 | 66 | 0.1 / 0.4 |
| 9 (6–10 tied) | DeepSeek V4.1 Flash (DeepSeek) | -6 | -37 ~ 28 | 706 | 3 | 1091 | 39.3% | 0 | 0 | 79 | 2.9 / 18.9 |
| 10 (6–10 tied) | Doubao-Seed-Evolving (Doubao) | -31 | -64 ~ 0 | 655 | 12 | 1133 | 36.7% | 0 | 0 | 76 | 0.1 / 0.4 |
| 11 (11–12 tied) | Kimi K3 (Kimi) | -129 | -162 ~ -97 | 466 | 30 | 1304 | 26.7% | 0 | 0 | 83 | 5.8 / 42.2 |
| 12 (11–12 tied) | Claude Haiku 5.5 (Haiku) | -149 | -182 ~ -117 | 438 | 18 | 1344 | 24.8% | 0 | 0 | 87 | 5.9 / 21.7 |
| 13 | Qwen 3.8 Max (Qwen) | -205 | -239 ~ -171 | 356 | 2 | 1442 | 19.8% | 0 | 0 | 85 | 0.1 / 0.5 |
| 14 | Zhipu GLM 5.3 (GLM) | -315 | -362 ~ -271 | 203 | 13 | 1584 | 11.6% | 0 | 0 | 84 | 5.6 / 19.0 |

Overall: 70.2 turns/game; draws 0.4% (53 games); seat 0 won 7290, seat 1 won 7057; 0 fault endings. Endings: base destroyed 13676, turn cap/base HP 316, turn cap/kills 184, both bases destroyed same turn 88, turn cap/stars 62, turn cap/draw 40, both bases destroyed same turn 21, both bases destroyed same turn 13.

“Forfeits” counts crashes, exits, or 3 consecutive overruns/illegal outputs that lost the game; “Games with faults” also includes overruns/illegal outputs without a forfeit. Usage is CPU milliseconds.

## Adjacent-place differences

| Adjacent contestants | Rating difference | 95% interval of difference | Decision |
|---|---:|---|---|
| Place 1 GPT-6 Astra (Astra) − Place 2 GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 40 | 1 ~ 84 | Distinguishable |
| Place 2 GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) − Place 3 Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 111 | 78 ~ 144 | Distinguishable |
| Place 3 Gemini 4 Argon Contributor (OpenCode Go) (Gemini) − Place 4 GPT-6.1 Sol (Sol) | 37 | 8 ~ 66 | Distinguishable |
| Place 4 GPT-6.1 Sol (Sol) − Place 5 Claude Opus 5.5 (Opus) | 60 | 30 ~ 90 | Distinguishable |
| Place 5 Claude Opus 5.5 (Opus) − Place 6 Muse Wonder Full (Muse) | 203 | 164 ~ 248 | Distinguishable |
| Place 6 Muse Wonder Full (Muse) − Place 7 Xiaomi MiMo V2.6 Pro (MiMo) | 27 | -2 ~ 55 | Tied (interval includes 0) |
| Place 7 Xiaomi MiMo V2.6 Pro (MiMo) − Place 8 Grok 4.7 (Grok) | 3 | -24 ~ 30 | Tied (interval includes 0) |
| Place 8 Grok 4.7 (Grok) − Place 9 DeepSeek V4.1 Flash (DeepSeek) | 32 | 0 ~ 65 | Tied (interval includes 0) |
| Place 9 DeepSeek V4.1 Flash (DeepSeek) − Place 10 Doubao-Seed-Evolving (Doubao) | 24 | -13 ~ 62 | Tied (interval includes 0) |
| Place 10 Doubao-Seed-Evolving (Doubao) − Place 11 Kimi K3 (Kimi) | 99 | 62 ~ 133 | Distinguishable |
| Place 11 Kimi K3 (Kimi) − Place 12 Claude Haiku 5.5 (Haiku) | 20 | -7 ~ 47 | Tied (interval includes 0) |
| Place 12 Claude Haiku 5.5 (Haiku) − Place 13 Qwen 3.8 Max (Qwen) | 56 | 17 ~ 94 | Distinguishable |
| Place 13 Qwen 3.8 Max (Qwen) − Place 14 Zhipu GLM 5.3 (GLM) | 109 | 75 ~ 146 | Distinguishable |

## Pairwise W-D-L (row program vs column program)

| Row vs column | Astra | sparring | SolUF | Gemini | Sol | Opus | Muse | MiMo | Grok | baseline | DeepSeek | Doubao | Kimi | Haiku | Qwen | GLM |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| GPT-6 Astra (Astra) | · | 60-3-57 | 75-0-45 | 82-0-38 | 102-0-18 | 85-0-35 | 116-0-4 | 110-1-9 | 119-0-1 | 110-0-10 | 118-0-2 | 109-0-11 | 111-0-9 | 119-0-1 | 120-0-0 | 120-0-0 |
| sparring (organizer reference) | 57-3-60 | · | 66-0-54 | 70-1-49 | 87-0-33 | 93-0-27 | 111-0-9 | 111-0-9 | 118-0-2 | 118-0-2 | 112-0-8 | 117-0-3 | 119-0-1 | 117-0-3 | 120-0-0 | 118-0-2 |
| GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 45-0-75 | 54-0-66 | · | 88-0-32 | 96-0-24 | 80-0-40 | 114-0-6 | 112-0-8 | 113-0-7 | 114-0-6 | 108-0-12 | 114-0-6 | 116-0-4 | 118-0-2 | 119-0-1 | 118-0-2 |
| Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 38-0-82 | 49-1-70 | 32-0-88 | · | 73-0-47 | 85-1-34 | 99-0-21 | 105-0-15 | 114-0-6 | 105-0-15 | 95-0-25 | 102-1-17 | 106-0-14 | 115-0-5 | 115-0-5 | 118-0-2 |
| GPT-6.1 Sol (Sol) | 18-0-102 | 33-0-87 | 24-0-96 | 47-0-73 | · | 77-0-43 | 94-0-26 | 101-0-19 | 113-0-7 | 111-0-9 | 90-0-30 | 118-0-2 | 114-1-5 | 118-0-2 | 117-0-3 | 120-0-0 |
| Claude Opus 5.5 (Opus) | 35-0-85 | 27-0-93 | 40-0-80 | 34-1-85 | 43-0-77 | · | 95-0-25 | 69-0-51 | 95-0-25 | 114-0-6 | 90-0-30 | 111-0-9 | 102-1-17 | 107-0-13 | 115-0-5 | 119-0-1 |
| Muse Wonder Full (Muse) | 4-0-116 | 9-0-111 | 6-0-114 | 21-0-99 | 26-0-94 | 25-0-95 | · | 68-0-52 | 84-0-36 | 58-1-61 | 82-0-38 | 55-0-65 | 88-1-31 | 88-2-30 | 99-0-21 | 110-2-8 |
| Xiaomi MiMo V2.6 Pro (MiMo) | 9-1-110 | 9-0-111 | 8-0-112 | 15-0-105 | 19-0-101 | 51-0-69 | 52-0-68 | · | 58-1-61 | 48-0-72 | 61-0-59 | 58-1-61 | 80-1-39 | 101-0-19 | 107-0-13 | 97-0-23 |
| Grok 4.7 (Grok) | 1-0-119 | 2-0-118 | 7-0-113 | 6-0-114 | 7-0-113 | 25-0-95 | 36-0-84 | 61-1-58 | · | 65-0-55 | 52-0-68 | 99-0-21 | 78-0-42 | 101-0-19 | 113-0-7 | 115-0-5 |
| baseline (baseline) | 10-0-110 | 2-0-118 | 6-0-114 | 15-0-105 | 9-0-111 | 6-0-114 | 61-1-58 | 72-0-48 | 55-0-65 | · | 59-0-61 | 54-1-65 | 88-1-31 | 82-0-38 | 89-0-31 | 110-0-10 |
| DeepSeek V4.1 Flash (DeepSeek) | 2-0-118 | 8-0-112 | 12-0-108 | 25-0-95 | 30-0-90 | 30-0-90 | 38-0-82 | 59-0-61 | 68-0-52 | 61-0-59 | · | 53-1-66 | 80-1-39 | 72-1-47 | 73-0-47 | 95-0-25 |
| Doubao-Seed-Evolving (Doubao) | 11-0-109 | 3-0-117 | 6-0-114 | 17-1-102 | 2-0-118 | 9-0-111 | 65-0-55 | 61-1-58 | 21-0-99 | 65-1-54 | 66-1-53 | · | 61-4-55 | 82-2-36 | 80-1-39 | 106-1-13 |
| Kimi K3 (Kimi) | 9-0-111 | 1-0-119 | 4-0-116 | 14-0-106 | 5-1-114 | 17-1-102 | 31-1-88 | 39-1-80 | 42-0-78 | 31-1-88 | 39-1-80 | 55-4-61 | · | 54-11-55 | 62-0-58 | 63-9-48 |
| Claude Haiku 5.5 (Haiku) | 1-0-119 | 3-0-117 | 2-0-118 | 5-0-115 | 2-0-118 | 13-0-107 | 30-2-88 | 19-0-101 | 19-0-101 | 38-0-82 | 47-1-72 | 36-2-82 | 55-11-54 | · | 81-1-38 | 87-1-32 |
| Qwen 3.8 Max (Qwen) | 0-0-120 | 0-0-120 | 1-0-119 | 5-0-115 | 3-0-117 | 5-0-115 | 21-0-99 | 13-0-107 | 7-0-113 | 31-0-89 | 47-0-73 | 39-1-80 | 58-0-62 | 38-1-81 | · | 88-0-32 |
| Zhipu GLM 5.3 (GLM) | 0-0-120 | 2-0-118 | 2-0-118 | 2-0-118 | 0-0-120 | 1-0-119 | 8-2-110 | 23-0-97 | 5-0-115 | 10-0-110 | 25-0-95 | 13-1-106 | 48-9-63 | 32-1-87 | 32-0-88 | · |

## Notes

- Batches with inconsistent before/after speed checks: [5, 16, 29, 34, 53] (see `测速.jsonl`). These results still count.
