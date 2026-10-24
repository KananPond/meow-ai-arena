[简体中文](README.md)

# Episode 7 — Legacy Code Optimization: counterexample inputs

These are all inputs used for the unified evaluation's counterexample column. Each `.in` has an `.out`, the standard output of `../../../workspace/baseline/` on that input. They use the old Round 1 input format and can be fed directly to submissions under `../../../submissions/ep7/`.

- **10 shared-pool inputs** (`stale_distance` … `round2_21d6b9fb03b8`) predate this event. The first 7 represent upheld Episode 5 Round 2 defects: 6 found by GPT-6 Astra and `blocked_pickup_churn` by GPT-6.1 Sol (originals in `../../../submissions/round2/`). `unreachable_charger_sentinel` and the final two `round2_*` inputs came from Astra's targeted search in a separate organizer evaluation on 10-17; its other materials are absent here. All were reused unchanged against submissions.
- **16 independently upheld Astra inputs**, named `astra_<first 12 SHA-256 digits>`, came from two search rounds and independent organizer reproduction. Competition names had an extra event prefix; contents are identical.
- **2 inputs under `excluded/`** were upheld independently (baseline completed within 10 seconds), but the unified evaluation recorded baseline CPU times of 9.88 and 9.89 seconds and wall times over 10 seconds. They were classified as invalid baselines and excluded from counts. They have no `.out`.

Verdicts follow Episode 5 Round 2: pass `tools/check_input.py` without `--full`, with a baseline that finishes normally within 10 seconds. Different output (normalizing only CRLF), abnormal exit, compilation failure, or failure to finish within 60 seconds is upheld. Timeout candidates first receive 100 seconds; those finishing between 60 and 100 seconds receive two further 60-second runs and must time out in all three. **Timeout verdicts depend on machine speed** and may differ on another machine.

Counts use input SHA-256, **not root causes**: several inputs may trigger one defect, and one input may fail several submissions. Per-submission verdicts are in `../结果.json` under `counterexamples`; independent run timings are in `../独立复现_结果.json`.

The final column lists only failing submissions. Names outside this event are reference entries described [here](../README_en.md). Pangu was too slow for unified counterexample testing, so its rows show independent results only. LongCat failed compilation and is absent from the table.

| File | Source | Baseline CPU seconds | Submissions failing in the unified evaluation |
|---|---|---:|---|
| `stale_distance` | Shared pool: stale distance used as current distance | 0.00 | 豆包 Seed 2.1 Turbo (original lead submission) (wrong output) |
| `blocked_origin_charger` | Shared pool: charging-distance directionality at a blocked start | 0.02 | Gemini 3.8 Flash (original lead submission) (wrong output) |
| `cache_eviction` | Shared pool: cache eviction overwrites a distance field still in use | 0.02 | Gemini 3.8 Flash (original lead submission) (wrong output) |
| `cache_thrashing` | Shared pool: unreachable pickup and cache thrashing | 3.56 | — |
| `eager_tail` | Shared pool: eager distance calculation for unreachable orders | 4.64 | DeepSeek V4.1 Flash (original lead submission) (timeout) |
| `no_candidate_dispatch` | Shared pool: distance calculations without any dispatchable robot | 2.00 | GLM 5.3 (original lead submission) (timeout) |
| `blocked_pickup_churn` | Shared pool: blocked pickups and dynamic blocking | 6.80 | GLM 5.3 (original lead submission) (timeout), MiMo V2.6 Pro (original lead submission) (timeout) |
| `unreachable_charger_sentinel` | Shared pool: unreachable-charger sentinel treated as finite cost | 0.00 | DeepSeek V4.1 Flash 1017 · high · trial 1 (wrong output) |
| `round2_4985a0f30877` | Shared pool: second targeted-search stress input (cache/charging/blocking) | 8.58 | Qwen (timeout), Mercury (timeout), Sonnet (timeout), Opus (timeout), GPT-6 Astra · medium (timeout), GPT-6.1 Sol · medium (timeout), GPT-5.6 Sol · medium (timeout), DeepSeek V4.1 Flash 1017 · max · trial 0 (timeout), DeepSeek V4.1 Flash 1017 · high · trial 1 (timeout), Gemini 3.8 Flash (original lead submission) (timeout), DeepSeek V4.1 Flash (original lead submission) (timeout), GLM 5.3 (original lead submission) (timeout), MiMo V2.6 Pro (original lead submission) (timeout) |
| `round2_21d6b9fb03b8` | Shared pool: second targeted-search stress input (cache/charging/blocking) | 7.56 | DeepSeek V4.1 Flash 1017 · max · trial 0 (timeout), Gemini 3.8 Flash (original lead submission) (timeout) |
| `astra_75ed43fbe778` | Astra second round `Haiku/timeout_candidates/N01_unreachable_delivery_tail/case.in` | 7.27 | Haiku (timeout); independent reproduction: Haiku (timeout) |
| `astra_5383cd84815e` | Astra second round `Haiku/timeout_candidates/unrun/irregular_walls24_s271828_p800_t2600.in` | 8.05 | Haiku (timeout); independent reproduction: Haiku (timeout) |
| `astra_f8a9b32ef914` | Astra second round `Haiku/timeout_candidates/other_measured/haiku_irregular_1000_t2200/case.in` | 8.78 | Haiku (timeout); independent reproduction: Haiku (timeout) |
| `astra_3e1ec47f6bbb` | Astra first round `Mercury/01_erased_order_id/case.in` | 0.00 | Mercury (wrong output); independent reproduction: Mercury (wrong output) |
| `astra_c0585f15c564` | Astra second round `Mercury/timeout_candidates/N01_order_lookup/case.in` | 5.97 | Mercury (timeout); independent reproduction: Mercury (timeout) |
| `astra_0385598ba299` | Astra second round `Mercury/timeout_candidates/unrun/mercury_capacity_2002_sources_t2200.in` | 7.77 | Mercury (timeout); independent reproduction: Mercury (timeout) |
| `astra_aaade735b3bb` | Astra second round `Mercury/timeout_candidates/other_measured/mercury_hash_idle_19900_t50/case.in` | 8.22 | Mercury (timeout); independent reproduction: Mercury (timeout) |
| `astra_c35ba7ab018e` | Astra first round `Pangu/01_dead_occupancy/case.in` | 0.00 | —; independent reproduction: Pangu (wrong output) |
| `astra_37b3750fba7e` | Astra first round `Pangu/02_rescue_occupancy/case.in` | 0.00 | —; independent reproduction: Pangu (wrong output) |
| `astra_35128cc3ff60` | Astra second round `Mercury/timeout_candidates/N01_order_lookup/controls/pangu_hash_2000_t400/case.in` | 4.09 | —; independent reproduction: Pangu (timeout) |
| `astra_29c23d01d967` | Astra second round `Pangu/timeout_candidates/N01_eager_dispatch/case.in` | 5.22 | —; independent reproduction: Pangu (timeout) |
| `astra_156e2d832dbc` | Astra second round `Pangu/timeout_candidates/N02_order_lookup/case.in` | 4.09 | —; independent reproduction: Pangu (timeout) |
| `astra_f1a67a37b94e` | Astra second round `Pangu/timeout_candidates/unrun/pangu_eager_busy_heavy.in` | 2.34 | —; independent reproduction: Pangu (timeout) |
| `astra_e8969d043245` | Astra second round `Pangu/timeout_candidates/unrun/pangu_hash_string_2000.in` | 9.88 | Excluded (baseline exceeded 10 seconds in unified evaluation); independent reproduction: Pangu (timeout) |
| `astra_67c40dc3e9e5` | Astra second round `Qwen27B/timeout_candidates/N01_order_lookup/case.in` | 6.02 | Qwen (timeout); independent reproduction: Qwen (timeout) |
| `astra_fd101dc72289` | Astra second round `Qwen27B/timeout_candidates/unrun/qwen_hash_int_2000.in` | 9.89 | Excluded (baseline exceeded 10 seconds in unified evaluation); independent reproduction: Qwen (timeout) |
| `astra_fea733719952` | Astra second round `Qwen27B/timeout_candidates/unrun/qwen_hash_int_8000.in` | 6.34 | Qwen (timeout); independent reproduction: Qwen (timeout) |
| `astra_9f301b935003` | Astra second round `Qwen27B/timeout_candidates/N01_order_lookup/controls/qwen_hash_4000/case.in` | 8.02 | Qwen (timeout); independent reproduction: Qwen (timeout) |
