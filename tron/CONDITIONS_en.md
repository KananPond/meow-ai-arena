[简体中文](CONDITIONS.md)

# Tron · Conditions and incidents

> As in other episodes, this page discloses every interruption, time credit, organizer intervention and incident in the counted runs. The video omits these details for pacing; this page is authoritative. Times are local to the contest PC (the same machine as episode 6).

## Format

- One half. Everyone worked on the same Windows 11 PC (Intel Core i9-13900HX), each in an empty folder with only `workspace.zip` (this directory's `workspace/`, sparring bots compiled-only). The opening prompt was identical for everyone (`prompt.md`); only resumption messages followed.
- **Timing:** starts at extraction; contestants can check `clock.py`. Recommended 45 minutes (a record-only snapshot at 45), forced sealing of `submission/bot.cpp` at 60 by the timing script. No earliest submission. Early finishes were manually collected and checked again a minute later. Post-seal edits never count.
- **Boundaries:** no internet, help from other models/subagents, or terminating processes not started by the contestant. Stated in the prompt, not enforced at tool level.
- **Tools/effort:** all 10 official contestants used noninteractive OpenCode CLI (`opencode run --auto`), default effort, no explicit setting; version unrecorded. Reference AstraUF used Codex (ultra reasoning + fast service tier, version unrecorded); DSNew used dsh 0.2.1-rc.2 standard mode, effort high.
- **Evaluation:** `tournament/tourney.py` on the contest PC, formal mode throughout (one process per bot, CPU-time penalties), pinned to 8 performance cores, Balanced power plan. Compile: `g++ -std=c++20 -O2 -static -Wl,--stack,67108864` (GCC 16.2.0).

## Contestants

Time in minutes is “start to sealing / last bot.cpp edit”, on the clock after time credits.

| Contestant | Model | Access | Sealing | Time | Interruptions/credits |
|---|---|---|---|---|---|
| Wenxin | ERNIE 5.1 | Baidu Qianfan | Manual after submission | 38.4 / 21.5 | None; session stopped itself at minute 24.4 |
| Pangu | openPangu-2.0-Pro | Huawei Cloud MaaS | Manual after submission | 44.6 / 39.3 | About 1 minute in, server stopped output, stalled about 37 minutes; 2234 seconds credited, original session resumed |
| Dots | Xiaohongshu dots3-note preview | dots official API | Forced deadline | 60.1 / 57.2 | Two stalls, 1294 and 546 seconds credited, original session resumed. Another ~7.4-minute connection reset was not credited (after last edit, no effect on sealed version) |
| LongCat | Meituan LongCat-2.0 | OpenRouter | Forced deadline | 60.0 / 53.1 | None |
| Qwen27B | Qwen3.8-27B | OpenRouter | Forced deadline | 60.1 / 38.1 | None; session ended around minute 41, sealed at 60 |
| Doubao | Doubao-Seed-Evolving | Volcano Engine Coding Plan | Forced deadline | 60.0 / 58.9 | Plan quota exhausted around minute 35; user upgraded plan, 2723 seconds credited, original session resumed, previous forced seal voided |
| Mistral | Mistral Large 4 | OpenRouter | Manual after submission | 59.9 / 56.6 | None |
| Hunyuan | Tencent Hunyuan Hy4 preview | OpenRouter | Manual after submission | 59.8 / 53.1 | None |
| Step | StepFun Step 5 preview | OpenRouter | Manual after submission | 59.7 / 34.1 | None; session ended around minute 37 |
| Mercury | Inception Mercury 3 | OpenRouter | Forced deadline | See below | See below |
| AstraUF (reference) | GPT-6 Astra, ultra + fast | OpenAI subscription (Codex) | Manual after submission | 54.5 / 39.7 | None; an automatic update invalidated the launcher Codex path, delaying start ~5 minutes (clock unaffected, starts at its own extraction) |
| DSNew (reference) | DeepSeek V4.1 Flash | DeepSeek official API (dsh) | Manual after submission | 36.1 / 29.4 | None; submitted at minute 30.9 |

- For Mistral, Hunyuan, Step and AstraUF, “start to sealing” uses the organizer's batch collection at 10-17 01:18:05, not each session's stopping time.
- Pangu, Qwen27B and Mercury extracted into `workspace\workspace`; timing/sealing were unaffected.

## Interruptions, credits and incidents

- **Credits:** after external interruptions (outages, stalled service, quota exhaustion), the organizer moved the start timestamp forward from the records and sent a resumption message (`prompt.md`). Seconds were estimated from sessions/logs, erring in contestants' favor.
- **Mercury (10-20 addition):** started 03:34:19, forced seal 04:47:54, unchanged on recheck 04:48:57. Session paused ~10 minutes at 03:55:34–04:05:35. Suspecting upstream overload (the organizer had encountered rate limits in direct API calls), the organizer credited 1200 seconds at 04:06:34 and sent a resumption message, then corrected the net credit to 810 seconds at 04:08:49. **Later investigation found Mercury's own test command had run through the tool's 600-second timeout; this should not have earned credit.** The credit was retained because it had no effect: last bot.cpp edit was 04:21:29, so the pre-credit deadline 04:34:19 and adjusted deadline 04:47:49 sealed the same file (sha256 `45dfa785…`). Before credits, last edit was minute 47.2 of 60; the “60.1 / minute 33.7” in `results/RESULTS_*.md` uses the credited clock. A second 600-second tool timeout, 04:23:58–04:33:59, earned no credit. The session stopped after a file write, without a final summary.
- **Doubao edited after sealing:** after forced sealing at 10-17 02:03:20, recheck found changes (session ran ~2 more minutes). The sealed version counts; later edits do not.
- **Three DeepSeek runs** (strong board only): first, dsh creative mode, start 00:42, submitted at minute 13.5 believing it had used 55–56 minutes; no environment fault, voided. Second, dsh 0.2.0 standard mode, start 01:40, stopped editing after minute 4 for unknown reasons; voided after upgrading to 0.2.1-rc.2. Only third run, DSNew, start 03:15, counts.
- **Human team:** planned, ultimately did not compete and appears on neither board.
- **Final:** 4267 games uninterrupted on 10-18 06:20–08:10; 1446 added on 10-20 06:10–07:17. Every program compiled; none replaced by template. No crashes or invalid output. Only Doubao overran: 14 of 1084 official-board games, 18 of 1220 strong-board games, one turn each, maximum 51.6 ms. Under the rules it continued in the previous direction, without forfeiting.

## Extra context injected by the environment

- **dsh time hints:** DSNew's dsh 0.2.1 automatically inserts the current time at minutes 10, 20 and 30; other tools do not. Consider this when discussing DSNew's time judgment.

## Post-race scoring changes

Contestant `RULES.md` said greedy = 0 and plays every type; 3–4 players includes 3-player games; one board. After the race (morning 10-17, before final), this became:

| Item | Contestant rules | Final |
|---|---|---|
| 0-point anchor | greedy | Doubao (shift only, no rank change) |
| greedy | Plays every type | Multiplayer seat filler only; unscored, unranked, no 1v1 |
| Types | 1v1, 3–4 players including 3, 12 players | No 3-player games; 3–4-player component uses 4-player only |
| Boards | One | Official (10 contestants) and strong-opponent (adds 2 references and hard sparring line) |
| Weights | 1v1 50%, 3–4 players 30%, 12 players 20% | Unchanged |

## Post-race checks

- All 12 programs are byte-identical to sealed sources, frozen by sha256 before final (`results/清单.json`, `results/程序.json`).
- Importing the 10-18 final and both previews checked engine, compiler and source hashes: all matched. Entire preview games with overruns were discarded (fast mode records without penalties and is not equivalent to formal): 3 games total.
- New multiplayer games avoided all imported seeds. 1v1 deliberately reused the same 20 maps as 10-18 so every pair shared a map set.

## Interpretation

- Each model ran once. Post-race fast-mode stability reruns on smaller schedules gave similar Wenxin results but ~300-point Pangu and ~290-point Qwen27B differences, comparable to gaps between models (their second runs were incomplete due to server resource freezing/account balance exhaustion). Places describe this one bot.
- Scoring imports some fast previews; win rates/death causes in `results/视频统计_*.md` cover formal games only, a different set.
- “Own trail” death includes the last move when already enclosed with no escape; it does not directly mean the bot trapped itself.
