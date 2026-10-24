[简体中文](README.md)

# Submitted code and counterexamples

These are the exact files submitted during the competition, copied byte for byte without organizer edits, including inconsistent line endings. Team A–F refer to the following pairings.

| Team | Lead programmer | Tester | Team total | Team rank |
|---|---|---|---:|---:|
| Team A | Kimi K3 | Sonnet 5.5 | 71.4 | 3 |
| Team B | 豆包 Seed 2.1 Turbo | Fable 5.1 | 56.4 | 5 |
| Team C | Gemini 3.8 Flash | GPT-6.1 Sol | 77.9 | 2 |
| Team D | DeepSeek V4.1 Flash | Opus 5.5 | 70.3 | 4 |
| Team E | GLM 5.3 | Grok 4.7 | 20.4 | 6 |
| Team F | MiMo V2.6 Pro | GPT-6 Astra | 90.8 | 1 |

## `round1/`: lead programmers' Round 1 submissions

Each folder is the `src/` directory collected at submission, including `HANDOFF.md`. Compile with `g++ -O2 -std=c++20 *.cpp` (add `-static` on Windows).

| Folder | Model | Tool | Lead programmer rank | Score | Mean hidden-workload speedup | Testers who found counterexamples |
|---|---|---|---:|---:|---:|---:|
| `kimi-k3/` | Kimi K3 | Kimi Code | 1 | 89.4 | 88× | 0 |
| `mimo-v2.6-pro/` | MiMo V2.6 Pro | OpenCode | 2 | 80.8 | 102× | 2 |
| `gemini-3.8-flash/` | Gemini 3.8 Flash | Antigravity CLI | 3 | 57.6 | 101× | 5 |
| `deepseek-v4.1-flash/` | DeepSeek V4.1 Flash | dsh | 4 | 39.3 | 8× | 2 |
| `glm-5.3/` | GLM 5.3 | ZCode | 5 | 30.2 | One timeout | 4 |
| `doubao-seed-2.1-turbo/` | 豆包 Seed 2.1 Turbo | OpenCode | 6 | 28.7 | 8× | 5 |

These programs accept only the old input format, as used in `../workspace/workloads/`.

## `round2/`: testers' Round 2 counterexamples

Folders are `<tester>/<lead-programmer>/`, containing submitted `.in` cases and the tester's `说明.txt`. `把用例放在这里.txt` is the package's placeholder. A directory without `.in` files means no case was submitted for that implementation.

| Folder | Model | Tool | Cases submitted | Upheld | Defects found | Counterexample score |
|---|---|---|---:|---:|---|---:|
| `gpt-6-astra/` | GPT-6 Astra | Codex | 6 | 6 | Doubao-1, Gemini-1, Gemini-2, Gemini-3, DeepSeek-1, GLM-1 | 100 |
| `gpt-6.1-sol/` | GPT-6.1 Sol | Codex | 4 | 4 | Doubao-1, DeepSeek-1, GLM-1, MiMo-1 | 79 |
| `opus-5.5/` | Opus 5.5 | Claude Code | 10 | 4 | Doubao-1, Gemini-1, GLM-1, MiMo-1 | 60 |
| `fable-5.1/` | Fable 5.1 | Claude Code | 4 | 2 | Gemini-1, GLM-1 | 35 |
| `grok-4.7/` | Grok 4.7 | Grok Build | 2 | 2 | Doubao-1, Gemini-1 | 33 |
| `sonnet-5.5/` | Sonnet 5.5 | Claude Code | 2 | 2 | Doubao-1, Gemini-1 | 22 |

Per-case verdicts and timings are in `../results/round2/矩阵.json`; see [defect classification](../results/round2/缺陷点归类_en.md). Fable and Opus submitted light, medium, and heavy versions for the same defect; only the heaviest versions survived the post-contest review, hence the difference between submitted and upheld counts. Astra records its unsuccessful approaches for Kimi in `round2/gpt-6-astra/kimi-k3/说明.txt`.

## `round3/`: testers' Round 3 final submissions

Folders are `<lead-programmer>__<tester>/`, containing the `src/` submitted after taking over a teammate's code. These programs accept only the new input format (two additional numbers at the end of `PARAMS`).

| Folder | Tester | Tool | Tester rank | Score | 9 hidden workloads | Speedup over original lead submission |
|---|---|---|---:|---:|---|---:|
| `mimo-v2.6-pro__gpt-6-astra/` | GPT-6 Astra | Codex | 1 | 86.2 | All correct | 31× |
| `deepseek-v4.1-flash__opus-5.5/` | Opus 5.5 | Claude Code | 2 | 84.8 | All correct | 629× |
| `gemini-3.8-flash__gpt-6.1-sol/` | GPT-6.1 Sol | Codex | 3 | 79.1 | All correct | 48× |
| `doubao-seed-2.1-turbo__fable-5.1/` | Fable 5.1 | Claude Code | 4 | 65.3 | All correct | 54× |
| `kimi-k3__sonnet-5.5/` | Sonnet 5.5 | Claude Code | 5 | 55.1 | All correct | 8× |
| `glm-5.3__grok-4.7/` | Grok 4.7 | Grok Build | 6 | 15.4 | All incorrect | — |

Code files in `glm-5.3__grok-4.7/` are byte-identical to `round1/glm-5.3/`: Grok used the full 45 minutes without modifying any file. The two `HANDOFF.md` files differ by a few words because the organizer substituted neutral role names when packaging the handoff (for example, “attacker” became “tester”); Grok did not make these changes. Other teams also received copies with those substitutions.

The tester leaderboard also includes Round 2 counterexample scores. See [competition conditions](../CONDITIONS_en.md) for the full formula.

## `ep7/`: Episode 7 — Legacy Code Optimization

Individual submissions for Episode 7's second event, using the Round 1 lead programmer task, are described [here](ep7/README_en.md).
