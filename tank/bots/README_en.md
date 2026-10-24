[简体中文](README.md)

# Sealed code from each model

These are the models' original contest submissions, unmodified by the organizer. `half1/` contains first-half versions (used in the half-time tournament and as the fixed opponent pool in the progress tournament); `half2/` contains second-half versions (used in the final tournament, which determines placement). The baseline is `../workspace/baseline.cpp`; the organizer's reference bot is `../tournament/sparring.cpp`.

Their sha256 hashes match `../results/*/清单.json`. Places and ratings below are from the final tournament.

| File | Model | Short name | Final place | Rating | First-half lines | Second-half lines | Changed in second half? |
|---|---|---|---:|---:|---:|---:|---|
| `gpt-6-astra.cpp` | GPT-6 Astra | Astra | 1 | 507 | 233 | 241 | Yes |
| `gpt-6.1-sol-ultrafast.cpp` | GPT-6.1 Sol（Ultrafast） | SolUF | 2 | 467 | 350 | 356 | Yes |
| `gemini-4-argon.cpp` | Gemini 4 Argon Contributor | Gemini | 3 | 355 | 1132 | 1390 | Yes |
| `gpt-6.1-sol.cpp` | GPT-6.1 Sol | Sol | 4 | 319 | 78 | 87 | Yes |
| `opus-5.5.cpp` | Claude Opus 5.5 | Opus | 5 | 259 | 435 | 476 | Yes |
| `muse-wonder-full.cpp` | Muse Wonder Full | Muse | 6 | 55 | 560 | 560 | No (byte-identical to first half) |
| `mimo-v2.6-pro.cpp` | Xiaomi MiMo V2.6 Pro | MiMo | 7 | 29 | 336 | 336 | No (byte-identical to first half) |
| `grok-4.7.cpp` | Grok 4.7 | Grok | 8 | 26 | 1099 | 1205 | Yes |
| `deepseek-v4.1-flash.cpp` | DeepSeek V4.1 Flash | DeepSeek | 9 | −6 | 367 | 425 | Yes |
| `doubao-seed-evolving.cpp` | Doubao-Seed-Evolving | Doubao | 10 | −31 | 402 | 308 | Yes |
| `kimi-k3.cpp` | Kimi K3 | Kimi | 11 | −129 | 353 | 487 | Yes |
| `haiku-5.5.cpp` | Claude Haiku 5.5 | Haiku | 12 | −149 | 210 | 210 | No (byte-identical to first half) |
| `qwen-3.8-max.cpp` | Qwen Qwen 3.8 Max | Qwen | 13 | −205 | 537 | 392 | Yes |
| `glm-5.3.cpp` | Zhipu GLM 5.3 | GLM | 14 | −315 | 551 | 651 | Yes |

Short names are used in result tables, replays and configs. Use these bots as opponents with `../workspace/arena.py`; for example, from `workspace/`, run `python arena.py batch submission/bot.cpp ../bots/half2/gpt-6-astra.cpp --games 20`.
