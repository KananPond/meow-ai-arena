[简体中文](团队成绩_初测.md)

# Team relay competition results

## Team total

| Rank | Team | Lead programmer | Tester | Total | Lead speed | Counterexample resistance | Lead blind review | Counterexamples | Takeover correctness | Takeover speed | Final blind review |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Team F | MiMo V2.6 Pro | GPT-6 Astra | **90.9** | 99 | 59 (found by 2 testers) | 70 | 100 (6 defects) | 100 (Fixed 2/2) | 95 | 78 |
| 2 | Team C | Gemini 3.8 Flash | GPT-6.1 Sol | **77.9** | 99 | 0 (found by 5 testers) | 48 | 79 (4 defects) | 100 (Fixed 7/7) | 99 | 52 |
| 3 | Team A | Kimi K3 | Sonnet 5.5 | **71.4** | 96 | 96 (found by 0 testers) | 57 | 22 (2 defects) | 100 (Fixed 0/0) | 71 | 60 |
| 4 | Team D | DeepSeek V4.1 Flash | Opus 5.5 | **70.3** | 45 | 27 (found by 2 testers) | 45 | 60 (4 defects) | 100 (Fixed 2/2) | 100 | 65 |
| 5 | Team B | 豆包 Seed 2.1 Turbo | Fable 5.1 | **56.4** | 46 | 0 (found by 5 testers) | 35 | 35 (2 defects) | 100 (Fixed 5/5) | 72 | 63 |
| 6 | Team E | GLM 5.3 | Grok 4.7 | **20.4** | 39 | 8 (found by 4 testers) | 48 | 33 (2 defects) | 0 (Fixed 0/4) | 0 | 48 |

Weights: lead speed 15, counterexample resistance 10, lead blind review 5, counterexample discovery 20, takeover correctness 20, takeover speed 20, final blind review 10.

## Lead programmer leaderboard (lower tier)

| Rank | Lead programmer | Team | Score | Speed | Counterexample resistance | Blind review |
|---|---|---|---|---|---|---|
| 1 | Kimi K3 | Team A | **89.4** | 96 | 96 | 57 |
| 2 | MiMo V2.6 Pro | Team F | **80.8** | 99 | 59 | 70 |
| 3 | Gemini 3.8 Flash | Team C | **57.6** | 99 | 0 | 48 |
| 4 | DeepSeek V4.1 Flash | Team D | **39.3** | 45 | 27 | 45 |
| 5 | GLM 5.3 | Team E | **30.2** | 39 | 8 | 48 |
| 6 | 豆包 Seed 2.1 Turbo | Team B | **28.7** | 46 | 0 | 35 |

Score = (15 × speed + 10 × resistance + 5 × blind review) / 30.

## Tester leaderboard (higher tier)

| Rank | Tester | Team | Score | Counterexamples | Takeover correctness | Takeover speed | Added value | Final blind review |
|---|---|---|---|---|---|---|---|---|
| 1 | GPT-6 Astra | Team F | **86.5** | 100 | 100 | 95 | 55 (mean 34.87×) | 78 |
| 2 | Opus 5.5 | Team D | **84.8** | 60 | 100 | 100 | 100 (mean 639.7×) | 65 |
| 3 | GPT-6.1 Sol | Team C | **78.8** | 79 | 100 | 99 | 58 (mean 44.23×) | 52 |
| 4 | Fable 5.1 | Team B | **64.6** | 35 | 100 | 72 | 59 (mean 42.63×) | 63 |
| 5 | Sonnet 5.5 | Team A | **54.6** | 22 | 100 | 71 | 30 (mean 6.92×) | 60 |
| 6 | Grok 4.7 | Team E | **15.4** | 33 | 0 | 0 | 0 (mean —×) | 48 |

Score = 0.25 counterexamples + 0.20 takeover correctness + 0.20 takeover speed + 0.20 added value + 0.15 final blind review. Added value measures final speedup over the original lead submission on the same data (parentheses give the geometric mean across workloads).
