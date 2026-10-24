[简体中文](性能探针.md)

# Final audit — performance probes

The 9 upheld Round 2 timeout counterexamples were converted to the new format (`agingEvery=50`, `carryCost=2`, so the new requirements apply), then run once against each of the 6 final submissions with a 60-second limit. Case filenames retain their original identifiers.

| Case (original lead submission tested) | Reference | Team A Sonnet 5.5 | Team B Fable 5.1 | Team C GPT-6.1 Sol | Team D Opus 5.5 | Team E Grok 4.7 | Team F GPT-6 Astra |
|---|---|---|---|---|---|---|---|
| 原打C_F_03_cache_thrashing_timeout.in | 1.6 s | 0.38 s | 0.38 s | 0.36 s | 0.16 s | Abnormal exit | 0.34 s |
| 原打D_C_01_eager_unreachable_timeout.in | 2.5 s | 0.03 s | 0.39 s | 0.01 s | 0.07 s | Abnormal exit | 0.00 s |
| 原打D_F_01_eager_tail_timeout.in | 1.7 s | 0.02 s | 0.30 s | 0.04 s | 0.09 s | Abnormal exit | 0.05 s |
| 原打E_B_e2_timeout_800x1500.in | 1.4 s | 0.01 s | 0.01 s | 0.00 s | 0.01 s | Abnormal exit | 0.00 s |
| 原打E_C_01_eager_dispatch_timeout.in | 2.5 s | 0.01 s | 0.01 s | 0.00 s | 0.01 s | Abnormal exit | 0.00 s |
| 原打E_D_case3_block_churn_heavy.in | 2.0 s | 0.02 s | 0.01 s | 0.00 s | 0.03 s | Abnormal exit | 0.00 s |
| 原打E_F_01_eager_dispatch_timeout.in | 0.7 s | 0.01 s | 0.01 s | 0.00 s | 0.01 s | Abnormal exit | 0.00 s |
| 原打F_C_01_eager_unreachable_timeout.in | 2.8 s | 0.03 s | 4.02 s | 0.01 s | 0.08 s | Abnormal exit | 0.00 s |
| 原打F_D_case3_blocked_pickup_churn_heavy.in | 1.8 s | 0.02 s | 2.96 s | 0.05 s | 4.97 s | Abnormal exit | 0.04 s |
