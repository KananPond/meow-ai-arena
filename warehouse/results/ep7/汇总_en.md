[简体中文](汇总.md)

# Unified evaluation (12 measured this event + 14 reused from 10-17)

Same contestant package, prompt, 8 hidden inputs, and baseline. Speedup is the geometric mean over 8 workloads, awarded only if all 8 are correct. No aggregate score.

| Submission | Source | Timing | Correct out of 8 | Geometric mean speedup | Auxiliary speedup (failed workloads count as 1×) | Counterexamples (10 shared + 16 new Astra inputs) | Type | Optimization time |
|---|---|---|---:|---:|---:|---:|---|---:|
|GPT-6 Astra · max · Fast|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|1045.6×|1045.6×|0 (12 pending)|-|25m45s|
|GPT-6.1 Sol · medium|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|534.3×|534.3×|1 (12 pending)|Timeout 1|22m35s|
|GPT-6 Astra · medium|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|455.9×|455.9×|1 (12 pending)|Timeout 1|26m42s|
|DeepSeek V4.1 Flash 1017 · max · trial 2|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|194.5×|194.5×|0 (12 pending)|-|24m40s|
|DeepSeek V4.1 Flash 1017 · max · trial 0|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|184.7×|184.7×|2 (12 pending)|Timeout 2|15m02s|
|Claude Sonnet|This event|Measured this event|8/8|169.7×|169.7×|1 (12 pending)|Timeout 1|46m18s|
|GPT-6 Sol · medium|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|161.5×|161.5×|0 (12 pending)|-|21m42s|
|DeepSeek V4.1 Flash 1017 · high · trial 1|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|151.9×|151.9×|2 (12 pending)|Wrong output 1, Timeout 1|31m31s|
|阶跃 Step 5 preview|This event|Measured this event|8/8|139.4×|139.4×|0|Not found in this inspection|56m26s|
|腾讯混元 Hy4 preview|This event|Measured this event|8/8|138.2×|138.2×|0|Not found in this inspection|61m52s|
|GPT-5.6 Sol · medium|10-17 (reference; code absent here)|Reused 10-17 measurement|8/8|113.3×|113.3×|1 (12 pending)|Timeout 1|12m58s|
|MiMo V2.6 Pro (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|105.5×|105.5×|2 (12 pending)|Timeout 2|45m01s (collected at deadline)|
|Gemini 3.8 Flash (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|104.8×|104.8×|4 (12 pending)|Wrong output 2, Timeout 2|10m29s|
|Claude Opus|This event|Measured this event|8/8|101.0×|101.0×|1 (12 pending)|Timeout 1|45m58s|
|Claude Haiku|This event|Measured this event|8/8|98.1×|98.1×|3 (12 pending)|Timeout 3|45m27s|
|Kimi K3 (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|87.9×|87.9×|0 (12 pending)|-|45m01s (collected at deadline)|
|通义千问 Qwen3.8-27B|This event|Measured this event|8/8|8.7×|8.7×|4|Timeout 4|45m00s (collected at deadline)|
|Inception Mercury 3 (diffusion model)|This event|Measured this event|8/8|8.6×|8.6×|5|Wrong output 1, Timeout 4|56m11s|
|豆包 Seed 2.1 Turbo (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|8.4×|8.4×|1|Wrong output 1|39m15s|
|DeepSeek V4.1 Flash (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|8.3×|8.3×|2 (12 pending)|Timeout 2|46m24s (collected at deadline)|
|GLM 5.3 (original lead)|Episode 5 original lead|Reused 10-17 measurement|8/8|6.9×|6.9×|3 (12 pending)|Timeout 3|34m00s|
|文心 ERNIE 5.1|This event|Measured this event|8/8|1.3×|1.3×|0|Not found in this inspection|36m25s|
|Mistral Large 4|This event|Measured this event|8/8 (baseline-identical code; not run separately)|About 1× (baseline-identical; screening single run 0.95×)|-|0|Same as baseline (identical code; not run separately)|62m07s|
|小红书 dots3-note preview|This event|Measured this event|8/8 (baseline-identical code; not run separately)|About 1× (baseline-identical; screening single run 0.74×)|-|0|Same as baseline (identical code; not run separately)|45m00s (collected at deadline)|
|盘古 openPangu-2.0-Pro|This event|Measured this event|1/8 (hid_huge correct; 7 others timed out without results and do not count as correct)|Too slow: at least 2× slower|-|Not run (too slow)|Astra first round reported 2 correctness defects (pending independent organizer reproduction)|45m00s (collected at deadline)|
|美团 LongCat-2.0|This event|Measured this event|0/8 (compilation failed)|Compilation failed|-|10 (16 pending)|Compilation failed 10|45m00s (collected at deadline)|

**Baseline reuse.** CPU times are from the 10-17 unified evaluation of fourteen submissions (`十四份统一评测/结果.json`, `baseline`), on the same machine/compiler with exclusive serial execution; the baseline was not remeasured. Measurements on 10-06 (task preparation) and 10-17 differed by -3.8% to +5.0% per workload and 0.65% geometrically over 8, implying about ±5% systematic error per workload and ±1% overall.

The 14 reused rows were not remeasured here. They come from that 10-17 evaluation, 17:27–18:20 UTC, on the same machine, w64devkit g++ 16.2, 8 hidden inputs, and baseline, with exclusive serial execution. Results for the shared pool's two second-round stress inputs come from the independent reproduction on 10-17 (`二轮独立复现/结果.json`), recompiling sealed code with a 65-second limit. Calibration below checks machine-state differences between dates.

**Calibration:** GPT-6 Astra · max · Fast (reference; code absent here), remeasured at 1073.6× versus 1045.6× on 10-17, ratio 1.027. Kimi K3 (original lead), remeasured at 86.5× versus 87.9×, ratio 0.985.

**小红书 dots3-note preview:** sealed `src/` matches baseline SHA-256 for all 5/5 files. Identical code, approximately 1×. Single measurements were hid_lowbattery 0.74× and hid_mixed 0.71×; deviation from 1 is single-run variation (the hid_mixed check used 8-way concurrency without timing). Correctness and counterexamples were recorded as baseline-identical without separate runs.

**Mistral Large 4:** sealed `src/` matches baseline SHA-256 for all 5/5 files. Identical code, approximately 1×. Single hid_lowbattery measurement was 0.95×; deviation from 1 is single-run variation (hid_mixed used 8-way concurrency without timing). Correctness and counterexamples were recorded as baseline-identical without separate runs.

**盘古 openPangu-2.0-Pro:** screening workload hid_lowbattery timed out at 180 seconds against a 33.5-second baseline, at least 5.4× slower. In single checks over all 8 workloads, only hid_huge completed correctly within its limit (229 CPU seconds versus baseline 113, about 2.0× slower); the other 7 failed to complete within max(180, baseline wall time×3+10) seconds. Its session measured a public workload at 119 seconds versus original 52, about 2.3× slower. Speed is recorded as a lower bound of “over 2× slower”, and the counterexample pool was skipped. Astra's first report also reproduced two correctness defects (occupancy not cleared on shutdown; occupancy not registered after rescue), originally recorded at `Astra找缺陷/第一轮/findings/Pangu` as pending independent organizer reproduction. This preserves the summary's original status; the [result index](README_en.md) explains the subsequent confirmation.

**Screening:** one timed hid_lowbattery run (baseline 33.5 CPU seconds). Speedup ≤ 1.05 means “not faster than baseline”: check all 8 workloads once for correctness using 8-way concurrency without timing. If that single-run geometric mean exceeds 1.05, overturn screening and run the full timing suite. Otherwise correctness is still checked on all 8, but no median-of-3 timing is performed; auxiliary speedup uses single-run CPU times.

**Background load:** 1.36 cores sampled before starting, below a 2.5-core threshold. During 409 timed runs, median background use was 0.85 cores, maximum 4.43, on a 32-core machine. Timing uses process CPU; 1–1.5 background cores have little effect. Cross-date consistency is assessed by calibration ratios. Timings with unresolved background interference: 0.

**Sealed source check:** True. Doubao's new submission was excluded by the user's 10-20 decision; the table reuses the Episode 5 original lead result instead.

Counterexamples count distinct input SHA-256, not root causes. Zero only means no reproduction on these inputs. “Pending” means those inputs have not been run for that entry; complete them with `--补跑反例`.

Targeted defect-search coverage differs: the 8 entries from 10-17 had two Astra max search rounds (1161 differential scenarios, then 62 inputs); the 12 current submissions had this event's Astra inspection; the 6 Episode 5 original leads had only that competition's mutual testing. Although submissions are tested against common inputs, this column cannot rank resistance to discovering counterexamples.
