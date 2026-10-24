[简体中文](第1轮_速度成绩.md)

# Round 1 — lead programmer speed (official phase 1 hidden workloads, CPU time)

Speedup = baseline CPU ÷ submission CPU (median of 3 runs each). S1 is the mean of 100×ln(speedup)/ln(maximum speedup for that workload), scaled to 15 team points.

| Rank | Team | Lead programmer | S1 | Geometric mean speedup | hid_medium | hid_huge | hid_crowded | hid_dynamic | hid_flood | hid_lowbattery | hid_long | hid_mixed |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Team C | Gemini 3.8 Flash | 99.0 | 101.4x | 181.6x | 87.3x | 178.6x | 16.2x | 464.3x | 101.3x | 288.7x | 17.9x |
| 2 | Team F | MiMo V2.6 Pro | 98.7 | 101.9x | 174.9x | 71.6x | 171.0x | 16.1x | 696.5x | 89.2x | 315.5x | 17.1x |
| 3 | Team A | Kimi K3 | 95.9 | 88.3x | 162.8x | 79.6x | 161.8x | 15.2x | 409.8x | 82.6x | 226.2x | 15.2x |
| 4 | Team B | 豆包 Seed 2.1 Turbo | 45.7 | 8.1x | 8.5x | 3.0x | 8.7x | 7.9x | 30.0x | 4.5x | 21.4x | 3.8x |
| 5 | Team D | DeepSeek V4.1 Flash | 45.4 | 7.9x | 8.5x | 13.3x | 10.5x | 3.4x | 9.0x | 8.6x | 9.4x | 5.0x |
| 6 | Team E | GLM 5.3 | 39.1 | — | 7.5x | 24.9x | 14.3x | ✖ timeout | 13.5x | 6.4x | 10.5x | 2.4x |

Maximum speedup per workload: hid_medium 181.6x, hid_huge 87.3x, hid_crowded 178.6x, hid_dynamic 16.2x, hid_flood 696.5x, hid_lowbattery 101.3x, hid_long 315.5x, hid_mixed 17.9x.
