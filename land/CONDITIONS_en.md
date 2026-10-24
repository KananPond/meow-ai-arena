[中文原文](CONDITIONS.md)

# Territory · Conditions and incidents

> Published in full **at the organizer's explicit request**. This page is authoritative for details omitted from the main video.

## Format

- Nine models worked on a Windows PC on September 26, 2026, starting in two batches. Each received the same contestant package (`workspace/`) and prompt (`prompt.md`; [translation](prompt_en.md)).
- About 60 minutes, managed by the model, with a hard stop at 90 minutes. `clock.py` starts at extraction. Only files in the current folder could be accessed; no internet or questions to the organizer.
- Each model ran once. **Reasoning effort followed each vendor's official recommendation**, unlike episode 3, which used the highest setting for everyone.
- Official tournament: 12 bots (9 models + random, greedy and the organizer's sparring bot).
  - 1v1: every pair played 200 maps in both seats, 26400 games.
  - 4-player: 300 rounds, randomly grouped tables, 4 seat rotations on the same map per table, 3600 games.
  - 0 timeouts and 0 forfeits throughout. Multiple runs of the same model do not face each other (this episode has only one run per model).

## Contestants

| ID (folder under `bots/`) | Model | Tool | First version | Submitted |
|---|---|---|---:|---:|
| `sonnet-5@claude-code#1` | Sonnet 5.5 (note 1) | Claude Code | 4.2  minutes | 56.4  minutes |
| `gpt-6-astra@codex#1` | GPT-6 Astra | Codex | 5.3 | 56.2 |
| `opus-5.5@claude-code#1` | Opus 5.5 | Claude Code | 3.4 | 38.6 |
| `gpt-6-sol@codex#1` | GPT-6 Sol | Codex | 4.0 | 49.3 |
| `mimo-2.6-pro@zcode#1` | MiMo V2.6 Pro | ZCode | 22.8 | 46.7 |
| `kimi-k3@workbuddy#1` | Kimi K3 | WorkBuddy | 11.5 | 56.0 |
| `glm-5.3@workbuddy#1` | GLM 5.3 | WorkBuddy | 36.3 | 48.9 |
| `mystery-model@zcode#1` | Spacebunny (note 2) | ZCode | 6.6 | 41.1 |
| `deepseek-4.1f@workbuddy#1` | DeepSeek V4.1 Flash | WorkBuddy | 5.8 | 26.5 |

- **Note 1:** Sonnet's logged model ID was `claude-sonnet-5`. Anthropic was rolling out Sonnet 5.5 at the time; the organizer checked its knowledge cutoff and confirmed the actual model was 5.5, so it is recorded as Sonnet 5.5. Before episode 2, the rollout had ended and the model reverted to Sonnet 5, so Sonnet did not take part in episode 2.
- **Note 2:** This was the anonymous stealth model **Spacebunny** on OpenRouter (the video's “mystery model”), later identified as **MiniMax M3.1 Flash**.
- `tournament/sparring.cpp` is the **organizer-written sparring bot**, not a contestant model. It was withheld from contestants and entered the official tournament as a recurring strength reference (`anchor-sparring`).

## Interruptions and incidents

- **GLM 5.3:** WorkBuddy has a per-output limit. GLM thought for about 35 minutes before writing code, and was cut off twice, at minutes 12.5 and 25.6, without producing a single line of code. Later, just after improving to 7:3 against greedy, a 429 quota limit interrupted it. The code at interruption counts.
- **Spacebunny:** one API stall of about 600 seconds.
- **DeepSeek:** estimated time without checking the clock, believed 85 minutes had elapsed, and handed in at minute 26.5. This was the model's own decision; the organizer did not intervene.
- **Voided earlier round:** an earlier trial used a hard 30-minute limit, with GLM and Kimi using a third-party API relay. After switching to a soft “about 60 minutes” limit, everyone reran; GLM and Kimi switched to WorkBuddy. The old results do not count.

## Scoring

- Separate Bradley–Terry fits for 1v1 and 4-player games. Each 4-player game becomes 6 pairwise comparisons by placement. Each pair that met receives one virtual draw.
- Each format is scaled with greedy = 0 and its strongest model = 100; values below greedy are clipped to 0. Total = average of the two format scores, rescaled so the highest total is 100.
- See the header of `tournament/rate.py`. Episodes 2 and 3 instead use baseline = 0 with no ceiling. **Scores cannot be compared directly across episodes.**
