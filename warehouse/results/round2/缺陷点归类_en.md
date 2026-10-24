[简体中文](缺陷点归类.md)

# Round 2 rematch — defect classification (2026-10-08)

Official verdicts: [matrix](矩阵_en.md), `矩阵.json`; original log: `赛事/运行记录/击穿判定_第2轮重赛.log`. Of 28 cases, 20 were upheld and 8 rejected (all performance cases finishing within 60 seconds on an idle machine). The 20 upheld cases form 7 defects. Correction dated 2026-10-08: the opening once incorrectly said 21 / 7; the tables and `矩阵.json` always had 20 / 8. Machine-readable `缺陷点归类.json` is consumed by `团队计分.py`.

## Classification and evidence

- Cases fixed by the same code change count as one defect. Multiple cases from one tester for the same defect count once.
- Evidence combines official output-difference/timeout verdicts, locations cited in `说明.txt`, and organizer code inspection.
- **Not performed:** minimal-patch validation for every defect. Only Astra did this in its working copy: Gemini-3 still timed out after individually repairing Gemini-1 and Gemini-2. Other classifications rely on code and notes. No ambiguous boundary cases were identified: all 7 belong to different code locations or teams.

## 7 defects

Value = 1 + fraction of eligible testers who did not find it (5 eligible testers per implementation).

| Defect | Type | Found by | Value | Description |
|---|---|---|---|---|
| Doubao-1 | Different output | Sonnet, Sol, Opus, Grok, Astra (5/5) | 1.0 | Movement reads the distance array without checking the current search visit stamp. Blocking isolates the destination; stale distances make the robot keep moving while the baseline waits |
| Gemini-1 | Different output | Sonnet, Fable, Opus, Grok, Astra (5/5) | 1.0 | Nearest-charger distances use multisource searches from chargers, which cannot enter blocked cells; the baseline searches from delivery and may leave a blocked start. When pickup = delivery and blocked, the baseline dispatches but Gemini does not |
| Gemini-2 | Different output | Astra (1/5) | 1.8 | Dispatch holds a pointer into a 512-slot ring cache. Eviction overwrites that slot during robot iteration; later robots read distances from another origin, selecting the wrong robot |
| Gemini-3 | Timeout | Astra (1/5) | 1.8 | Full-map search for every pending order before checking robot reachability; 700 blocked pickups exceed the cache and all are evicted and recomputed each tick (63.5 seconds, baseline 3.5 seconds) |
| DeepSeek-1 | Timeout | Sol, Astra (2/5) | 1.6 | Both order-leg distances computed before checking robots. The baseline immediately skips blocked pickups; DeepSeek still searches the entire map, invalidating the cache whenever blocking changes (74–78 seconds, baseline 5–7 seconds) |
| GLM-1 | Timeout | Fable, Sol, Opus, Astra (4/5) | 1.2 | Distance fields computed for every pending order even without a dispatchable robot (baseline performs no searches); blocking changes clear the cache (67–79 seconds, baseline 2–7 seconds) |
| MiMo-1 | Timeout | Sol, Opus (2/5) | 1.6 | Order-level cache unconditionally computes both distances before the robot loop. For blocked pickups the baseline does no search; MiMo performs two full-map searches per order (61–74 seconds, baseline 5–8 seconds) |

No counterexample was found for Kimi. Astra records attempts at random differential tests, long-range rescue, and large-map cache-capacity tests.

## 8 rejected cases

| Tester → target | Case | Idle-machine runtime | Notes |
|---|---|---|---|
| Fable → GLM | e1 (500 orders), e3 (600 orders) | 43.4, 51.3 s | Three scales for one defect; the heaviest (800 orders, 67.1 seconds) was upheld |
| Opus → GLM | light, mid | 39.1, 57.8 s | Same pattern; heavy (75.8 seconds) upheld |
| Opus → MiMo | light, mid | 32.6, 45.2 s | Same pattern; heavy (61.3 seconds) upheld, close to the limit |
| Opus → Gemini | case2, case3 | 29.1, 27.4 s | Both rejected. Opus noted the narrow margin and possible equivalence verdict on faster machines. Gemini-3 is therefore not credited to Opus |

All 6 models ran on this machine concurrently during play, making programs slower than on an idle machine. Some local timeouts were rejected on review. Fable and Opus used several scales; their heaviest cases survived review.

## A common pattern

DeepSeek-1, GLM-1, MiMo-1, and Gemini-3 share eager distance computation. The baseline is lazy: check dispatchable robots and reachable pickups first and skip immediately if impossible. All four lead programmers instead precomputed and cached each order's distances. A backlog of impossible orders plus blocking changes every tick made them over ten times slower than baseline. Random differential testing misses this because outputs are correct; in the first competition, only Astra found one such case for GLM. Kimi does not have this issue.

## Counterexample scores

Sum the values of discovered defects, add the own-team allowance (mean of the other 5 testers' scores against that implementation), then normalize the maximum to 100.

| Tester | Defects found | Own discoveries | Own-team allowance | Total | Score |
|---|---|---|---|---|---|
| GPT-6 Astra | Doubao-1, Gemini-1, Gemini-2, Gemini-3, DeepSeek-1, GLM-1 | 8.4 | 0.64 (MiMo) | 9.04 | 100.0 |
| GPT-6.1 Sol | Doubao-1, DeepSeek-1, GLM-1, MiMo-1 | 5.4 | 1.72 (Gemini) | 7.12 | 78.8 |
| Opus 5.5 | Doubao-1, Gemini-1, GLM-1, MiMo-1 | 4.8 | 0.64 (DeepSeek) | 5.44 | 60.2 |
| Fable 5.1 | Gemini-1, GLM-1 | 2.2 | 1.00 (豆包) | 3.20 | 35.4 |
| Grok 4.7 | Doubao-1, Gemini-1 | 2.0 | 0.96 (GLM) | 2.96 | 32.7 |
| Sonnet 5.5 | Doubao-1, Gemini-1 | 2.0 | 0.00 (Kimi) | 2.00 | 22.1 |
