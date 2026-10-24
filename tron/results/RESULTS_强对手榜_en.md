[简体中文](RESULTS_强对手榜.md)

# Tron (episode 7 official tournament) · Strong-opponent board

Performance against stronger opponents. Doubao anchors 0. sparring_hard participates normally and is fitted but holds no place, serving as the hard sparring line. greedy fills seats, unscored and unranked. The 12-player field fills exactly.

Generated 2026-10-20 07:17:38. 12 ranked contestants: Wenxin, Pangu, Dots, LongCat, Qwen27B, Doubao, Mistral, Hunyuan, Step, Mercury, DSNew, AstraUF. **Doubao anchors 0**, ranked normally, no interval; all other scores, intervals and rank intervals are relative to it. greedy and 12-player filler copies are unscored/unranked; all comparisons involving them, including 1v1, are excluded. sparring_hard plays and is fitted as an unranked reference line.

Scoring follows RULES.md section 3: separate Bradley–Terry ratings for 1v1, pooled 3–4-player, and 12-player games. Pairwise comparisons between listed programs exclude greedy/fillers: better place wins (1), same-turn deaths draw (0.5 each). Add one virtual draw per pair. Doubao = 0; 10× stronger = +400 (Elo scale, 400-point gap ≈ 10:1). Total weights: 1v1 50%, 3–4 players 30%, 12 players 20%.

Intervals: 1000 stratified block-bootstrap resamples by type. Blocks: both seat-swapped games of a pair/map in 1v1; a table’s seat rotation in 3–4-player; one game in 12-player. Imported previews use the same blocking. Resample all three types together, refit separately and weight; 95% intervals. Reset Doubao to 0 each resample, so all intervals are relative to Doubao.

Places are 95% bootstrap rank intervals: rerank by total in each resample among contestants only (reference line unranked, Doubao ranked), then take 2.5%/97.5% quantiles. Show “a–b” or a single place if a=b. Distinguishability uses whether the pair’s total-difference 95% interval contains 0; it is pairwise, not transitive (A–B and B–C indistinguishable does not imply A–C). See section 2.

## 1. Total

Reference line is inserted by total.

| Place | Contestant | Total | 95% interval | 1v1 | 3–4 players | 12 players | Program played |
|---|---|---:|---|---:|---:|---:|---|
| — (hard sparring line) | **sparring_hard (hard sparring line)** | 324 | 290 ~ 363 | 487 | 179 | 135 | sparring program |
| Place 1 | GPT-6 Astra (ultra + fast) (AstraUF) | 313 | 279 ~ 352 | 418 | 239 | 162 | submitted version |
| Place 2–3 | DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | 128 | 95 ~ 164 | 277 | 9 | -67 | submitted version |
| Place 2–3 | Mistral Large 4 (Mistral) | 120 | 86 ~ 155 | 383 | -160 | -119 | submitted version |
| Place 4–6 | Meituan LongCat-2.0 (LongCat) | 13 | -21 ~ 49 | 240 | -241 | -174 | submitted version |
| Place 4–6 | **Doubao-Seed-Evolving (Doubao) (anchor, 0 points)** | 0 | — | 0 | 0 | 0 | submitted version |
| Place 4–6 | StepFun Step 5 preview (Step) | -9 | -40 ~ 25 | 20 | -38 | -37 | submitted version |
| Place 7–8 | openPangu-2.0-Pro (Pangu) | -193 | -225 ~ -161 | -167 | -176 | -283 | submitted version |
| Place 7–8 | Tencent Hunyuan Hy4 preview (Hunyuan) | -221 | -260 ~ -181 | -340 | -228 | 91 | submitted version |
| Place 9–11 | Qwen3.8-27B (Qwen27B) | -265 | -301 ~ -233 | -355 | -194 | -147 | submitted version |
| Place 9–11 | Inception Mercury 3 (diffusion model) (Mercury) | -276 | -314 ~ -241 | -373 | -183 | -172 | submitted version |
| Place 9–11 | ERNIE 5.1 (Wenxin) | -276 | -312 ~ -244 | -332 | -212 | -233 | submitted version |
| Place 12 | Xiaohongshu dots3-note preview (Dots) | -408 | -448 ~ -373 | -493 | -294 | -365 | submitted version |

### Against reference line

Contestant total minus reference total (95% difference interval in parentheses); ≈ means interval includes 0, indistinguishable.

| Contestant | Place | Minus sparring_hard (hard sparring line) |
|---|---|---|
| AstraUF | Place 1 | -11 (-46 ~ 24) ≈ |
| DSNew | Place 2–3 | -197 (-232 ~ -165) |
| Mistral | Place 2–3 | -204 (-237 ~ -174) |
| LongCat | Place 4–6 | -312 (-347 ~ -278) |
| Doubao | Place 4–6 | -324 (-363 ~ -290) |
| Step | Place 4–6 | -333 (-368 ~ -301) |
| Pangu | Place 7–8 | -518 (-557 ~ -485) |
| Hunyuan | Place 7–8 | -545 (-586 ~ -508) |
| Qwen27B | Place 9–11 | -590 (-630 ~ -557) |
| Mercury | Place 9–11 | -600 (-640 ~ -565) |
| Wenxin | Place 9–11 | -601 (-639 ~ -567) |
| Dots | Place 12 | -732 (-774 ~ -696) |

## 2. Pairwise comparisons

Indistinguishable pairs among ranked contestants (95% total-difference interval includes 0):

| Contestant | Place | Indistinguishable from |
|---|---|---|
| AstraUF | Place 1 |  (distinct from everyone) |
| DSNew | Place 2–3 | Mistral |
| Mistral | Place 2–3 | DSNew |
| LongCat | Place 4–6 | Doubao, Step |
| Doubao | Place 4–6 | LongCat, Step |
| Step | Place 4–6 | LongCat, Doubao |
| Pangu | Place 7–8 | Hunyuan |
| Hunyuan | Place 7–8 | Pangu |
| Qwen27B | Place 9–11 | Mercury, Wenxin |
| Mercury | Place 9–11 | Qwen27B, Wenxin |
| Wenxin | Place 9–11 | Qwen27B, Mercury |
| Dots | Place 12 |  (distinct from everyone) |

Total difference (row − column). ≈ means 95% difference interval includes 0; unmarked pairs are distinguishable.

| Row − column | AstraUF | DSNew | Mistral | LongCat | Doubao | Step | Pangu | Hunyuan | Qwen27B | Mercury | Wenxin | Dots | sparring_hard |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| AstraUF | · | 186 | 193 | 300 | 313 | 322 | 506 | 534 | 578 | 589 | 589 | 721 | -11 ≈ |
| DSNew | -186 | · | 8 ≈ | 115 | 128 | 136 | 321 | 348 | 393 | 403 | 404 | 535 | -197 |
| Mistral | -193 | -8 ≈ | · | 107 | 120 | 128 | 313 | 340 | 385 | 396 | 396 | 528 | -204 |
| LongCat | -300 | -115 | -107 | · | 13 ≈ | 21 ≈ | 206 | 233 | 278 | 289 | 289 | 420 | -312 |
| Doubao | -313 | -128 | -120 | -13 ≈ | · | 9 ≈ | 193 | 221 | 265 | 276 | 276 | 408 | -324 |
| Step | -322 | -136 | -128 | -21 ≈ | -9 ≈ | · | 185 | 212 | 257 | 267 | 268 | 399 | -333 |
| Pangu | -506 | -321 | -313 | -206 | -193 | -185 | · | 27 ≈ | 72 | 83 | 83 | 215 | -518 |
| Hunyuan | -534 | -348 | -340 | -233 | -221 | -212 | -27 ≈ | · | 45 | 55 | 56 | 187 | -545 |
| Qwen27B | -578 | -393 | -385 | -278 | -265 | -257 | -72 | -45 | · | 11 ≈ | 11 ≈ | 143 | -590 |
| Mercury | -589 | -403 | -396 | -289 | -276 | -267 | -83 | -55 | -11 ≈ | · | 0 ≈ | 132 | -600 |
| Wenxin | -589 | -404 | -396 | -289 | -276 | -268 | -83 | -56 | -11 ≈ | 0 ≈ | · | 132 | -601 |
| Dots | -721 | -535 | -528 | -420 | -408 | -399 | -215 | -187 | -143 | -132 | -132 | · | -732 |
| sparring_hard | 11 ≈ | 197 | 204 | 312 | 324 | 333 | 518 | 545 | 590 | 600 | 601 | 732 | · |

Adjacent-place difference intervals:

| Higher | Lower | Total difference | 95% interval of difference | Decision |
|---|---|---:|---|---|
| AstraUF | DSNew | 186 | 153 ~ 220 | Distinguishable |
| DSNew | Mistral | 8 | -22 ~ 39 | Indistinguishable (interval includes 0) |
| Mistral | LongCat | 107 | 76 ~ 138 | Distinguishable |
| LongCat | Doubao | 13 | -21 ~ 49 | Indistinguishable (interval includes 0) |
| Doubao | Step | 9 | -25 ~ 40 | Indistinguishable (interval includes 0) |
| Step | Pangu | 185 | 158 ~ 216 | Distinguishable |
| Pangu | Hunyuan | 27 | -4 ~ 58 | Indistinguishable (interval includes 0) |
| Hunyuan | Qwen27B | 45 | 13 ~ 77 | Distinguishable |
| Qwen27B | Mercury | 11 | -18 ~ 38 | Indistinguishable (interval includes 0) |
| Mercury | Wenxin | 0 | -27 ~ 28 | Indistinguishable (interval includes 0) |
| Wenxin | Dots | 132 | 103 ~ 163 | Distinguishable |

## 3. Per-type BT ratings

### 1v1 (weight  50% ; pairwise comparisons:  3647)

| Contestant | Rating | 95% interval |
|---|---:|---|
| sparring_hard (hard sparring line) | 487 | 429 ~ 555 |
| GPT-6 Astra (ultra + fast) (AstraUF) | 418 | 361 ~ 484 |
| Mistral Large 4 (Mistral) | 383 | 326 ~ 448 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | 277 | 221 ~ 337 |
| Meituan LongCat-2.0 (LongCat) | 240 | 188 ~ 298 |
| StepFun Step 5 preview (Step) | 20 | -31 ~ 75 |
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| openPangu-2.0-Pro (Pangu) | -167 | -221 ~ -118 |
| ERNIE 5.1 (Wenxin) | -332 | -399 ~ -272 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -340 | -410 ~ -277 |
| Qwen3.8-27B (Qwen27B) | -355 | -415 ~ -297 |
| Inception Mercury 3 (diffusion model) (Mercury) | -373 | -434 ~ -316 |
| Xiaohongshu dots3-note preview (Dots) | -493 | -563 ~ -432 |

### 3–4 players (weight  30% ; pairwise comparisons:  4824)

| Contestant | Rating | 95% interval |
|---|---:|---|
| GPT-6 Astra (ultra + fast) (AstraUF) | 239 | 181 ~ 296 |
| sparring_hard (hard sparring line) | 179 | 130 ~ 227 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | 9 | -45 ~ 66 |
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| StepFun Step 5 preview (Step) | -38 | -92 ~ 13 |
| Mistral Large 4 (Mistral) | -160 | -213 ~ -111 |
| openPangu-2.0-Pro (Pangu) | -176 | -231 ~ -124 |
| Inception Mercury 3 (diffusion model) (Mercury) | -183 | -243 ~ -129 |
| Qwen3.8-27B (Qwen27B) | -194 | -246 ~ -149 |
| ERNIE 5.1 (Wenxin) | -212 | -268 ~ -163 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -228 | -288 ~ -175 |
| Meituan LongCat-2.0 (LongCat) | -241 | -298 ~ -186 |
| Xiaohongshu dots3-note preview (Dots) | -294 | -348 ~ -244 |

### 12 players (weight  20% ; pairwise comparisons:  31878)

| Contestant | Rating | 95% interval |
|---|---:|---|
| GPT-6 Astra (ultra + fast) (AstraUF) | 162 | 133 ~ 192 |
| sparring_hard (hard sparring line) | 135 | 103 ~ 165 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | 91 | 63 ~ 118 |
| Doubao-Seed-Evolving (Doubao) (anchor, 0 points) | 0 | — |
| StepFun Step 5 preview (Step) | -37 | -67 ~ -8 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | -67 | -97 ~ -39 |
| Mistral Large 4 (Mistral) | -119 | -150 ~ -90 |
| Qwen3.8-27B (Qwen27B) | -147 | -174 ~ -121 |
| Inception Mercury 3 (diffusion model) (Mercury) | -172 | -209 ~ -136 |
| Meituan LongCat-2.0 (LongCat) | -174 | -207 ~ -141 |
| ERNIE 5.1 (Wenxin) | -233 | -260 ~ -205 |
| openPangu-2.0-Pro (Pangu) | -283 | -311 ~ -253 |
| Xiaohongshu dots3-note preview (Dots) | -365 | -392 ~ -340 |

## 4. 1v1 directly against sparring_hard (reference, not separately scored)

Seat-swapped 1v1 games against sparring_hard, including imported previews. If this board excludes sparring_hard, these games do not count in its rating. Score rate = mean rank points: win 1, same-turn death 0.5, loss 0.

| Contestant | Games | W | Same-turn death | L | Score rate | Mean turns |
|---|---:|---:|---:|---:|---:|---:|
| GPT-6 Astra (ultra + fast) (AstraUF) | 48 | 21 | 0 | 27 | 43.8% | 150.4 |
| Mistral Large 4 (Mistral) | 48 | 17 | 2 | 29 | 37.5% | 148.2 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | 48 | 9 | 2 | 37 | 20.8% | 122.5 |
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
| ERNIE 5.1 (Wenxin) | Passed | 1216 | 0 | 0 | 0 | 0 | 6 | 0.1 | 0.4 |
| openPangu-2.0-Pro (Pangu) | Passed | 1220 | 0 | 0 | 0 | 0 | 6 | 0.1 | 1.4 |
| Xiaohongshu dots3-note preview (Dots) | Passed | 1216 | 0 | 0 | 0 | 0 | 98 | 0.2 | 4.0 |
| Meituan LongCat-2.0 (LongCat) | Passed | 1220 | 0 | 0 | 0 | 0 | 1999 | 16.1 | 48.7 |
| Qwen3.8-27B (Qwen27B) | Passed | 1216 | 0 | 0 | 0 | 0 | 501 | 10.3 | 23.9 |
| Doubao-Seed-Evolving (Doubao) | Passed | 1220 | 18 | 18 | 0 | 0 | 1615 | 11.0 | 51.6 |
| Mistral Large 4 (Mistral) | Passed | 1220 | 0 | 0 | 0 | 0 | 1991 | 19.9 | 41.9 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | Passed | 1216 | 0 | 0 | 0 | 0 | 1993 | 24.4 | 44.0 |
| StepFun Step 5 preview (Step) | Passed | 1216 | 0 | 0 | 0 | 0 | 1986 | 11.1 | 34.7 |
| Inception Mercury 3 (diffusion model) (Mercury) | Passed | 952 | 0 | 0 | 0 | 0 | 1998 | 9.6 | 49.0 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | Passed | 1212 | 0 | 0 | 0 | 0 | 128 | 24.5 | 49.1 |
| GPT-6 Astra (ultra + fast) (AstraUF) | Passed | 1220 | 0 | 0 | 0 | 0 | 600 | 17.2 | 42.5 |
| sparring_hard | Passed | 1220 | 0 | 0 | 0 | 0 | 40 | 6.0 | 21.8 |

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
| GPT-6 Astra (ultra + fast) (AstraUF) | codex gpt-6-astra | Manual after submission | 54.5 minutes | at minute 39.7 | Unchanged | 0c5883a27999 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | other deepseek-flash | Manual after submission | 36.1 minutes | at minute 29.4 | Unchanged | 25931c5e6f69 |

## 7. Schedule, sources and environment

- 1v1 is shared by all boards. Only Mercury pairings added: 20 maps against every other round-robin program, same map set as prior final, both seats, 480 new games. Others imported from the prior final, also 20 maps/both seats. This board uses 480 new games among its 13 programs.
- greedy fills multiplayer seats only, no 1v1 round robin, ratings, pairwise comparisons or leaderboard entry. Same policy as previews. Doubao anchors 0.
- 4-player additions: 55 tables including Mercury; 3 other seats drawn from the remaining 13 programs, least appearances first, random ties. One new map/table, 4 seat rotations, 220 games. Other 4-player games imported.
- 12-player additions: 21 maps, 14 programs for 12 seats, rotating exclusions. Random circle per map; game j seats 12 consecutive programs starting at j, other 2 sit out. 14 games/map, each program plays 12 (every seat once), rests 2: 294 games. All new-field programs participate; prior final games also imported.
- 3p is unscored: not played in final and discarded from imported previews. The 3–4-player component uses 4-player games only.
- Imported prior final (`final1018`, formal): engine/compiler checked against its `清单.json` and sources against `程序.json`. Overrun games retained because formal penalties already applied. Multiplayer games enter only their original board, retaining original bootstrap blocks.
- Imports (table below): 1v1 requires both programs on board; multiplayer from `preview2/multi_ref`, `final1018/results` requires all non-fillers on board (corresponding preview policy). Preview 12-player greedy copies follow the same filler rules. New games avoid all imported seeds, except shared 1v1 deliberately reuses previous maps for previously unplayed pairs: no duplicate game.
- Schedule fingerprint a5ccca4ecec5e645…; table seed 7007 (each board uses table_seed + board name). Map seeds in `赛程.json` (unpublished at the time; released schedules are now in `../tournament/`).

Scored games on this board by source:

| Source | 1v1 | 4p | 12p | Total |
|---|---:|---:|---:|---:|
| New final games | 480 | 220 | 294 | 994 |
| final1018 (10-18 formal final, no Mercury) | 2640 | 672 | 247 | 3559 |
| preview1 | 358 | 0 | 0 | 358 |
| preview2 | 169 | 48 | 26 | 243 |
| Total | 3647 | 940 | 567 | 5154 |

Games played / scored pairwise comparisons per contestant, indicating coverage; excludes fillers, outsiders and greedy:

| Contestant | 1v1 | 4p | 12p |
|---|---:|---:|---:|
| Wenxin | 568 / 568 | 272 / 736 | 504 / 5096 |
| Pangu | 568 / 568 | 276 / 752 | 504 / 5099 |
| Dots | 568 / 568 | 268 / 744 | 504 / 5099 |
| LongCat | 568 / 568 | 272 / 760 | 504 / 5099 |
| Qwen27B | 567 / 567 | 268 / 756 | 504 / 5102 |
| Doubao | 567 / 567 | 276 / 772 | 504 / 5103 |
| Mistral | 568 / 568 | 276 / 764 | 504 / 5101 |
| Hunyuan | 568 / 568 | 272 / 760 | 504 / 5099 |
| Step | 568 / 568 | 272 / 756 | 504 / 5100 |
| Mercury | 480 / 480 | 220 / 608 | 252 / 2555 |
| DSNew | 568 / 568 | 268 / 744 | 504 / 5103 |
| AstraUF | 568 / 568 | 276 / 756 | 504 / 5100 |
| sparring_hard | 568 / 568 | 272 / 740 | 504 / 5100 |

Import preview1: read 600, retained 598 across boards; discarded 2 overrun games (fast mode has no penalties, unlike formal). Package and all source hashes match.

Import preview2: read 826, retained 387 across boards; discarded 1 overrun game and 438 duplicates of earlier sources (first copy retained). Package and all source hashes match.

Import final1018: read 4267, retained 4267 across boards; discarded none. Package and all source hashes match.

- CPU affinity: [0, 2, 4, 6, 8, 10, 12, 14]; 12-player: [0, 2, 4, 6, 8, 10, 12, 14]. Power plan GUID: 381b4222-f694-41f0-9685-ff5bb260df2e (Balanced).
- Cycles per CPU millisecond measured by referee: 2419200.
- Final 12p across both boards: 534 games, 53.3 minutes in this run.
- Final 4p across both boards: 432 games, 6.8 minutes in this run.
- Final 1v1 across both boards: 480 games, 6.3 minutes in this run.

Original event output paths: `回放\` (shared `回放\1v1\`, this board’s multiplayer `回放\强对手榜\`, index `回放\INDEX.md`). Per-game data: `results.jsonl` (new final), `预览并入.jsonl`; summary in `results.json` → `boards`; video statistics in `视频统计_强对手榜.md`.

## Supplement: rank points (old scoring, reference only, no placement)

Normalize rank points as (N − place)/(N − 1), averaging same-turn ties, with fillers occupying seats normally. Average by type, subtract Doubao’s mean, ×100, then use the same weights. 95% intervals from the same 1000 stratified resamples.

| Contestant | Rank-point total | 95% interval | 1v1 | 3–4 players | 12 players |
|---|---:|---|---:|---:|---:|
| GPT-6 Astra (ultra + fast) (AstraUF) | +27.6 | [+24.1, +31.6] | +33.1 | +25.0 | +17.9 |
| DeepSeek V4.1 Flash (dsh 0.2.1-rc.2 standard mode) (DSNew) | +10.8 | [+6.4, +14.8] | +22.5 | +3.9 | -8.3 |
| Mistral Large 4 (Mistral) | +7.1 | [+3.4, +10.8] | +30.6 | -17.2 | -15.2 |
| Doubao-Seed-Evolving (Doubao) | +0.0 | [+0.0, +0.0] | +0.0 | +0.0 | +0.0 |
| StepFun Step 5 preview (Step) | -0.3 | [-4.5, +4.0] | +1.8 | -1.1 | -4.4 |
| Meituan LongCat-2.0 (LongCat) | -3.4 | [-7.6, +0.8] | +19.6 | -29.1 | -22.3 |
| Tencent Hunyuan Hy4 preview (Hunyuan) | -20.0 | [-23.7, -16.2] | -28.9 | -25.7 | +10.9 |
| openPangu-2.0-Pro (Pangu) | -20.9 | [-24.9, -16.6] | -14.1 | -22.9 | -35.0 |
| Qwen3.8-27B (Qwen27B) | -25.5 | [-29.4, -21.7] | -30.1 | -22.4 | -18.6 |
| Inception Mercury 3 (diffusion model) (Mercury) | -26.1 | [-29.9, -22.2] | -31.2 | -20.1 | -22.0 |
| ERNIE 5.1 (Wenxin) | -27.1 | [-30.9, -23.1] | -28.2 | -23.8 | -29.1 |
| Xiaohongshu dots3-note preview (Dots) | -39.7 | [-43.1, -36.2] | -40.3 | -36.2 | -43.4 |
| sparring_hard | +28.1 | [+24.7, +31.3] | +37.6 | +20.8 | +15.2 |
