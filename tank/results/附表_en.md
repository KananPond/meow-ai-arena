[简体中文](附表.md)

## Table 1: second-half version vs. its own first-half version

The corresponding progress-tournament games (8 maps, both seats per map). Score rate is from the second-half version's perspective; 95% intervals calculated by map.

“Indistinguishable maps” have turn-for-turn identical seat-swapped games. With no randomness, this occurs only when both versions make identical moves throughout that map: the games are the same as first-half self-play and reveal no difference.

| Contestant | W | D | L | Score rate | Maps | Indistinguishable maps | Notes |
|---|---:|---:|---:|---|---:|---:|---|
| GPT-6 Astra (Astra) | 11 | 0 | 5 | 68.8% ± 25.8% | 8 | 0 |  |
| Claude Opus 5.5 (Opus) | 11 | 0 | 5 | 68.8% ± 17.9% | 8 | 0 |  |
| Claude Haiku 5.5 (Haiku) | 8 | 0 | 8 | 50.0% ± 0.0% | 8 | 8 | Same program in both halves (no second-half change) |
| GPT-6.1 Sol (Sol) | 12 | 0 | 4 | 75.0% ± 18.5% | 8 | 0 |  |
| Zhipu GLM 5.3 (GLM) | 10 | 3 | 3 | 71.9% ± 17.2% | 8 | 0 |  |
| Xiaomi MiMo V2.6 Pro (MiMo) | 8 | 0 | 8 | 50.0% ± 0.0% | 8 | 6 | Same program in both halves (no second-half change) |
| Doubao-Seed-Evolving (Doubao) | 15 | 0 | 1 | 93.8% ± 12.2% | 8 | 0 |  |
| DeepSeek V4.1 Flash (DeepSeek) | 8 | 0 | 8 | 50.0% ± 18.5% | 8 | 0 |  |
| Muse Wonder Full (Muse) | 8 | 0 | 8 | 50.0% ± 0.0% | 8 | 7 | Same program in both halves (no second-half change) |
| Grok 4.7 (Grok) | 9 | 0 | 7 | 56.2% ± 28.9% | 8 | 0 |  |
| Qwen 3.8 Max (Qwen) | 11 | 0 | 5 | 68.8% ± 17.9% | 8 | 0 |  |
| Kimi K3 (Kimi) | 3 | 6 | 7 | 37.5% ± 16.0% | 8 | 3 |  |
| GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 8 | 0 | 8 | 50.0% ± 18.5% | 8 | 0 |  |
| Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 13 | 0 | 3 | 81.2% ± 17.9% | 8 | 0 |  |

## Table 2: improvement (first-half scale)

Each second-half bot plays the fixed pool of all first-half versions + baseline, with no second-half pairings: 3360 games on 8 new maps. Fit those games and all half-time games together once with Bradley–Terry (baseline = 0), putting both halves on the same scale. Improvement = second-half rating − own first-half rating; its interval uses the direct difference within each of 1000 resamples, sampling maps separately for the two tournaments. “First-half rating” is the combined-fit value; parentheses give the half-time-only value. Small differences are normal.

| Contestant | First-half rating | Second-half rating on first-half scale | Improvement | 95% interval | Against fixed pool: W-D-L | Score rate | Final place (final tournament) |
|---|---|---:|---:|---|---|---:|---:|
| Doubao-Seed-Evolving (Doubao) | -444 (-470) | -49 | +395 | +332 ~ +460 | 106-0-134 | 44.2% | 10 |
| Gemini 4 Argon Contributor (OpenCode Go) (Gemini) | 222 (232) | 392 | +170 | +122 ~ +225 | 206-0-34 | 85.8% | 3 |
| GPT-6 Astra (Astra) | 337 (344) | 485 | +147 | +87 ~ +228 | 219-0-21 | 91.2% | 1 |
| Zhipu GLM 5.3 (GLM) | -427 (-447) | -298 | +129 | +70 ~ +178 | 46-9-185 | 21.0% | 14 |
| Qwen 3.8 Max (Qwen) | -330 (-331) | -244 | +86 | +41 ~ +127 | 61-0-179 | 25.4% | 13 |
| DeepSeek V4.1 Flash (DeepSeek) | -65 (-60) | -8 | +58 | +19 ~ +97 | 116-1-123 | 48.5% | 9 |
| Claude Opus 5.5 (Opus) | 138 (154) | 177 | +39 | -6 ~ +88 | 163-0-77 | 67.9% | 5 |
| GPT-6.1 Sol (Sol) | 258 (274) | 287 | +29 | -50 ~ +111 | 187-0-53 | 77.9% | 4 |
| Grok 4.7 (Grok) | -7 (-8) | 21 | +29 | -29 ~ +78 | 124-0-116 | 51.7% | 8 |
| Muse Wonder Full (Muse) | 76 (74) | 90 | +14 | -28 ~ +55 | 141-1-98 | 59.0% | 6 |
| Kimi K3 (Kimi) | -120 (-126) | -118 | +2 | -67 ~ +72 | 81-16-143 | 37.1% | 11 |
| Claude Haiku 5.5 (Haiku) | -132 (-132) | -141 | -9 | -78 ~ +57 | 80-7-153 | 34.8% | 12 |
| Xiaomi MiMo V2.6 Pro (MiMo) | 37 (39) | 21 | -16 | -51 ~ +21 | 124-0-116 | 51.7% | 7 |
| GPT-6.1 Sol Ultrafast (Codex CLI) (SolUF) | 467 (486) | 405 | -62 | -120 ~ +1 | 208-0-32 | 86.7% | 2 |
