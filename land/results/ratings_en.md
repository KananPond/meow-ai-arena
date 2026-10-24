[中文原文](ratings.md)

# Territory ratings

1v1: 26400 games; 4-player: 3600.0 games (21600 pairwise comparisons). Bradley–Terry maximum likelihood; each format is scaled with anchor-greedy = 0 and its strongest model = 100. Total = average of both format scores, rescaled so the highest total = 100. Intervals: bootstrap 95% (200 resamples).

| Rank | Model | Total | 95% interval | Rank interval | 1v1 score | 1v1 rating (Elo scale) | 4-player score | 4-player rating (Elo scale) |
|---|---|---|---|---|---|---|---|---|
| 1 | sonnet-5@claude-code | 100.0 | 100.0–100.0 | 1–1 | 100.0 (100.0–100.0) | 989 | 100.0 (100.0–100.0) | 290 |
| 2 | gpt-6-astra@codex | 81.7 | 78.5–85.4 | 2–3 | 72.3 (70.2–74.3) | 715 | 91.0 (84.6–98.5) | 264 |
| Anchor | anchor-sparring | 79.8 | 76.8–82.9 | — | 89.3 (87.2–91.3) | 883 | 70.3 (64.5–75.8) | 204 |
| 3 | opus-5.5@claude-code | 79.4 | 76.2–82.6 | 2–3 | 81.2 (79.0–83.2) | 803 | 77.5 (71.0–83.6) | 225 |
| 4 | gpt-6-sol@codex | 39.4 | 36.5–42.7 | 4–4 | 54.0 (51.7–56.4) | 534 | 24.8 (19.7–30.7) | 72 |
| 5 | mimo-2.6-pro@zcode | 32.4 | 29.3–35.4 | 5–5 | 50.4 (48.3–52.3) | 499 | 14.4 (9.4–20.1) | 42 |
| 6 | kimi-k3@workbuddy | 16.7 | 13.6–19.7 | 6–6 | 22.0 (19.9–23.5) | 217 | 11.5 (5.6–17.8) | 33 |
| 7 | glm-5.3@workbuddy | 5.9 | 5.0–6.7 | 7–7 | 11.8 (10.0–13.5) | 116 | 0.0 (0.0–0.0) | -81 |
| Anchor | anchor-greedy | 0.0 | 0.0–0.0 | — | 0.0 (0.0–0.0) | 0 | 0.0 (0.0–0.0) | 0 |
| 8 | mystery-model@zcode | 0.0 | 0.0–0.0 | 8–8 | 0.0 (0.0–0.0) | -80 | 0.0 (0.0–0.0) | -219 |
| 9 | deepseek-4.1f@workbuddy | 0.0 | 0.0–0.0 | 9–9 | 0.0 (0.0–0.0) | -35 | 0.0 (0.0–0.0) | -386 |
| Anchor | anchor-random | 0.0 | 0.0–0.0 | — | 0.0 (0.0–0.0) | -721 | 0.0 (0.0–0.0) | -608 |

## 1v1: per-bot statistics

| bot | Games | Score rate | Mean area | Mean deaths | Mean kills | Timeouts | Forfeits |
|---|---|---|---|---|---|---|---|
| sonnet-5@claude-code#1 | 4400 | 91.6% | 1229 | 0.9 | 2.7 | 0 | 0 |
| anchor-sparring | 4400 | 85.8% | 1158 | 0.7 | 2.7 | 0 | 0 |
| opus-5.5@claude-code#1 | 4400 | 80.7% | 1044 | 0.5 | 3.0 | 0 | 0 |
| gpt-6-astra@codex#1 | 4400 | 74.7% | 973 | 2.3 | 2.8 | 0 | 0 |
| gpt-6-sol@codex#1 | 4400 | 62.2% | 847 | 0.5 | 1.7 | 0 | 0 |
| mimo-2.6-pro@zcode#1 | 4400 | 59.8% | 839 | 3.9 | 2.0 | 0 | 0 |
| kimi-k3@workbuddy#1 | 4400 | 40.8% | 623 | 1.0 | 1.8 | 0 | 0 |
| glm-5.3@workbuddy#1 | 4400 | 33.8% | 586 | 0.8 | 1.4 | 0 | 0 |
| anchor-greedy | 4400 | 25.8% | 501 | 2.9 | 0.9 | 0 | 0 |
| deepseek-4.1f@workbuddy#1 | 4400 | 23.5% | 315 | 7.6 | 2.2 | 0 | 0 |
| mystery-model@zcode#1 | 4400 | 20.6% | 321 | 3.6 | 1.9 | 0 | 0 |
| anchor-random | 4400 | 0.5% | 85 | 5.5 | 0.5 | 0 | 0 |

## 4-player: per-bot statistics

| bot | Games | First-place rate | Mean rank | Mean area | Mean deaths | Mean kills | Timeouts | Forfeits |
|---|---|---|---|---|---|---|---|---|
| sonnet-5@claude-code#1 | 1200 | 56.8% | 1.60 | 812 | 0.8 | 3.4 | 0 | 0 |
| gpt-6-astra@codex#1 | 1200 | 54.5% | 1.65 | 766 | 3.1 | 2.8 | 0 | 0 |
| opus-5.5@claude-code#1 | 1200 | 45.7% | 1.79 | 740 | 0.5 | 4.0 | 0 | 0 |
| anchor-sparring | 1200 | 42.8% | 1.85 | 728 | 0.5 | 3.1 | 0 | 0 |
| gpt-6-sol@codex#1 | 1200 | 26.9% | 2.30 | 582 | 0.5 | 2.7 | 0 | 0 |
| mimo-2.6-pro@zcode#1 | 1200 | 23.9% | 2.37 | 569 | 8.1 | 3.5 | 0 | 0 |
| kimi-k3@workbuddy#1 | 1200 | 19.5% | 2.42 | 542 | 1.9 | 3.3 | 0 | 0 |
| anchor-greedy | 1200 | 14.2% | 2.55 | 510 | 5.3 | 1.8 | 0 | 0 |
| glm-5.3@workbuddy#1 | 1200 | 9.8% | 2.81 | 442 | 0.6 | 2.1 | 0 | 0 |
| mystery-model@zcode#1 | 1200 | 3.4% | 3.19 | 331 | 6.9 | 4.0 | 0 | 0 |
| deepseek-4.1f@workbuddy#1 | 1200 | 2.2% | 3.58 | 215 | 5.5 | 1.7 | 0 | 0 |
| anchor-random | 1200 | 0.0% | 3.87 | 108 | 6.1 | 0.9 | 0 | 0 |
