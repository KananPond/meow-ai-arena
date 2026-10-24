[简体中文](CONDITIONS.md)

# Tank Battle · Conditions and incidents

> As in other episodes, every interruption, time credit, organizer intervention and incident in the counted runs is disclosed here. The video omits details for pacing; this page is authoritative. Times are local to the contest PC (US Pacific, UTC−7).

## Format

- 14 models, one run per half. Local contestants worked in batches on one Windows 11 PC (Intel Core i9-13900HX); outside operators ran SolUF, Gemini and Muse on another PC (AMD Ryzen 7 9800X3D).
- Each got an empty folder with only `workspace.zip` (this directory's `workspace/`, baseline compiled-only). Identical opening prompt (`prompt.md`); only half-time, resumption and post-match messages followed.
- **Timing:** first half starts on workspace extraction; second on half-time extraction; `clock.py` available throughout. Each half 30 minutes, earliest submission 20 (rule/reminder only, not blocked), forced sealing of current `submission/bot.cpp` at 45 by organizer script. Early submission manually collected and rechecked a minute later.
- **External contestants:** organizer could neither time nor force sealing on their machine. Durations are operators' records; bots were registered on receipt.
- **Boundaries:** no internet, help from models/subagents, or killing processes other than one's own. Prompt rules, not tool-level restrictions.
- **Effort:** not unified; tool defaults. Recorded: all three GPTs and both Claudes medium; DeepSeek/Kimi high; Muse high (tool default); GLM max; Gemini/Doubao/MiMo `variant: default` (no specific level mapping); Grok no concrete value; Qwen unset/default.
- **Evaluation:** all three tournaments via `tournament/tournament.py` on contest PC, performance power mode, performance cores only, one physical core per bot, 4 games concurrently. Compile `g++ -std=c++20 -O2 -static` (GCC 16.2.0). Speed checked before/after each batch (all games on one map); changes or baseline deviations >15% flagged but still counted.

## Contestants

| Contestant | Model | Tool (version) | Access | Machine | First-half time | Second-half time |
|---|---|---|---|---|---|---|
| Astra | GPT-6 Astra | Codex desktop 0.162.0-alpha.17.2 | OpenAI subscription (official client) | contest PC | 40.3 / 28.6 | 30.4 / 26.6, 403 seconds credited |
| Sol | GPT-6.1 Sol | Codex desktop 0.162.0-alpha.17.2 | OpenAI subscription (official client) | contest PC | 34.2 / 27.8 | 31.5 / 28.7, 407 seconds credited |
| SolUF | GPT-6.1 Sol | Codex CLI 0.162.0-alpha.17.2 | OpenAI API ultrafast | external | ~29.9 (operator) | ~26.9 (operator) |
| Opus | Claude Opus 5.5 | Claude Code 2.1.296 | Anthropic subscription (official client) | contest PC | 30.4 / 23.0 | 25.4 / 16.9, 372 seconds credited |
| Haiku | Claude Haiku 5.5 | Claude Code 2.1.296 | Anthropic subscription (official client) | contest PC | 29.0 / 15.2 | 25.8, 395 seconds credited |
| Muse | Muse Wonder Full | Muse Code CLI 1.4.7 | Meta | external | ~32.1 (operator) | ~20.6 (operator) |
| Gemini | Gemini 4 Argon Contributor | OpenCode 2.0.29 | OpenCode Go | external | ~36.3 (operator) | ~35.9 (operator) |
| GLM | Zhipu GLM 5.3 | ZCode desktop 0.16.9 | Cline pass | contest PC | 43.1 / 37.5, 315 seconds credited | 26.4 / 19.4 |
| DeepSeek | DeepSeek V4.1 Flash | dsh (version unrecorded) | Cline pass | contest PC | 45.0 / 31.3 (forced seal) | 31.5 / 27.0 |
| Doubao | Doubao-Seed-Evolving | OpenCode 2.0.19 | Volcano Engine | contest PC | 45.1 / 40.7 (forced seal) | 31.4 / 27.5 (collected after quota exhaustion) |
| MiMo | Xiaomi MiMo V2.6 Pro | OpenCode 2.0.19 | Cline pass | contest PC | 35.9 / 34.1 | 22.7 / 12.9 |
| Kimi | Kimi K3 | Kimi Code CLI 2.1.1 | Cline pass | contest PC | 36.8 / 35.3, 582 seconds credited | 29.7 / 27.5 |
| Grok | Grok 4.7 | Grok Build 1.0.46 | Account pool (address withheld) | contest PC | 45.0 / 44.2 (forced seal) | 40.0 / 39.3 |
| Qwen | Qwen 3.8 Max | Qwen Code CLI 0.24.7 | Cline pass | contest PC | 41.5 / 38.7, 357 seconds credited | 15.5 / 9.9 |

- **Minutes:** local rows mean “adjusted start to seal / adjusted start to last bot edit”. First includes manual-collection delay; second better approximates completion. Start includes credits.
- GLM, DeepSeek, MiMo, Kimi and Qwen share “Cline pass”; actual upstreams were not individually verified.
- External machine is faster and can play more local games in the same time. No matched benchmark was made, so no speed multiplier is given.

## Organizer mistakes

- **GLM mistakenly collected** (10-14 first half): a ZCode tool fault was mistaken for submission; sealed at 05:47:33. Discovered 05:49:54, seal voided, resumed, finally collected manually 06:04:13. Pause credited separately (T11).
- **Credits applied ~3 minutes late** (10-15 second half): Opus/Astra/Sol/Haiku lost network together at 04:23–04:29; start timestamps were adjusted only at 04:32:29–04:32:56. Until then their clocks overstated elapsed time (Haiku read 10.2 minutes at 04:29:46, ~3.6 after credit). User sent “continue” rather than the standard resumption message in all four cases.
- **Estimated credits:** Kimi's first-half 582 seconds estimated interruption start from last bot edit; the four 10-15 credits, 372/403/407/395 seconds, slightly exceed visible session gaps. All favor contestants.
- **DeepSeek workspace changed after seal** (10-13 first half): after forced collection at 45, dsh was awakened by a background notification, started another turn and copied a different candidate into `submission/bot.cpp`. Organizer checked only once after sealing and missed this. Sealed copy counts (sha256 prefix `f71626be2ee0`, matches `bots/half1/deepseek-v4.1-flash.cpp`); later edits do not.
- **Opus early submission manually recorded** (10-15 second half): ended minute 18.9 on adjusted clock, collected minute 25.4. Script checked collection time, so did not flag early completion; manually recorded at 04:53:11.

## Interruptions, credits and incidents

Credits move the contestant's start timestamp forward, followed by a neutral resumption message in the original session (`prompt.md`), without other information. Credits are organizer judgments based on session gaps and tool logs.

| ID | Time | Contestant | Stage | Event | Handling |
|---|---|---|---|---|---|
| T01 | 10-13 06:06–06:08 | Opus, Haiku | Before start | Network outage; repeated Claude Code errors (Haiku 8, Opus 7). Both clocks start after recovery | No credit |
| T02 | 10-13 06:15 | Opus | First | Output stream cut; tool auto-resumed after ~7 seconds | No credit |
| T03 | 10-13 06:28 | Haiku | First | From minute 11 used timed wakeups to wait; submitted at 20.05, just meeting earliest time | Record only |
| T04 | 10-13 09:20 | Kimi | First | 5 consecutive request timeouts, recovered after ~51 seconds. Missing in event log; discovered in pre-release tool-log audit | No credit |
| T05 | 10-13 09:27–09:36 | Kimi | First | Outage, 10 further timeouts, process exited; original session resumed 09:36 | 582 seconds |
| T06 | 10-13 09:55 | DeepSeek | First | Forced seal at 45, later workspace edits (above) | Sealed copy counts |
| T07 | 10-13 | Muse | First | External, operator ~32.1 minutes, no interruption | Registered, not timed |
| T08 | 10-14 | Grok, GLM, Doubao, MiMo, Qwen | First | Reruns: post-race audit found external interruptions, insufficient credits or launcher-truncated opening prompts; originals voided, restarted in same folders | Only reruns count |
| T09 | 10-14 05:20–05:26 | Qwen | First | Interrupted request, ~6-minute session gap | 357 seconds |
| T10 | 10-14 05:39–05:40 | GLM | First | Brief network fault, ZCode retry succeeded after ~9 seconds | No credit |
| T11 | 10-14 05:46–05:51 | GLM | First | ZCode tool fault canceled turn; next turn after resumption began 05:51:39; 315 seconds matches log | 315 seconds, applied twice in portions |
| T12 | 10-14 05:47–05:49 | GLM | First | Mistaken collection, immediately voided (above) | Erroneous seal voided |
| T13 | 10-14 05:54 | GLM | First | Another ZCode fault, recovered 11 seconds later | Too brief; no credit |
| T14 | 10-14 05:31 | Doubao, Grok | First | Both used full 45 minutes, forced seal; within rules, listed because both exhausted time | Deadline seal |
| T15 | 10-14 | SolUF, Gemini | First | External, operators ~29.9, 36.3 minutes; Gemini over 30, under 45 | Registered, not timed |
| T16 | 10-14 21:41–22:41 | All | Half-time tournament | 10080 games, 0 referee errors. 5/42 batches (0,1,2,4,22; 1200 games, ~11.9%) flagged: within-batch ~18–20% difference, start deviation 22–27%, or other work on a logical processor. Batches 0–2 used warmup baseline; later completed-batch median | Counted; user chose no rerun |
| T17 | Half-time tournament | Doubao | Half-time tournament | First-half bot forfeited 2 games; 10 other games had overruns or illegal output | Counted under rules |
| T18 | 10-15 03:38 | Kimi | Second | Half-time package nested in extra `workspace`; clock still found it | Normal timing |
| T19 | 10-15 03:44 | GLM | Second | Package deeper; own `clock.py` could not find it and kept showing first half. Two start markers; earliest 03:44:06 used | Marker timing, no credit |
| T20 | 10-15 04:19 | Haiku | Second | Nonstandard extraction briefly showed first half, then contents moved to standard location; start unchanged | Marker location updated; no credit |
| T21 | 10-15 04:23–04:29 | Opus | Second | Outage, 10 errors, visible gap ~321 seconds | 372 seconds |
| T22 | 10-15 04:24–04:29 | Astra | Second | Outage, visible gap ~340 seconds | 403 seconds |
| T23 | 10-15 04:23–04:29 | Sol | Second | Outage, visible gap ~337 seconds | 407 seconds |
| T24 | 10-15 04:23–04:29 | Haiku | Second | Outage, 28 errors, visible gap ~386 seconds | 395 seconds |
| T25 | 10-15 04:05 | Doubao | Second | Minute 27.8, Volcano Engine 429: 5-hour quota exhausted, process exited while finalizing/build-testing | No credit or wait for reset; 04:09 collected current `submission/bot.cpp`, confirmed compiling and same as 30-minute snapshot |
| T26 | 10-15 04:44 | Opus | Second | Finished minute 18.9, before earliest time | Record only |
| T27 | 10-15 03:54 | Qwen | Second | Finished minute 15.5, before earliest time | Record only |
| T28 | Second-half seal | Muse, MiMo, Haiku | Second | Byte-identical to own first-half bots. Muse operator recorded testing candidates then restoring first; Haiku retained original after variants; MiMo wrote bot file but final content matched first half | Counted normally |
| T29 | 10-14 23:05 – 10-15 01:00 | Muse, SolUF, Gemini | Second | External, operators ~20.6,26.9,35.9 minutes; all reported no human replies mid-run. Muse second-half effort/tool version not saved, so consistency with first half unconfirmed | Registered, not timed |
| T30 | 10-15 05:00–05:56 | All | Final tournament | 14400 games, 0 referee errors, none ended in fault. 5/60 batches (5,16,29,34,53) speed-flagged | Counted normally |

## Extra environment context

Unrelated to the task, but undisclosed additional context with unquantifiable effects:

- **User global rules file:** a personal work convention unrelated to contests. Temporarily renamed for Claude (Opus/Haiku), restored later. Not moved during Grok's 10-14 first-half rerun; Grok Build injected it as user rules. Moved before second half, but the resumed session retained the first-half context.
- **Claude Code** (Opus/Haiku) injects the user's installed skill catalog at session start.
- **Codex** (SolUF; inferred same for Astra/Sol) includes base instructions, memory guidance, skill catalog, permissions and collaboration rules.
- **Muse:** preset rules, memory mechanism and internal reminders; second half resumes with first-half history.
- **`arena.py check`** prints the contest compiler's absolute path, including organizer username. No task effect; repository files omit that path.

## Post-race checks

- All 28 sealed sources (14 × 2) use only `tank.hpp` and C++ standard library, with no file access, environment reads, system commands, threads or `<windows.h>`.
- Every bot sha256 matches all three manifests (`results/*/清单.json`), as do baseline/reference/template sources.
- All three configs regenerate identical `games_sha`. Replayed first 352 final games on the same PC before release: 338 identical turn for turn, 347 same winner. All differing games involved budget-based search.

## Interpretation

- One run per model per half: this event's results, not a rigorous benchmark.
- Tools, access, effort and machines differ: five via Cline pass rather than verified official endpoints; three on a faster external machine without organizer timing; medium through max effort.
- Five first halves were rerun after contestants had already seen the task.
- Credits are organizer judgments, some estimated. Doubao's second half stopped at minute 27.8 from quota exhaustion without credit.
- Flagged batches still count. Adjacent differences whose intervals include 0 are tied; do not read placement gaps as certain strength differences.
- `tournament/` computes results under preset rules; per-game records permit recalculation.
- Raw sessions and voided-run data are omitted. Session descriptions above rely on organizer records and cannot be independently checked by readers.
