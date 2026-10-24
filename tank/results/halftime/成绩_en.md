[简体中文](成绩.md)

# Half-time tournament (first-half versions) · Results

10080 games (42 maps, every pair plays both seats per map). Bradley–Terry fit: win 1, draw 0.5, one virtual draw per pair; baseline = 0, 10× stronger = +400. 95% intervals from 1000 whole-map bootstrap resamples.

Tie rule: sort by rating and inspect the 95% interval of each adjacent pair's rating difference (computed directly within each resample). If it includes 0, the games cannot distinguish the pair and they are marked tied; ties may form a chain. Individual intervals show uncertainty relative to baseline and do not determine ties: overlapping individual intervals do not establish a tie.

| Place | Contestant | Rating | 95% interval | W | D | L | Score rate | Forfeits | Games with faults | Mean turns | Per-turn CPU ms: mean / max |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---|
| — | sparring (organizer reference) | 558 | 508 ~ 613 | 1160 | 0 | 100 | 92.1% | 0 | 0 | 61 | 10.0 / 22.4 |
| 1 | GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 486 | 443 ~ 533 | 1111 | 0 | 149 | 88.2% | 0 | 0 | 54 | 18.0 / 45.2 |
| 2 | GPT-6 Astra (Astra) | 344 | 294 ~ 393 | 989 | 1 | 270 | 78.5% | 0 | 0 | 58 | 8.5 / 23.7 |
| 3 | GPT-6.1 Sol (Sol) | 274 | 234 ~ 315 | 916 | 0 | 344 | 72.7% | 0 | 0 | 65 | 3.9 / 11.7 |
| 4 | Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 232 | 196 ~ 269 | 870 | 0 | 390 | 69.0% | 0 | 0 | 59 | 12.9 / 28.9 |
| 5 | Claude Opus 5.5 (Opus) | 154 | 115 ~ 192 | 775 | 3 | 482 | 61.6% | 0 | 0 | 66 | 0.5 / 1.8 |
| 6 (6–7 tied) | Muse Wonder Full (Muse) | 74 | 32 ~ 115 | 665 | 20 | 575 | 53.6% | 0 | 0 | 91 | 5.1 / 44.2 |
| 7 (6–7 tied) | Xiaomi MiMo V2.6 Pro (MiMo) | 39 | 2 ~ 70 | 630 | 1 | 629 | 50.0% | 0 | 0 | 67 | 26.6 / 36.6 |
| — | baseline (baseline) | 0 | — | 580 | 1 | 679 | 46.1% | 0 | 0 | 67 | 0.1 / 0.4 |
| 8 | Grok 4.7 (Grok) | -8 | -49 ~ 32 | 568 | 4 | 688 | 45.2% | 0 | 0 | 106 | 9.6 / 41.2 |
| 9 | DeepSeek V4.1 Flash (DeepSeek) | -60 | -109 ~ -15 | 503 | 4 | 753 | 40.1% | 0 | 0 | 89 | 1.7 / 14.1 |
| 10 (10–11 tied) | Kimi K3 (Kimi) | -126 | -179 ~ -78 | 412 | 25 | 823 | 33.7% | 0 | 0 | 93 | 4.4 / 42.1 |
| 11 (10–11 tied) | Claude Haiku 5.5 (Haiku) | -132 | -163 ~ -101 | 415 | 5 | 840 | 33.1% | 0 | 0 | 95 | 5.7 / 21.5 |
| 12 | Qwen 3.8 Max (Qwen) | -331 | -366 ~ -296 | 215 | 0 | 1045 | 17.1% | 0 | 0 | 82 | 26.4 / 47.3 |
| 13 (13–14 tied) | Zhipu GLM 5.3 (GLM) | -447 | -495 ~ -401 | 96 | 62 | 1102 | 10.1% | 0 | 0 | 97 | 8.9 / 33.6 |
| 14 (13–14 tied) | Doubao-Seed-Evolving (Doubao) | -470 | -508 ~ -432 | 71 | 82 | 1107 | 8.9% | 2 | 10 | 116 | 26.0 / 62.4 |

Overall: 79.0 turns/game; draws 1.0% (104 games); seat 0 won 5003, seat 1 won 4973; 2 fault endings. Endings: base destroyed 9370, turn cap/base HP 266, turn cap/kills 154, turn cap/stars 128, turn cap/draw 103, both bases destroyed same turn 47, both bases destroyed same turn 9, fault forfeits 2, both bases destroyed same turn 1.

“Forfeits” counts crashes, exits, or 3 consecutive overruns/illegal outputs that lost the game; “Games with faults” also includes overruns/illegal outputs without a forfeit. Usage is CPU milliseconds.

## Adjacent-place differences

| Adjacent contestants | Rating difference | 95% interval of difference | Decision |
|---|---:|---|---|
| Place 1 GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) − Place 2 GPT-6 Astra (Astra) | 141 | 98 ~ 189 | Distinguishable |
| Place 2 GPT-6 Astra (Astra) − Place 3 GPT-6.1 Sol (Sol) | 71 | 32 ~ 108 | Distinguishable |
| Place 3 GPT-6.1 Sol (Sol) − Place 4 Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 41 | 2 ~ 78 | Distinguishable |
| Place 4 Gemini 4 Argon Contributor (OpenCode Go) (Gemini) − Place 5 Claude Opus 5.5 (Opus) | 78 | 50 ~ 108 | Distinguishable |
| Place 5 Claude Opus 5.5 (Opus) − Place 6 Muse Wonder Full (Muse) | 80 | 44 ~ 118 | Distinguishable |
| Place 6 Muse Wonder Full (Muse) − Place 7 Xiaomi MiMo V2.6 Pro (MiMo) | 35 | -6 ~ 73 | Tied (interval includes 0) |
| Place 7 Xiaomi MiMo V2.6 Pro (MiMo) − Place 8 Grok 4.7 (Grok) | 47 | 9 ~ 82 | Distinguishable |
| Place 8 Grok 4.7 (Grok) − Place 9 DeepSeek V4.1 Flash (DeepSeek) | 52 | 23 ~ 86 | Distinguishable |
| Place 9 DeepSeek V4.1 Flash (DeepSeek) − Place 10 Kimi K3 (Kimi) | 66 | 18 ~ 117 | Distinguishable |
| Place 10 Kimi K3 (Kimi) − Place 11 Claude Haiku 5.5 (Haiku) | 6 | -42 ~ 57 | Tied (interval includes 0) |
| Place 11 Claude Haiku 5.5 (Haiku) − Place 12 Qwen 3.8 Max (Qwen) | 199 | 160 ~ 239 | Distinguishable |
| Place 12 Qwen 3.8 Max (Qwen) − Place 13 Zhipu GLM 5.3 (GLM) | 115 | 67 ~ 169 | Distinguishable |
| Place 13 Zhipu GLM 5.3 (GLM) − Place 14 Doubao-Seed-Evolving (Doubao) | 23 | -17 ~ 60 | Tied (interval includes 0) |

## Pairwise W-D-L (row program vs column program)

| Row vs column | sparring | SolUF | Astra | Sol | Gemini | Opus | Muse | MiMo | baseline | Grok | DeepSeek | Kimi | Haiku | Qwen | GLM | Doubao |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| sparring (organizer reference) | · | 54-0-30 | 58-0-26 | 70-0-14 | 74-0-10 | 78-0-6 | 82-0-2 | 77-0-7 | 82-0-2 | 82-0-2 | 83-0-1 | 84-0-0 | 84-0-0 | 84-0-0 | 84-0-0 | 84-0-0 |
| GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 30-0-54 | · | 60-0-24 | 75-0-9 | 67-0-17 | 63-0-21 | 81-0-3 | 80-0-4 | 80-0-4 | 80-0-4 | 78-0-6 | 81-0-3 | 84-0-0 | 84-0-0 | 84-0-0 | 84-0-0 |
| GPT-6 Astra (Astra) | 26-0-58 | 24-0-60 | · | 55-0-29 | 53-0-31 | 56-0-28 | 72-0-12 | 74-1-9 | 64-0-20 | 79-0-5 | 74-0-10 | 76-0-8 | 84-0-0 | 84-0-0 | 84-0-0 | 84-0-0 |
| GPT-6.1 Sol (Sol) | 14-0-70 | 9-0-75 | 29-0-55 | · | 53-0-31 | 59-0-25 | 61-0-23 | 66-0-18 | 76-0-8 | 76-0-8 | 63-0-21 | 78-0-6 | 80-0-4 | 84-0-0 | 84-0-0 | 84-0-0 |
| Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 10-0-74 | 17-0-67 | 31-0-53 | 31-0-53 | · | 60-0-24 | 67-0-17 | 58-0-26 | 62-0-22 | 72-0-12 | 70-0-14 | 69-0-15 | 74-0-10 | 82-0-2 | 83-0-1 | 84-0-0 |
| Claude Opus 5.5 (Opus) | 6-0-78 | 21-0-63 | 28-0-56 | 25-0-59 | 24-0-60 | · | 51-1-32 | 37-0-47 | 77-0-7 | 60-0-24 | 64-0-20 | 66-0-18 | 66-1-17 | 84-0-0 | 84-0-0 | 82-1-1 |
| Muse Wonder Full (Muse) | 2-0-82 | 3-0-81 | 12-0-72 | 23-0-61 | 17-0-67 | 32-1-51 | · | 60-0-24 | 45-0-39 | 66-0-18 | 62-1-21 | 55-3-26 | 69-1-14 | 76-0-8 | 73-3-8 | 70-11-3 |
| Xiaomi MiMo V2.6 Pro (MiMo) | 7-0-77 | 4-0-80 | 9-1-74 | 18-0-66 | 26-0-58 | 47-0-37 | 24-0-60 | · | 35-0-49 | 41-0-43 | 52-0-32 | 57-0-27 | 72-0-12 | 78-0-6 | 79-0-5 | 81-0-3 |
| baseline (baseline) | 2-0-82 | 4-0-80 | 20-0-64 | 8-0-76 | 22-0-62 | 7-0-77 | 39-0-45 | 49-0-35 | · | 40-0-44 | 43-0-41 | 60-0-24 | 51-1-32 | 75-0-9 | 81-0-3 | 79-0-5 |
| Grok 4.7 (Grok) | 2-0-82 | 4-0-80 | 5-0-79 | 8-0-76 | 12-0-72 | 24-0-60 | 18-0-66 | 43-0-41 | 44-0-40 | · | 44-1-39 | 57-0-27 | 66-0-18 | 79-0-5 | 82-1-1 | 80-2-2 |
| DeepSeek V4.1 Flash (DeepSeek) | 1-0-83 | 6-0-78 | 10-0-74 | 21-0-63 | 14-0-70 | 20-0-64 | 21-1-62 | 32-0-52 | 41-0-43 | 39-1-44 | · | 52-2-30 | 38-0-46 | 71-0-13 | 68-0-16 | 69-0-15 |
| Kimi K3 (Kimi) | 0-0-84 | 3-0-81 | 8-0-76 | 6-0-78 | 15-0-69 | 18-0-66 | 26-3-55 | 27-0-57 | 24-0-60 | 27-0-57 | 30-2-52 | · | 39-1-44 | 65-0-19 | 67-4-13 | 57-15-12 |
| Claude Haiku 5.5 (Haiku) | 0-0-84 | 0-0-84 | 0-0-84 | 4-0-80 | 10-0-74 | 17-1-66 | 14-1-69 | 12-0-72 | 32-1-51 | 18-0-66 | 46-0-38 | 44-1-39 | · | 76-0-8 | 66-1-17 | 76-0-8 |
| Qwen 3.8 Max (Qwen) | 0-0-84 | 0-0-84 | 0-0-84 | 0-0-84 | 2-0-82 | 0-0-84 | 8-0-76 | 6-0-78 | 9-0-75 | 5-0-79 | 13-0-71 | 19-0-65 | 8-0-76 | · | 73-0-11 | 72-0-12 |
| Zhipu GLM 5.3 (GLM) | 0-0-84 | 0-0-84 | 0-0-84 | 0-0-84 | 1-0-83 | 0-0-84 | 8-3-73 | 5-0-79 | 3-0-81 | 1-1-82 | 16-0-68 | 13-4-67 | 17-1-66 | 11-0-73 | · | 21-53-10 |
| Doubao-Seed-Evolving (Doubao) | 0-0-84 | 0-0-84 | 0-0-84 | 0-0-84 | 0-0-84 | 1-1-82 | 3-11-70 | 3-0-81 | 5-0-79 | 2-2-80 | 15-0-69 | 12-15-57 | 8-0-76 | 12-0-72 | 10-53-21 | · |

## Notes

- Batches with inconsistent before/after speed checks: [0, 1, 2, 4, 22] (see `测速.jsonl`). These results still count.
