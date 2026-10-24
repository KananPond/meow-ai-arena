[简体中文](RESULTS_正式榜.md)

# Tron (episode 7 official tournament) · Official board

10 official contestants, Doubao anchors 0. greedy is unscored/unranked seat filler; extra greedy copies fill the 12-player field and are also unscored.

Generated 2026-10-20 07:17:26. 10 ranked contestants: Wenxin, Pangu, Dots, LongCat, Qwen27B, Doubao, Mistral, Hunyuan, Step, Mercury. **Doubao anchors 0**, ranked normally, no interval; all other scores, intervals and rank intervals are relative to it. greedy and 12-player filler copies are unscored/unranked; all comparisons involving them, including 1v1, are excluded.

Scoring follows RULES.md section 3: separate Bradley–Terry ratings for 1v1, pooled 3–4-player, and 12-player games. Pairwise comparisons between listed programs exclude greedy/fillers: better place wins (1), same-turn deaths draw (0.5 each). Add one virtual draw per pair. Doubao = 0; 10× stronger = +400 (Elo scale, 400-point gap ≈ 10:1). Total weights: 1v1 50%, 3–4 players 30%, 12 players 20%.

Intervals: 1000 stratified block-bootstrap resamples by type. Blocks: both seat-swapped games of a pair/map in 1v1; a table’s seat rotation in 3–4-player; one game in 12-player. Imported previews use the same blocking. Resample all three types together, refit separately and weight; 95% intervals. Reset Doubao to 0 each resample, so all intervals are relative to Doubao.

Places are 95% bootstrap rank intervals: rerank by total in each resample among contestants only (reference line unranked, Doubao ranked), then take 2.5%/97.5% quantiles. Show “a–b” or a single place if a=b. Distinguishability uses whether the pair’s total-difference 95% interval contains 0; it is pairwise, not transitive (A–B and B–C indistinguishable does not imply A–C). See section 2.

## 1. Total

Reference line is inserted by total.

| Place | Contestant | Total | 95% interval | 1v1 | 3–4 players | 12 players | Program played |
|---|---|---:|---|---:|---:|---:|---|
| Place 1 | Mistral Large 4 (Mistral) | 102 | 63 ~ 147 | 363 | -191 | -113 | submitted version |
| Place 2–3 | Meituan LongCat-2.0 (LongCat) | 10 | -24 ~ 50 | 261 | -271 | -198 | submitted version |
| Place 2–4 | **Doubao-Seed-Evolving (Doubao) (anchor, 0 points)** | 0 | — | 0 | 0 | 0 | submitted version |
| Place 3–4 | StepFun Step 5 preview (Step) | -25 | -60 ~ 11 | 13 | -61 | -64 | submitted version |
| Place 5 | openPangu-2.0-Pro (Pangu) | -208 | -242 ~ -175 | -170 | -199 | -316 | submitted version |
| Place 6 | Tencent Hunyuan Hy4 preview (Hunyuan) | -240 | -280 ~ -203 | -352 | -258 | 66 | submitted version |
| Place 7–9 | Qwen3.8-27B (Qwen27B) | -290 | -325 ~ -253 | -363 | -239 | -185 | submitted version |
| Place 7–9 | Inception Mercury 3 (diffusion model) (Mercury) | -297 | -338 ~ -262 | -384 | -201 | -222 | submitted version |
| Place 7–9 | ERNIE 5.1 (Wenxin) | -304 | -341 ~ -271 | -343 | -263 | -271 | submitted version |
| Place 10 | Xiaohongshu dots3-note preview (Dots) | -440 | -480 ~ -403 | -510 | -335 | -424 | submitted version |

## 2. Pairwise comparisons

Indistinguishable pairs among ranked contestants (95% total-difference interval includes 0):

| Contestant | Place | Indistinguishable from |
|---|---|---|
| Mistral | Place 1 |  (distinct from everyone) |
| LongCat | Place 2–3 | Doubao |
| Doubao | Place 2–4 | LongCat, Step |
| Step | Place 3–4 | Doubao |
| Pangu | Place 5 |  (distinct from everyone) |
| Hunyuan | Place 6 |  (distinct from everyone) |
| Qwen27B | Place 7–9 | Mercury, Wenxin |
| Mercury | Place 7–9 | Qwen27B, Wenxin |
| Wenxin | Place 7–9 | Qwen27B, Mercury |
| Dots | Place 10 |  (distinct from everyone) |

Total difference (row − column). ≈ means 95% difference interval includes 0; unmarked pairs are distinguishable.

| Row − column | Mistral | LongCat | Doubao | Step | Pangu | Hunyuan | Qwen27B | Mercury | Wenxin | Dots |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Mistral | · | 92 | 102 | 126 | 310 | 342 | 392 | 398 | 406 | 542 |
| LongCat | -92 | · | 10 ≈ | 34 | 218 | 250 | 299 | 306 | 314 | 450 |
| Doubao | -102 | -10 ≈ | · | 25 ≈ | 208 | 240 | 290 | 297 | 304 | 440 |
| Step | -126 | -34 | -25 ≈ | · | 183 | 216 | 265 | 272 | 280 | 416 |
| Pangu | -310 | -218 | -208 | -183 | · | 32 | 82 | 89 | 96 | 232 |
| Hunyuan | -342 | -250 | -240 | -216 | -32 | · | 50 | 57 | 64 | 200 |
| Qwen27B | -392 | -299 | -290 | -265 | -82 | -50 | · | 7 ≈ | 15 ≈ | 150 |
| Mercury | -398 | -306 | -297 | -272 | -89 | -57 | -7 ≈ | · | 8 ≈ | 143 |
| Wenxin | -406 | -314 | -304 | -280 | -96 | -64 | -15 ≈ | -8 ≈ | · | 136 |
| Dots | -542 | -450 | -440 | -416 | -232 | -200 | -150 | -143 | -136 | · |

Adjacent-place difference intervals:

| Higher | Lower | Total difference | 95% interval of difference | Decision |
|---|---|---:|---|---|
| Mistral | LongCat | 92 | 59 ~ 132 | Distinguishable |
| LongCat | Doubao | 10 | -24 ~ 50 | Indistinguishable (interval includes 0) |
| Doubao | Step | 25 | -11 ~ 60 | Indistinguishable (interval includes 0) |
| Step | Pangu | 183 | 156 ~ 215 | Distinguishable |
| Pangu | Hunyuan | 32 | 4 ~ 65 | Distinguishable |
| Hunyuan | Qwen27B | 50 | 17 ~ 79 | Distinguishable |
| Qwen27B | Mercury | 7 | -23 ~ 37 | Indistinguishable (interval includes 0) |
| Mercury | Wenxin | 8 | -21 ~ 38 | Indistinguishable (interval includes 0) |
| Wenxin | Dots | 136 | 106 ~ 166 | Distinguishable |

## 3. Per-type BT ratings

### 1v1 (weight  50% ; pairwise comparisons:  2087)

| Contestant | Rating | 95% interval |
|---|---:|---|
| Mistral Large 4 (Mistral) | 363 | 293 ~ 446 |
| Meituan LongCat-2.0 (LongCat) | 261 | 199 ~ 337 |
| StepFun Step 5 preview (Step) | 13 | -49 ~ 76 |
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| openPangu-2.0-Pro (Pangu) | -170 | -233 ~ -113 |
| ERNIE 5.1 (Wenxin) | -343 | -411 ~ -281 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -352 | -426 ~ -282 |
| Qwen3.8-27B (Qwen27B) | -363 | -431 ~ -298 |
| Inception Mercury 3 (diffusion model) (Mercury) | -384 | -455 ~ -318 |
| Xiaohongshu dots3-note preview (Dots) | -510 | -586 ~ -443 |

### 3–4 players (weight  30% ; pairwise comparisons:  3636)

| Contestant | Rating | 95% interval |
|---|---:|---|
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| StepFun Step 5 preview (Step) | -61 | -111 ~ -14 |
| Mistral Large 4 (Mistral) | -191 | -239 ~ -146 |
| openPangu-2.0-Pro (Pangu) | -199 | -250 ~ -150 |
| Inception Mercury 3 (diffusion model) (Mercury) | -201 | -255 ~ -147 |
| Qwen3.8-27B (Qwen27B) | -239 | -282 ~ -197 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -258 | -308 ~ -213 |
| ERNIE 5.1 (Wenxin) | -263 | -308 ~ -219 |
| Meituan LongCat-2.0 (LongCat) | -271 | -320 ~ -225 |
| Xiaohongshu dots3-note preview (Dots) | -335 | -377 ~ -297 |

### 12 players (weight  20% ; pairwise comparisons:  19440)

| Contestant | Rating | 95% interval |
|---|---:|---|
| Tencent Hunyuan Hy4 preview (Hunyuan) | 66 | 37 ~ 97 |
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| StepFun Step 5 preview (Step) | -64 | -93 ~ -32 |
| Mistral Large 4 (Mistral) | -113 | -145 ~ -80 |
| Qwen3.8-27B (Qwen27B) | -185 | -215 ~ -154 |
| Meituan LongCat-2.0 (LongCat) | -198 | -230 ~ -168 |
| Inception Mercury 3 (diffusion model) (Mercury) | -222 | -264 ~ -186 |
| ERNIE 5.1 (Wenxin) | -271 | -299 ~ -239 |
| openPangu-2.0-Pro (Pangu) | -316 | -347 ~ -285 |
| Xiaohongshu dots3-note preview (Dots) | -424 | -453 ~ -398 |

## 4. 1v1 directly against sparring_hard (reference, not separately scored)

Seat-swapped 1v1 games against sparring_hard, including imported previews. If this board excludes sparring_hard, these games do not count in its rating. Score rate = mean rank points: win 1, same-turn death 0.5, loss 0.

| Contestant | Games | W | Same-turn death | L | Score rate | Mean turns |
|---|---:|---:|---:|---:|---:|---:|
| Mistral Large 4 (Mistral) | 48 | 17 | 2 | 29 | 37.5% | 148.2 |
| Meituan LongCat-2.0 (LongCat) | 48 | 8 | 0 | 40 | 16.7% | 126.9 |
| StepFun Step 5 preview (Step) | 48 | 2 | 1 | 45 | 5.2% | 124.2 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | 48 | 1 | 0 | 47 | 2.1% | 76.3 |
| ERNIE 5.1 (Wenxin) | 48 | 0 | 0 | 48 | 0.0% | 52.0 |
| openPangu-2.0-Pro (Pangu) | 48 | 0 | 0 | 48 | 0.0% | 78.0 |
| Xiaohongshu dots3-note preview (Dots) | 48 | 0 | 0 | 48 | 0.0% | 52.4 |
| Qwen3.8-27B (Qwen27B) | 48 | 0 | 0 | 48 | 0.0% | 57.3 |
| Doubao-Seed-Evolving (Doubao) | 48 | 0 | 0 | 48 | 0.0% | 113.8 |
| Inception Mercury 3 (diffusion model) (Mercury) | 40 | 0 | 0 | 40 | 0.0% | 71.7 |

## 5. Compilation, overruns, crashes

Only newly played formal games on this board. Imported fast previews with overruns were discarded entirely, so are absent here. An over-budget turn exceeds CPU budget (2000 first turn, 50 CPU ms thereafter) or wall safeguard; bot continues in previous direction.

| Contestant | Compile | Games | Over-budget turns | Games with overruns | Invalid | Games with crashes | First-turn max | Per-turn mean | Per-turn max |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ERNIE 5.1 (Wenxin) | Passed | 1084 | 0 | 0 | 0 | 0 | 6 | 0.1 | 0.5 |
| openPangu-2.0-Pro (Pangu) | Passed | 1084 | 0 | 0 | 0 | 0 | 8 | 0.1 | 1.8 |
| Xiaohongshu dots3-note preview (Dots) | Passed | 1084 | 0 | 0 | 0 | 0 | 98 | 0.2 | 4.0 |
| Meituan LongCat-2.0 (LongCat) | Passed | 1084 | 0 | 0 | 0 | 0 | 1999 | 13.0 | 48.7 |
| Qwen3.8-27B (Qwen27B) | Passed | 1084 | 0 | 0 | 0 | 0 | 501 | 9.4 | 26.9 |
| Doubao-Seed-Evolving (Doubao) | Passed | 1084 | 14 | 14 | 0 | 0 | 1615 | 9.4 | 51.6 |
| Mistral Large 4 (Mistral) | Passed | 1084 | 0 | 0 | 0 | 0 | 1991 | 15.7 | 41.9 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | Passed | 1084 | 0 | 0 | 0 | 0 | 1994 | 24.2 | 43.9 |
| StepFun Step 5 preview (Step) | Passed | 1080 | 0 | 0 | 0 | 0 | 1984 | 12.8 | 34.7 |
| Inception Mercury 3 (diffusion model) (Mercury) | Passed | 812 | 0 | 0 | 0 | 0 | 1998 | 8.1 | 49.0 |

## 6. Time used

Recommended 45 minutes, forced collection at 60. Last edit = sealed bot.cpp modification timestamp minus start, a file-time reference only.

| Contestant | Tool | Sealing method | Start to seal | Last edit | Recheck | sha256 |
|---|---|---|---:|---:|---|---|
| ERNIE 5.1 (Wenxin) | opencode qianfan/ernie-5.1 | Manual after submission | 38.4 minutes | at minute 21.5 | Unchanged | 3d127342e7bc |
| openPangu-2.0-Pro (Pangu) | opencode huawei-maas/openpangu-2.0-pro | Manual after submission | 44.6 minutes | at minute 39.3 | Unchanged | 8bad2945631c |
| Xiaohongshu dots3-note preview (Dots) | opencode dots/dots3-note-prev | Forced deadline seal | 60.1 minutes | at minute 57.2 | Unchanged | cf31f2cf6361 |
| Meituan LongCat-2.0 (LongCat) | opencode openrouter/meituan/longcat-2.0 | Forced deadline seal | 60.0 minutes | at minute 53.1 | Unchanged | f884001d6327 |
| Qwen3.8-27B (Qwen27B) | opencode openrouter/qwen/qwen3.8-27b | Forced deadline seal | 60.1 minutes | at minute 38.1 | Unchanged | ab274da44dd1 |
| Doubao-Seed-Evolving (Doubao) | opencode volcengine/doubao-seed-evolving | Forced deadline seal | 60.0 minutes | at minute 58.9 | **Changed after sealing** | ad36b8a09bac |
| Mistral Large 4 (Mistral) | opencode openrouter/mistralai/mistral-large-4-0 | Manual after submission | 59.9 minutes | at minute 56.6 | Unchanged | ce0defddd8b6 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | opencode openrouter/tencent/hy4-preview | Manual after submission | 59.8 minutes | at minute 53.1 | Unchanged | 09142c877d0e |
| StepFun Step 5 preview (Step) | opencode openrouter/stepfun/step-5-preview | Manual after submission | 59.7 minutes | at minute 34.1 | Unchanged | b26e29889fef |
| Inception Mercury 3 (diffusion model) (Mercury) | opencode openrouter/inception/mercury-3 | Forced deadline seal | 60.1 minutes | at minute 33.7 | Unchanged | 45dfa785d1c5 |

## 7. Schedule, sources and environment

- 1v1 is shared by all boards. Only Mercury pairings added: 20 maps against every other round-robin program, same map set as prior final, both seats, 480 new games. Others imported from the prior final, also 20 maps/both seats. This board uses 360 new games among its 10 programs.
- greedy fills multiplayer seats only, no 1v1 round robin, ratings, pairwise comparisons or leaderboard entry. Same policy as previews. Doubao anchors 0.
- 4-player additions: 53 tables including Mercury; 3 other seats drawn from the remaining 10 programs, least appearances first, random ties. One new map/table, 4 seat rotations, 212 games. Other 4-player games imported.
- 12-player additions: 20 maps, 11 programs plus 1 greedy copy (`greedy_fill`). Filler comparisons excluded, but filler occupies a rank-point seat. Random initial seating then 12 full rotations/map: 240 games. All new-field programs participate; prior final games also imported.
- 3p is unscored: not played in final and discarded from imported previews. The 3–4-player component uses 4-player games only.
- Imported prior final (`final1018`, formal): engine/compiler checked against its `清单.json` and sources against `程序.json`. Overrun games retained because formal penalties already applied. Multiplayer games enter only their original board, retaining original bootstrap blocks.
- Imports (table below): 1v1 requires both programs on board; multiplayer from `preview1/multi_formal`, `preview2/multi_formal`, `final1018/results` requires all non-fillers on board (corresponding preview policy). Preview 12-player greedy copies follow the same filler rules. New games avoid all imported seeds, except shared 1v1 deliberately reuses previous maps for previously unplayed pairs: no duplicate game.
- Schedule fingerprint a5ccca4ecec5e645…; table seed 7007 (each board uses table_seed + board name). Map seeds in `赛程.json` (unpublished at the time; released schedules are now in `../tournament/`).

Scored games on this board by source:

| Source | 1v1 | 4p | 12p | Total |
|---|---:|---:|---:|---:|
| New final games | 360 | 212 | 240 | 812 |
| final1018 (10-18 formal final, no Mercury) | 1440 | 480 | 228 | 2148 |
| preview1 | 286 | 32 | 12 | 330 |
| preview2 | 1 | 20 | 0 | 21 |
| Total | 2087 | 744 | 480 | 3311 |

Games played / scored pairwise comparisons per contestant, indicating coverage; excludes fillers, outsiders and greedy:

| Contestant | 1v1 | 4p | 12p |
|---|---:|---:|---:|
| Wenxin | 424 / 424 | 276 / 716 | 480 / 4080 |
| Pangu | 424 / 424 | 276 / 740 | 480 / 4080 |
| Dots | 424 / 424 | 276 / 728 | 480 / 4080 |
| LongCat | 424 / 424 | 280 / 732 | 480 / 4080 |
| Qwen27B | 423 / 423 | 280 / 784 | 480 / 4080 |
| Doubao | 423 / 423 | 276 / 760 | 480 / 4080 |
| Mistral | 424 / 424 | 276 / 772 | 480 / 4080 |
| Hunyuan | 424 / 424 | 276 / 720 | 480 / 4080 |
| Step | 424 / 424 | 272 / 748 | 480 / 4080 |
| Mercury | 360 / 360 | 212 / 572 | 240 / 2160 |

Import preview1: read 600, retained 598 across boards; discarded 2 overrun games (fast mode has no penalties, unlike formal). Package and all source hashes match.

Import preview2: read 826, retained 387 across boards; discarded 1 overrun game and 438 duplicates of earlier sources (first copy retained). Package and all source hashes match.

Import final1018: read 4267, retained 4267 across boards; discarded none. Package and all source hashes match.

- CPU affinity: [0, 2, 4, 6, 8, 10, 12, 14]; 12-player: [0, 2, 4, 6, 8, 10, 12, 14]. Power plan GUID: 381b4222-f694-41f0-9685-ff5bb260df2e (Balanced).
- Cycles per CPU millisecond measured by referee: 2419200.
- Final 12p across both boards: 534 games, 53.3 minutes in this run.
- Final 4p across both boards: 432 games, 6.8 minutes in this run.
- Final 1v1 across both boards: 480 games, 6.3 minutes in this run.

Original event output paths: `回放\` (shared `回放\1v1\`, this board’s multiplayer `回放\正式榜\`, index `回放\INDEX.md`). Per-game data: `results.jsonl` (new final), `预览并入.jsonl`; summary in `results.json` → `boards`; video statistics in `视频统计_正式榜.md`.

## Supplement: rank points (old scoring, reference only, no placement)

Normalize rank points as (N − place)/(N − 1), averaging same-turn ties, with fillers occupying seats normally. Average by type, subtract Doubao’s mean, ×100, then use the same weights. 95% intervals from the same 1000 stratified resamples.

| Contestant | Rank-point total | 95% interval | 1v1 | 3–4 players | 12 players |
|---|---:|---|---:|---:|---:|
| Mistral Large 4 (Mistral) | +2.5 | [-0.7, +5.8] | +26.5 | -26.6 | -13.7 |
| Doubao-Seed-Evolving (Doubao) | +0.0 | [+0.0, +0.0] | +0.0 | +0.0 | +0.0 |
| StepFun Step 5 preview (Step) | -3.4 | [-7.6, +0.8] | +1.3 | -8.4 | -7.5 |
| Meituan LongCat-2.0 (LongCat) | -6.0 | [-9.6, -2.5] | +20.5 | -37.5 | -25.0 |
| openPangu-2.0-Pro (Pangu) | -24.7 | [-29.3, -20.3] | -16.8 | -27.2 | -40.5 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -27.9 | [-32.1, -23.9] | -36.4 | -36.8 | +6.6 |
| Qwen3.8-27B (Qwen27B) | -33.5 | [-37.3, -29.5] | -37.5 | -33.6 | -23.5 |
| Inception Mercury 3 (diffusion model) (Mercury) | -33.6 | [-38.0, -29.6] | -39.5 | -26.9 | -29.1 |
| ERNIE 5.1 (Wenxin) | -35.1 | [-39.2, -31.1] | -35.4 | -35.1 | -34.3 |
| Xiaohongshu dots3-note preview (Dots) | -50.9 | [-54.5, -47.7] | -51.7 | -48.3 | -52.6 |
