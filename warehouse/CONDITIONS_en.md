[简体中文](CONDITIONS.md)

# Warehouse scheduling · Conditions and incidents

> As in other episodes, this page discloses every interruption, voided run, organizer intervention and incident. The video omits details for pacing; this page is authoritative. Times are local to the contest PC; the system timezone changed twice. See “Time and timezones” for each period.

## Format

- 12 models on one Windows 11 PC. Organizer split them into 6 testers and 6 leads based on prior results (rosters in `referee/抽签.py`), randomly paired across pools with seed 20261006. Reproduce: `python referee/抽签.py --seed 20261006 --dry`. Grok 4.7 and Gemini 3.8 Flash were new contestants.
- Finish each round before starting the next:

| Stage | Participants | Task | Time |
|---|---|---|---|
| Pre-race | 12 | Announce draw, one trash-talk line each | — |
| Round 1 | 6 leads | Optimize baseline, byte-identical output, `HANDOFF.md` | Suggested 30 minutes, forced stop 45 |
| Round 2 | 6 testers | Counterexamples against other 5 leads, at most 3 per target with explanations | Hard 45, no early bonus |
| Round 3 | 6 testers, fresh sessions/folders, no other teams' code | Own lead takeover: fix regressions, implement change, focus on optimization | Hard 45, no early bonus |
| Post-race | Astra, Opus, DeepSeek | Maintainability blind review of 12 anonymized entries, one session per judge | — |
| Post-race, unscored | Astra, Opus | Retest other 5 teams' finals for counterexamples | Hard 45 |
| Post-race, unscored | Organizer | Final random differential tests and performance probes | — |
| Post-race | 12 | Results and reviews | — |

- **Timing:** round 1 starts when organizer sends the prompt; CLI process trees killed at 45, manual seats' `src/` sealed then. Rounds 2–3 start on extraction: `referee/自动计时.sh` watches folders, records extraction, seals 45 minutes later. Package `clock.py` shows elapsed time.
- **Boundaries:** separate `C:\arena5\第<轮> <队> <角色> <模型>` folders. No internet, other models/subagents or outside read/write, **prompt-only, not tool-enforced** by user decision. `referee/进度.py` and `巡检.py` scan tool output for outside paths/network calls. Claude Code: `bypassPermissions`; Codex: `danger-full-access`, no approval prompts; Grok Build: `--always-approve`; OpenCode: `--auto`; Antigravity CLI: `--dangerously-skip-permissions`.
- **Effort:** tool default/recommended (episode 3 used maximum). Recorded values below.
- **Concurrency:** user chose all 6 each round concurrently, slowing contestants' own tests. Official scores measured later on idle machine.
- **Wording:** round 1 used attack/defense/attacker/break; rounds 2–3 used tester/regression/counterexample (reasons below). Rules unchanged.
- **Scoring**, locked before rematch, `referee/团队计分.py`: any byte mismatch makes that workload 0. Components on 0–100 scale:

| Component | Formula | Team weight |
|---|---|---:|
| Lead speed S1 | Round 1 code on 8 hidden workloads; error or no speedup → 0; otherwise 100 × ln(speedup) / ln(max lead speedup for workload), averaged | 15 |
| Resistance D | (5 − testers finding a counterexample)/5 × S1; multiplication by S1 prevents unchanged code being safest | 10 |
| Lead blind review M1 | Mean judge score ×10, excluding own-team judge | 5 |
| Findings A | Each root cause worth 1 + fraction of 5 eligible testers missing it; own-team entry credited by other 5 testers' average; normalize maximum to 100 | 20 |
| Takeover correctness C | 100 × (0.75 × changed-hidden correctness + 0.25 × regression repair fraction); no regressions means repair fraction 1 | 20 |
| Takeover speed S2 | 9 changed-hidden workloads, error → 0, otherwise log speedup over reference normalized to workload maximum 100 | 20 |
| Final blind review M2 | Same as M1 | 10 |

Lead board = (15 S1 +10 D +5 M1)/30. Tester board = 0.25 A +0.20 C′ +0.20 S2 +0.20 V +0.15 M2. C′ uses only hidden correctness when no team regressions. V = value added: lead's old-format CPU time / final CPU time on same maps/events, log-normalized to maximum.

## Contestants

| Model | Role / team | Tool (version) | Access | Effort | Round times (minutes) |
|---|---|---|---|---|---|
| Kimi K3 | Lead / A | Kimi Code CLI 2.1.1 (`-p`, unattended) | Cline pass | default | R1 45.0, forced |
| Doubao Seed 2.1 Turbo | Lead / B | OpenCode CLI 2.0.19 (`run --standalone --auto`) | Volcano Ark agent plan | default | R1 39.2, self-finished |
| Gemini 3.8 Flash | Lead / C | Antigravity CLI `agy`, version unrecorded | Google account, free quota | High, tool default | R1 10.5, self-finished |
| DeepSeek V4.1 Flash | Lead / D | dsh web, official DeepSeek tool, version unrecorded, user operated | Cline pass | default/recommended | R1 45.0, sealed |
| GLM 5.3 | Lead / E | ZCode desktop, version unrecorded, user operated | Cline pass, routed to third-party Baseten, NVFP4 quantization | default/recommended | R1 34.0, self-finished |
| MiMo V2.6 Pro | Lead / F | OpenCode CLI 2.0.19 (`run --standalone --auto`) | Cline pass | default | R1 45.0, forced |
| Sonnet 5.5 | Tester / A | Claude Code 2.1.286, user operated | Anthropic subscription | medium | R2 34.8, R3 28.6 |
| Fable 5.1 | Tester / B | Claude Code 2.1.286, user operated | Anthropic subscription | high | R2 38.8, R3 34.5 |
| GPT-6.1 Sol | Tester / C | Codex desktop 0.162.0-alpha.2, user operated | OpenAI subscription | medium | R2 45.0, R3 43.4 |
| Opus 5.5 | Tester / D | Claude Code 2.1.286, user operated | Anthropic subscription | medium | R2 31.3, R3 45.0 |
| Grok 4.7 | Tester / E | Grok Build 1.0.46, unattended CLI, cross-session memory off | Official accounts through account pool, address undisclosed | unrecorded | R2 45.1, R3 45.0, both forced |
| GPT-6 Astra | Tester / F | Codex desktop 0.162.0-alpha.2, user operated | OpenAI subscription | medium | R2 45.0, R3 41.7 |

- **Time definition:** R1 process start/end, or prompt paste for manual seats. R2–3 extraction to last session event, capped at 45; 45.0 means still running at deadline. Grok output has no timestamps; only forced-stop completion is known.
- **Note 1, effort:** organizer policy default/recommended. Claude/Codex session fields show Sonnet, Opus, both GPTs medium, **Fable high**; default versus manually selected for Fable cannot be verified. No verifiable records for other 7. Gemini High reflects the tool's default “Gemini 3.8 Flash (High)” label.
- **Note 2, Grok:** only contestant using official accounts dispatched through an account pool. Both rounds abnormal: R2 found a dozen-plus local cases but submitted 2; R3 used 45 minutes, only 35 tool calls (reads/searches/few commands) and 1385 reasoning segments; final `src/` identical to lead. Its first, later-voided R3 finished in 21.5 minutes. **We suspect official account throttling/degradation at that time, but no timestamps allow distinguishing slow access from extended planning; logs cannot confirm it.** No rerun; scored under rules, not representative of normal operation.
- **Note 3, Cline:** Kimi, DeepSeek, GLM, MiMo used it. Recorded upstreams only GLM→Baseten (as episode 3) and Kimi's R1 disconnect error→Fireworks. Others unverified. DeepSeek used official API in episode 3, not here.
- **Note 4, MiMo tool:** MiMo Code automatically imports local Claude/Codex sessions, including task-authoring sessions with answers, into history; also cross-project global memory. Used in voided first R1; it wrote task summary into global memory. Rerun used OpenCode (MiMo Code's base), memory removed and MiMo Code uninstalled by user. No audit record establishes whether it read imported authoring sessions in voided run.
- **Note 5, Doubao:** Cline balance-channel pricing was abnormally expensive, so Volcano Ark used.
- **Note 6, manual seats:** dsh launched from organizer assistant could not change folder permissions; all CLI tools failed. User ran `dsh web` in own terminal. ZCode is desktop, also user operated. Organizer recorded prompt-paste times synchronously.
- **Note 7, visible reasoning:** Kimi/Doubao/DeepSeek/GLM/MiMo have raw reasoning in session records; three Claudes, Gemini, Grok expose summaries only; GPT reasoning encrypted/unreadable. Video labels distinguish these. Raw sessions omitted.
- **Note 8, GLM time:** organizer hand-off once incorrectly said forced stop at 45; corrected from logs to self-finished 34.0.

Trash talk was collected separately: user asked the five Claude/GPTs via subscription tools; Gemini via Antigravity CLI; organizer records say the remaining 6, including Doubao/Grok, used direct Cline API calls, different channels from their contests.

## Interruptions and incidents

Only counted runs: R1 rerun, rematch R2–3. Voided runs below.

| Round | Who | Event |
|---|---|---|
| R1 | All | **Organizer error:** `tools/check_input.py --p2` exposed next-phase parameter names/ranges (`agingEvery`, `carryCost`); `tools/gen.py` comments mentioned phase 2. All 6 got identical files. Gemini/GLM/MiMo hand-offs anticipated changes (Gemini guessed battery aging, actually order aging). Semantics not leaked. User chose no action/rerun. Possible positive/negative blind-review effect on those hand-offs unquantifiable |
| R1 | Kimi | Upstream disconnect ~1.5 minutes; no credit by user decision; used full 45 |
| R1 | DeepSeek | Request stalled ~minute 8; at 15.5 tool reported 300-second idle stream timeout/retried; next output at 21.6, ~13 minutes no output. No credit; full 45 |
| R1 | All | Before start, outdated zip `clock.py` searched only one parent; repackaged before running |
| R1 | All | All 6 averaged ~29% waiting for tests, mainly slow baseline in custom differential scripts/huge inputs. DeepSeek waited 15.6 minutes for one unfinished command (10 foreground +~5.6 after backgrounding); command start to 45-minute seal ~17.4 minutes, basis of video's 17. Kimi waited 14 minutes on maximum-size input. Contestants' choices, not incidents, listed to explain time |
| Rematch R2 | All | No safety blocks or detected outside access, subagents, internet or name-based process kills |
| Rematch R2 | Fable, Opus, etc. | 6 models shared the machine, making tests slower than idle retests; 8 of 28 local timeout cases did not hold afterwards |
| Rematch R2 | Grok | Found a dozen-plus locally, submitted 2; submission folder counts |
| Rematch R3 | Grok | Forced after 45, no source changes, all new-format inputs abnormal exit: hidden 0/9, regression 0/4; note 2 |
| Rematch R3 | Opus | Still running at 45 seal (last event ~2 minutes later); sealed `src/` counts. Review said one final low-battery differential seed disagreed, unresolved; neither post-race random test round reproduced disagreement |

No separate conclusion in organizer records on outside access in R1/rematch R3; monitoring emitted no alert. It only scans paths/network calls in tool output and cannot prove absence.

## Voids and rematch

Many runs were voided; chronological list below. Their data is omitted.

**First R1 start, early 10-06, all voided:**

| Who | Duration | Reason |
|---|---|---|
| DeepSeek | ~6 minutes | dsh sandbox could not change folder permissions/extract; searched outside contest folder, saw Team E's identical zip/prompt, no answers/code. Organizer stopped manually |
| Doubao | ~3.6 minutes | **Organizer error:** inherited environment made OpenCode work in authoring directory. Read identical task/baseline/public inputs, no reference/hidden referee files; rewrote one authoring source. Stopped, authoring files restored/checked. Later OpenCode always `--standalone` with explicit cwd |
| Kimi, Gemini, MiMo, GLM | Kimi ~9 minutes, Gemini/MiMo ~10, GLM user-stopped with no duration record | **Organizer error:** package omitted customary `clock.py`; user chose add timer and rerun all 6 |

Fresh sessions, new root/folder names; Antigravity/MiMo cross-session memory removed. **No lead saw the task for the first time in the counted run:** Kimi/Gemini/MiMo ~10 minutes exposure; GLM unknown duration; Doubao 3.6 reading; DeepSeek unable to extract.

**First R2, morning 10-06, later entirely voided:** five launches needed:

1. Original attack/defense wording, `attack/`, `hacks/`. **Fable refused as a network attack.** Grok ~5 minutes also voided. Organizer neutralized all contestant-visible wording.
2. Fable worked but spawned subagents. **Organizer error:** prompt allowed all tools and did not prohibit them. Fable/Grok restarted; explicit no-other-models/subagents added.
3. Both GPTs stopped by OpenAI cybersecurity classifier (`cyber_policy`): Astra after instructions, Sol upon five leads' hand-offs containing attack wording. Renamed version did not block Claude/Grok. GPT-only regression/equivalence instructions and neutralized hand-off copies prepared.
4. Both GPTs could not find Python: Codex PATH had only an old tool directory with broken forwarding scripts. Organizer environment repaired; restarted.
5. Three Claudes killed all Python processes by name 5 times (Opus 2, Sonnet 2, Fable 1), repeatedly killing organizer timers, Grok wrapper (Grok continued), possibly other contestants' tests. Timers moved to bash; own-process-only rule added.

Background-only results of this voided round: Doubao/Gemini broken by 5 testers each; GLM by Astra alone; Kimi/DeepSeek/MiMo none. Durations 14.8–43.2 minutes.

**First R3, 10-06 through 10-09, later entirely voided:**

- 10-06: Grok 21.5, Sol 9.8, Astra 23.6 minutes completed normally. Claudes postponed for user quota exhaustion.
- 10-08: Fable account banned; Astra temporarily substituted Team B. Sonnet/Opus/substitute Astra all interrupted/voided: **compiler broken**. System settings changed 10-07 (code page 936→UTF-8, locale zh-TW, Windows license/update logs in same minute, not manual); linker could not open Chinese paths. Sonnet ~2 minutes, one edit then build fail; Opus ~1.5, instructions only; Astra 19.1, worked around build issue.
- 10-09: Fable restored, substitute canceled; locale restored, compiler worked. Claudes restarted: Sonnet ended after 1.2 minutes; Fable/Opus stopped after one or two minutes when rematch decided.

**Why rematch:** R3 was meant to test optimization, but prompt said “optimize if time remains”/“finish when done”, omitted weights, and package had only small tests, hiding speed. R2 conditions differed (launch count, GPT-only instructions, mutual process kills). User retained R1 and reran R2–3 in one batch with new prompts/packages/scoring.

Changes before rematch:

- R2: identical regression wording and neutralized hand-offs for everyone; explicit root-cause scoring, 60-second timeout criterion and no early bonus.
- R3: explicit optimization focus/scoring/no early bonus, plus 3 near-official large workloads and `bench2.py --big`.
- Hidden workloads 6→9: optimized original 6 took only 0.3–1 seconds; added 3 heavy cases.
- Extraction-based starts, automatic 45-minute sealing.
- Findings scored by root cause rather than targets broken; own-team blind-review votes excluded; tester value-added added. Locked before restart.

**Unequal prior exposure:**

- All six testers saw the task a second/sometimes third time. Fresh sessions/folders; user's Codex memory off, but models had done it before.
- Astra/Sol/Grok completed R3; Astra also spent 19 minutes on Doubao as Team-B substitute (rematch inherited MiMo). Sonnet had ~2-minute interruption and 1.2-minute finish; Opus two one/two-minute interruptions; Fable one one/two-minute stop.
- Organizer trial-evaluated first-event Sonnet/Grok/Sol/Astra finals on original 6 hidden workloads to decide on heavier inputs; results not shared.
- Prompt changes coincided with large changes: Sonnet R3 1.2→28.6 minutes; GLM timeout finders 1→4. Clearer instructions are more likely explanation, but repeat exposure cannot be ruled out.

## Evaluation and scoring

- Same PC after contestant sessions stopped. Hidden-speed runs serial; R2/final retest scripts run 3 cases concurrently. Fixed `g++ -O2 -std=c++20`, plus Windows `-static`; total process CPU, 1 GB.
- **R1:** 8 hidden workloads, baseline 35–218 seconds. Up to 3 runs, median; stop repeats after error or >10 CPU seconds (48 records: 31 ×3, 17 ×1). Baseline median of 3. Slower entries stopped early, score 0. GLM `hid_dynamic` slower, 0.
- **R3:** 9 changed-hidden workloads, clean unoptimized reference 18–327 seconds; timeout reference ×3+10 seconds. Six share R1 maps/events with two new parameters; three heavy additions for rematch.
- **Precision:** Windows CPU ticks ~0.016 seconds. Preliminary 1–3 runs gave top three only 2–3 ticks on light inputs, many ties. Official precise rerun: cumulative 2 CPU seconds, at most 80 repeats, average; same for lead baselines. Formulas/ranks unchanged; Astra/Opus tester gap 1.4 points. Preliminary data retained in `results/*_初测.*`. Organizer chose exact parameters; user authorized remeasurement only.
- **Baseline timed once:** original six remeasured before evaluation; heavy three measured at package creation with light background work. Shared effect across teams.
- **R2 decisions** (`referee/击穿判定.py`): valid input, baseline ≤10 seconds. Idle-machine timeout recheck: unfinished at 100 seconds holds; completion 60–100 triggers two more runs, all three must exceed 60. `results/round2/矩阵.json`: 28 cases, 20 hold/8 fail; failures are performance cases finishing under 60 idle. Classification once said 21/7; corrected at release, no effect on 7 defects/scores. Two borderline: Opus→MiMo 61.3 seconds, Astra→Gemini 63.5. **Hardware dependent.**
- **Classification:** organizer (Claude) grouped root causes by one code fix, using decisions, explanations, code reading; **no patch-and-test proof per defect** (Astra independently verified Gemini's three in its working copy). Seven occupy distinct locations/teams, no uncertain boundaries in organizer judgment. See `results/round2/缺陷点归类.md`.
- **Value added:** lead originals timed on old-format versions of 9 workloads. GLM incomplete since final all wrong, value 0: preliminary heavy three used timeout-line placeholders; precise run skipped Team E, absent from `results/主程基准.jsonl`.
- **Authorized organizer choices without itemized approval:** precise parameters; same lead remeasurement except GLM; R2 rules reused for final retests and organizer classification of 2 defects; final audit scale; three Claude reviews collected by branching CLI resumption of R3 rather than manual paste.
- **Recalculate:** team/final/R1-speed/blind-summary outputs regenerate byte-identically from raw data with `referee/` scripts, checked before release. See [referee guide](referee/README_en.md).

## Post-race audit

- **Blind review:** GPT-6 Astra, Opus 5.5, DeepSeek V4.1 Flash, one conversation each, 12 entries (6 leads +6 finals), different order per judge; five 1–5 dimensions scaled to 0–10, anchors baseline 1/clean reference 4, code evidence required. Model/vendor/tool/team names replaced with “某某”. **Organizer error:** vendor token “字节” also replaced inside “逐字节”, producing “逐某某一致”; identical copies for all judges (`referee/生成盲评包.py`). All judges competed; own-team vote excluded (Astra/Opus testers, DeepSeek lead). Leniency varied: Opus strictest, DeepSeek loosest; largest disagreement Sol final, Astra 6.5/DeepSeek 6.0/Opus 3.0. Grok final = GLM original, same scores. Follow-up overall 100-point impressions displayed only. Instructions/results in `packages/blind-review/`, `results/blind-review/`.
- **Final retest, unscored:** Astra/Opus fresh sessions, five other anonymous finals/reference, 45 minutes. Two defects: Grok no new requirements (both); Fable degrades with thousands of blocked pickups exceeding 1024-slot distance cache and blocks changing every tick (97 seconds vs reference 5.9), Astra only. Opus tried same direction below capacity, found 2× slower, correctly not a counterexample. Astra withdrew Sol 61.6-second result after 27-second recheck. No cases found for Sonnet/Sol/Opus/Astra finals; neither judge tested own team. Astra stopped at minute 43 by `cyber_policy` despite regression wording, just after timing; submitted cases judged normally.
- **Final audit, unscored, organizer:** five working finals each tested on 1500 small random and 1500 extreme cases (196 extreme skipped for slow reference), byte comparisons: **0 differences**. Performance probes converted 9 valid R2 timeouts: none timed out/differed; three slower than reference (Fable 4.0/3.0 vs. 2.8/1.8 seconds, Opus 5.0 vs. 1.8). Logs `results/final-check/`.
- **R1 preliminary audit, unscored:** each lead 1500 boundary +1500 extreme cases. Kimi/DeepSeek/GLM/MiMo no differences; Doubao boundary 437, Gemini boundary 75/extreme 90. Output-only missed all four later performance defects. Not shared; cases omitted.
- **Reviews:** all 12 in `notes/`. GLM first lacked results, re-asked, both preserved.

## Time and timezones

System timezone switched between Pacific and Taipei twice.

- R1 (10-06 04:51–05:40) and original R2–3 records use Beijing time.
- Organizer notes rematch decision 10-09; rematch logs local 10-08 12:05–14:01, post-race review/retest/evaluation same day 14:16–16:33. Inferred at release to correspond to Beijing 10-09 ~03:00–07:30; original records do not specify.
- Repository timestamps retain then-local time, unconverted.

## Interpretation

- One counted run per model per round: event results, no capability ranking.
- R1 rerun and full R2–3 rematch; all had prior exposure to differing extents.
- Random teams: takeover speed largely inherited, value-added favors weaker leads (DeepSeek's slow original contributes to Opus 629×). Equal 20% speed/value weights offset but cannot eliminate dependence.
- Grok official-account pool, abnormal R3, suspected but unconfirmed account degradation; score not normal capability. Team's final failure unrelated to GLM code quality.
- GLM third-party quantized via Cline; DeepSeek/Kimi/MiMo also Cline rather than official interfaces. Different tools/efforts (Fable high, other recorded values medium).
- All blind judges contestants; some gaps arise from judge strictness.
- Timing/timeout cases depend on this machine, two borderline findings.
- R1 tools leaked phase-2 parameter names.
- Organizer's Claude assistant authored task/referee/classification/statistics while three Claudes competed. Programs compute scores under rules locked before rematch; data/scripts allow recalculation.
- Two humans did only R1, untimed; later same-machine measurements, unscored, separate from AI measurements, in `appendix/human/`.
- Raw sessions/voided/original-event data omitted; descriptions rely on organizer records readers cannot independently check.

## Open-source audit addendum

- R3 public `edge_02,edge_09,edge_11,edge_20,edge_27,edge_29` have chargeRate >batteryMax, failing `check_input.py --full --p2`. Original `.in/.out` retained for history; out-of-domain extra tests, no formal correctness obligation.
- Post-race boundary generator drew charge rate independently: of 1500 seeds, 278 invalid/1212 valid. Historical “1500, 0 differences” is not all legal-input coverage. Extreme generator validates and skipped 196 slow-reference cases among 1500.
- R2/final retest timeouts use wall time and 3 concurrent cases, potentially affecting 61.3/63.5-second borderline cases. No new serial recheck; historical decisions/scores unchanged.
- Only `referee/评测.py` gained resume fingerprint and precise-repeat memory checks at release. No historical reevaluation/rescoring. Old JSONL without fingerprint cannot resume; use new `--out` for reruns.

## Episode 7 · Legacy-code optimization

Second event of District King reused this task. Conditions/incidents below, contest-PC local time UTC−7, date 10-20.

### Format

- Same 10 official contestants as Tron plus 3 Claude references: 13 solo sessions doing episode-5 R1, no teammates. Doubao's new code not timed, prior result carried over (note 1), so 12 measured entries.
- Same episode-5 `workspace.zip` (`workspace/`, SHA-256 `49cc19d2…`); read-only copies in all 13 folders, hashes checked. Same solo sentence for all (`packages/ep7/提示词_主程.txt`).
- **Time:** suggested 30 minutes, hard limit 45 minutes. Organizer script launches 10 OpenCode sessions, launch=start, kills process tree by PID at 45 minutes then seals `src/`. User pastes prompts/manually collects 3 Claudes. `clock.py` available.
- **Boundaries:** no internet, other AI/subagents, other contestants' materials. OpenCode prompt-only as Tron; question tool disabled from batch 2 including Pangu (LongCat below). Claude operations sheet required `--disallowedTools` network/subagent bans, actual commands unrecorded; session calls only Bash, PowerShell, Read, Write, Edit, Monitor, no network/subagents.
- **Concurrency:** batch 1 five OpenCode sessions 04:14–04:15, 15-second spacing; batch 2 four at 04:57 plus 3 Claudes at 04:54–04:55; Pangu separately. 6–7 concurrent sessions slowed local tests; later exclusive timing authoritative.
- **Effort:** defaults; OpenCode no explicit setting/actual values unrecorded; all 3 Claude logs medium.

### Contestants

| Model | Tool | Model ID / variant | Access | Start | Minutes |
|---|---|---|---|---|---|
| StepFun Step 5 preview | OpenCode 2.0.19 | `openrouter/stepfun/step-5-preview` | OpenRouter | 04:57:41 | 45.0, deadline, note 3 |
| Tencent Hunyuan Hy4 preview | OpenCode 2.0.19 | `openrouter/tencent/hy4-preview` | OpenRouter | 04:57:26 | 45.0, deadline, note 3 |
| Qwen3.8-27B | OpenCode 2.0.19 | `openrouter/qwen/qwen3.8-27b` | OpenRouter | 04:15:31 | 45.0, forced |
| Inception Mercury 3 | OpenCode 2.0.19 | `openrouter/inception/mercury-3` | OpenRouter | 04:57:56 | 45.0, deadline, note 3 |
| ERNIE 5.1 | OpenCode 2.0.19 | `qianfan/ernie-5.1` | Baidu Qianfan | 04:14:46 | 36.4, self-finished |
| Mistral Large 4 | OpenCode 2.0.19 | `openrouter/mistralai/mistral-large-4-0` | OpenRouter | 04:57:11 | 45.0, deadline, note 3 |
| Xiaohongshu dots3-note preview | OpenCode 2.0.19 | `dots/dots3-note-prev` | dots official API | 04:14:31 | 45.0, forced |
| openPangu-2.0-Pro | OpenCode 2.0.19 | `huawei-maas/openpangu-2.0-pro` | Huawei Cloud MaaS | 05:16:56 | 45.0, forced; credit 7 min 36 s |
| Meituan LongCat-2.0 | OpenCode 2.0.19 | `openrouter/meituan/longcat-2.0` | OpenRouter | 04:15:16 | 45.0, forced; credit 1 min 44 s |
| Doubao-Seed-Evolving | OpenCode 2.0.19 | `volcengine/doubao-seed-evolving` | Volcano Ark agent plan | 04:15:01 | 45.0, forced, not timed; episode 5 carried over |
| Claude Sonnet 5.5 | Claude Code 2.1.301, manual | `claude-sonnet-5-5`, medium | Anthropic subscription | 04:54:47 | 26.0, self-finished |
| Claude Opus 5.5 | Claude Code 2.1.301, manual | `claude-opus-5-5`, medium | Anthropic subscription | 04:55:08 | 32.7, self-finished |
| Claude Haiku 5.5 | Claude Code 2.1.301, manual | `claude-haiku-5-5`, medium | Anthropic subscription | 04:55:31 | 33.8, self-finished |

- **Time definition:** prompt→submission, last output for self-finish or 45 minutes for deadline, minus credits. `results/ep7/汇总.md` raw script collection times differ for notes 3–4.
- **Note 1, Doubao:** user on 10-20 excluded new submission (single `main.cpp`, deadline collection) from timing/ranking; episode 5 lead result used. New code untested/omitted.
- **Note 2, Claude:** user omitted start-timer command; organizer later registered `workspace/` creation/extraction time, making clocks ~10 seconds short. All finished before 45 minutes, batch-collected at 05:41; durations corrected from prompt/last-reply timestamps.
- **Note 3, missed deadlines:** Step/Hunyuan/Mercury/Mistral due at 05:42, not killed (monitor restarted at 05:22; failure reason unrecorded). Manual collection at 05:54 Step/Mercury,05:59 Hunyuan/Mistral; post-deadline `src/` changes reverted from event logs. Hunyuan: 3 `sim.cpp` edits 3–24 seconds late and one `HANDOFF.md` sed edit >3 minutes late, reversed; five restored files byte-match replay of all pre-deadline writes/edits. Late backups omitted. Mercury: `HANDOFF.md` created 6–12 seconds late, deleted; others predeadline. Mistral: hand-off created ~8.5 minutes late, deleted; other source unchanged baseline. Step: no late source edits, only tests/scripts, no action.
- **Note 4, Mistral:** all files read by minute 2, baseline timing minutes 15–22, repeated-baseline command ran until 43; then one Decode error and 3 `Upstream idle timeout exceeded` errors. At 05:09 monitor warned 10 minutes without output, reminder only; user neither resumed nor credited. Final source identical baseline.
- **Note 5, access:** six OpenRouter models shared an account; upstream routing unrecorded. IDs/channels match Tron.

### Interruptions and incidents

| Who | Event |
|---|---|
| LongCat | At 13 min 50 s called OpenCode question tool; unanswered unattended session exited 1 without error. User chose resume at 04:30:48 in original session with explanation that organizer answers no questions, interruption not its fault, time credited, decide independently and continue. Credit 1 min 44 s from last output, forced stop at 05:02. Question tool disabled for later sessions |
| Pangu | Starts at 05:03/05:10 failed within 10 seconds:403 `provider.auth`, “The resource is no longer available”, no work, voided. Third start at 05:16:56 succeeded. ~05:48, minute 31, external action killed python/opencode processes together; organizer classified external interruption. Resumed original session at 05:55 with accidental-interruption/time-credit message;7 min 36 s credited, forced stop at 06:09 |
| Four batch 2 OpenCode | Automatic stop missed, late changes reverted, note 3 |
| Mistral | Long wait on own timing command, late OpenRouter transport errors, no credit, note 4 |
| Dots, Mercury, Wenxin | Nested `workspace/workspace/`; collector found it, scores unaffected |
| Mistral, Dots | All 5 source-file SHA-256 hashes identical to baseline; user on 10-20 ruled identical: 8 correct, ~1×, counterexamples recorded as baseline-identical, no separate run |

### Evaluation

- **Unified timing:** same PC, no contestant sessions. w64devkit g++ 16.2 `-O2 -std=c++20 -static`, top-level `src/*.cpp` only, total CPU, 1 GB, same 8 hidden workloads as episode 5. Median of 3, only 1 if >10 CPU seconds; if 3 total <2 CPU seconds, repeat to >2, max 80, mean. Timeout max(180 seconds, baseline wall×3+10). All 8 correct required for geo-mean.
- **Screening:** one `hid_lowbattery` (baseline 33.5 CPU s); ≤1.05× means no faster, run 8 once for correctness in 8-way parallel, untimed. If those yield geo-mean>1.05, overturn screening and run full. Screened Pangu unfinished at 180 s, Dots 0.74×, Mistral 0.95×; latter two later baseline-identical ruling.
- **Reused baseline:** 10-17 same-PC internal evaluation CPU times, no remeasurement. Baseline 10-06 vs. 10-17 differences −3.8%…+5.0%, geo-mean difference 0.65%. Retimed two 10-17 entries here to assess state drift: ratios 1.027 and 0.985.
- **Background load:** Astra inspection 1 concurrent, max 2 target programs, no performance tests. Timing script recorded background use on 32-core machine, reran >2.5 cores. Across 409 timings median 0.85, max 4.43 cores; no unresolved contaminated measurement remained.
- **Reused rows:** Doubao ranking and 14 episode 5/internal 10-17 rows in `results/ep7/汇总.md` reuse 10-17 measurements.
- **LongCat:** compile failed in unified timing and later separate counterexample-pool run.

### Defects and counterexamples

- Two GPT-6 Astra Codex CLI inspections of 12 timed entries, excluding new Doubao: max/~17 minutes correctness-only, ultra/~46 minutes timeout focus, subagents allowed in second. Tasks/reports `packages/ep7/astra-review/`.
- **Independent verification required:** organizer reran every candidate under episode 5 R2 rules: valid, baseline ≤10 s, first target 100 s, completion in 60–100 triggers two 60 s runs, all must timeout. 53 candidates, 50 legal, 18 confirmed: Haiku 3, Mercury 4, Pangu 7, Qwen 4. Accepted run on 10-20 22:59–23:42, initial background ~0.7 cores under 1.5 threshold; earlier verification voided.
- Two of 18 (one Pangu/one Qwen) invalidated by baseline 9.9 CPU seconds but wall>10 during unified rerun; excluded from counts, retained in `results/ep7/counterexamples/excluded/`. Remaining 16 plus 10 carried-over pool cases rerun against actual ranking entries, completed on 10-21 00:48. Claudes/reference rows only partial coverage.
- User chose findings recorded without rank changes. Timeout findings hardware dependent.
- Pangu row's “awaiting independent verification” footnote reflects table-writing time; later confirmed.

### Omitted

Model sessions/event logs; organizer launch/timing/evaluation/inspection scripts and operation sheets with local paths/proxy/account details; late-state backups; two voided Pangu starts; Astra working directories (~180 MB builds/intermediate inputs/logs/history); internal 10-17 code/materials.
