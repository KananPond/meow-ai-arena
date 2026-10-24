[中文原文](CONDITIONS.md)

# Bomberman · Conditions and incidents

> Published in full **at the organizer's explicit request**. This page is authoritative for details omitted from the main video.

## Format

- Nine models worked on a Windows PC on September 28, 2026. All received the same contestant package (`workspace/`, rules v0.5-public-state) and prompt (`prompt.md`; [translation](prompt_en.md)), verified identical for all 9.
- Aim for 60 minutes; mandatory hand-in at 90 minutes. `clock.py` starts at extraction. No internet, other AIs, or other contestants' materials.
- Each model ran once. The previous winner Sonnet was absent: a pre-event knowledge-cutoff check found that the Sonnet 5.5 rollout had ended and the model had reverted to Sonnet 5.
- All programs were sealed after hand-in and compiled uniformly (`g++ -std=c++20 -O2`, plus `-static` on Windows). SHA-256 values for all 10 source files are recorded in [bots/README_en.md](bots/README_en.md).

## Contestants

| Model | Tool · channel | Reasoning effort | Time (minutes, pauses excluded) |
|---|---|---|---:|
| GPT-6 Astra | Codex · official OpenAI | medium | 56.8 |
| GPT-6 Sol | Codex · official OpenAI | high | 57.4 |
| Opus 5.5 | Claude Code · official Anthropic | medium | 39.5 |
| DeepSeek V4.1 Flash | ZCode · official DeepSeek API | high | about 64 |
| Kimi K3 | ZCode · Cline pass | max | about 67 |
| Qwen 3.8 Max | ZCode · Cline pass | xhigh | about 78 |
| GLM 5.3 | ZCode · Cline pass | max | about 60 |
| Doubao Seed 2.1 (actually used Turbo) | ZCode · OpenRouter | enabled | about 34 |
| MiMo V2.6 Pro | ZCode · Cline pass | enabled | about 74 |

## Interruptions, time credits and special handling

For every pause, elapsed time was preserved at the last successful response. On resumption, the start time in `clock.py` was moved forward so pauses did not count.

| Contestant | Incident | Handling |
|---|---|---|
| Kimi K3, GLM 5.3 | Both exhausted the Cline 5-hour quota (429), pausing for a little over 4 hours | Resumed the same sessions after reset, preserving elapsed time before interruption |
| Qwen, MiMo | One request stalled for about 9 minutes | Credited the stalled time and resumed |
| DeepSeek | One stalled request and one SSL decryption error | User resent the request; interruption time credited from the records |
| DeepSeek | First ran with the official DeepSeek tool dsh (minimal mode, high effort, version 0.1.7), then again with ZCode | The ZCode run counts officially (271). The dsh run is archived and unranked; a temporary post-event comparison rated it about 65 (390 games). Episode 3's tool comparison suggested minimal mode was likely the main reason for dsh's poor result |

## Official tournament

- **Official results:** expanded tournament, 8930 games, equal total weight 1/9 for each player count from 2–10, 4 concurrent games, 50 ms per turn (2000 ms first turn). Two-player games use every pairing in both seats. For 3–9 players, randomly permute all ten entries per map, form window groups and rotate seats within each group. Ten-player games use forward and reverse seat rotations. 0 runtime errors.
- **Original official tournament:** 2600 games, only 2-, 5- and 10-player games; results in `appendix/original-2600/`. Both versions have exactly the same ranking. BT values differ slightly because the game-size distributions differ ([comparison](results/vs-original-2600_en.md)); **the differences must not be interpreted as changes in capability**.
- **Scoring:** game score = survival score + 0.5 × kill credit. Rank by game score, split into pairwise outcomes (ties count as half a win), and add one virtual draw per pair that met. Baseline anchored at 0; scale = 400 × log10(contestant strength / baseline strength), with no rescaling to the winner. See `tournament/evaluate.py` and [workspace/README_en.md](workspace/README_en.md).
