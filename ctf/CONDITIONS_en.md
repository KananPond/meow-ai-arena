[中文原文](CONDITIONS.md)

# Capture the Flag · Conditions and incidents

> Published in full **at the organizer's explicit request**, including all interruptions, time credits and incidents. The main video omits these details for pacing; this page is authoritative.

## Format

- Eleven models worked on a Windows 11 PC, receiving the same contestant package (`workspace/`, with only a compiled baseline at the time) and prompt (`prompt.md`; [translation](prompt_en.md)).
- The prompt allowed 60 minutes, no bonus for early hand-in, some overrun, and forced hand-in at 90 minutes. `clock.py` starts at extraction and can be checked at any time.
- Local compilation, games and replay review were allowed. No internet, other AIs, or other contestants' materials.
- Each model ran once, with the highest reasoning setting its tool could select. The principle was **each vendor's own agent tool**, or a commonly used community tool if none existed.
- Development ran in power-saving mode. Official evaluation used performance mode and 12 concurrent games, equally for everyone. Taking the slowest turn of each game, the median was about 22 ms for GPT-6 Astra, 11–14 ms for GPT-6.1 Sol and 2–6 ms for the others, all below 50 ms.

## Contestants

| Model | Tool (version) | Channel | Reasoning effort | Time (minutes, pauses excluded) |
|---|---|---|---|---:|
| GPT-6 Astra | Codex desktop | official OpenAI (subscription) | highest | 60 |
| GPT-6.1 Sol | Codex desktop | official OpenAI (subscription) | highest | 67 |
| Fable 5.1 | Claude Code desktop | official Anthropic (subscription) | max | 39 |
| Sonnet 5.5 | Claude Code CLI 2.1.284 | official Anthropic (subscription) | max | 52 |
| Opus 5.5 | Claude Code desktop | official Anthropic (subscription) | max | 46 |
| DeepSeek V4.1 Flash | dsh 0.2.0-rc.2 (official DeepSeek tool, standard mode) | official DeepSeek API | max | 35 |
| Qwen 3.8 Max | Qwen Code 0.24.7 CLI | Cline pass, Cline contributor endpoint | max | 49.6 |
| MiMo V2.6 Pro | MiMo Code 0.1.15 CLI | Cline pass, through OpenRouter to Xiaomi | max | 51.8 |
| Kimi K3 | Kimi Code desktop 1.0.4 | Cline pass, Cline contributor endpoint | max (see note 1) | 53.6 |
| GLM 5.3 | ZCode desktop 3.14.3 | Cline pass, actually routed to Baseten (third party, NVFP4 quantization) | max | 63.9 |
| Doubao Seed 2.1 Turbo | trae-agent 0.1.0 (open-sourced by ByteDance) | OpenRouter | see note 2 | 53.9 (forced hand-in) |

- **Note 1:** Kimi Code exposes only whether thinking is supported. Code inspection confirmed that enabling support for an OpenAI-compatible provider sends no effort parameter, so `default_effort = "max"` was manually added for this model. Session records confirm requests carried `thinkingEffort: "max"`.
- **Note 2:** trae-agent does not support effort parameters. A small local patch added the `reasoning` parameter to requests through an environment variable. trae-agent does not save the model's reasoning content.
- **DeepSeek:** four runs in total. First, an OpenCode run in the first batch (interrupted by a permission dialog, credited 250 seconds); then dsh standard, dsh minimal and ZCode for tool comparison. The organizer adopted the vendor-tool principle from this comparison; the dsh standard run counts officially. See [the four-tool comparison](appendix/deepseek-tool-comparison/README_en.md).

## Interruptions, time credits and incidents

All pauses were credited in full by moving the `clock.py` start time forward, so contestants' displayed time excluded pauses. Resumption messages were neutral, such as explaining that an API quota interruption was not the contestant's fault, time had been restored, and work should continue from the interruption; or simply asking it to continue. No other information was disclosed.

| Contestant | Pause | Cause and handling |
|---|---|---|
| Qwen | 4 hours 8 minutes | Cline 5-hour quota exhausted; resumed the same session with `--resume` after reset |
| Qwen | Before the valid start | Effort was not set on the first start; discovered at minute 15. Run voided and archived, directory reset, restarted with max |
| GLM | 4 hours 57 minutes | Cline 5-hour quota exhausted; resumed after reset |
| GLM | 5 minutes 18 seconds, 2 minutes 10 seconds | ZCode stopped itself twice (`Protocol runtime stopping` in logs); resumption messages resent |
| K3 | 10 minutes 32 seconds | Cline gateway disconnected mid-request three consecutive times; automatic tool retries |
| K3 | 1 minute 18 seconds | New account's monthly quota exhausted; switched back to the old account |
| K3 | 2 hours 19 minutes | Old account's 5-hour quota exhausted; waited for reset |
| K3 | 27 minutes 31 seconds | **Organizer error:** the clock was not corrected before resumption. K3 saw 189 minutes elapsed and the 90-minute limit exceeded, and handed in without changing code. After correcting the clock, the next pasted message **accidentally included the organizer's internal notes**, saying quota would last only about 10 minutes and exhaustion would force hand-in. K3 wrapped up early, ran one version comparison, confirmed the current version was strongest and handed in. Final code was **byte-identical** to before the pause, so it gained no benefit and lost at most about 6 minutes |
| Doubao | 10 minutes | One request hung for about 10 minutes; resent after client timeout, returned within 1 minute |
| Doubao | Hand-in | OpenRouter gave no discount for Doubao cache hits. Measured cost was about $0.17 per minute, increasing with context. Spending exceeded the organizer's budget; forced hand-in at minute 54 when balance reached the preset floor. Current code counted. Total about $4.6 |
| Doubao | Before starting | Two failed launches, neither began work: prompt-file encoding error, then Windows CLI unable to handle a Chinese directory name. Restarted in an English-named directory |

Separately, before any interruption, GLM never checked the clock in its first segment and announced at minute 31 that it was already badly over time. This was the model's own judgment.

## Official tournament

- 32010 games: 110 maps for each count from 2–8 sides; for 9–15 sides, two copies of each contestant were available in the same game, again 110 maps per count. Every map uses a full seat rotation.
- Scoring: rank by game score, split placements into pairwise outcomes and fit Bradley–Terry strength. Baseline = 0, ten times stronger = +400; equal weight for each count from 2–15 sides. Intervals use 200 map resamples.
- Faults: 33 single-turn timeouts, each losing only one turn without eliminating the bot. All occurred in 10–15-side games, often affecting several bots in the same game (one game had 7 different bots each time out once). Judged to be transient evaluation-machine load stalls, with no visible effect on results. See the `faults` field in `results/games.jsonl.gz`.
- Post-event audit: all submissions used only the C++ standard library, without file/network access or system commands. Fable and Sonnet read environment variables (Fable for debug output; Sonnet for local parameter sweeps). These variables were unset in the official tournament; both ran with their code's defaults.
