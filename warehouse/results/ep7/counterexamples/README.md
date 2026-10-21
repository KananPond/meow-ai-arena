# 第 7 集 · 屎山优化：反例输入

这里是统一评测"反例"一列用到的全部输入，每个 `.in` 配一份 `.out`，`.out` 是基线（`../../../workspace/baseline/`）在这份输入上的标准输出。格式和第 1 轮的输入相同（旧格式），可以直接喂给 `../../../submissions/ep7/` 里的任何一份代码。

- **统一池 10 份**（`stale_distance` … `round2_21d6b9fb03b8`）：不是这一场新找的。前 7 份是第 5 集第 2 轮判定成立的反例，按缺陷挑出的代表（6 份是 GPT-6 Astra 找的，`blocked_pickup_churn` 是 GPT-6.1 Sol 找的，原件在 `../../../submissions/round2/`）；`unreachable_charger_sentinel` 和最后 2 份 `round2_*` 来自主办方 10-17 另一次内部评测里 GPT-6 Astra 的专项找缺陷，那次评测的其他材料不在本仓库。这一场把它们原样拿来，对所有提交都跑一遍。
- **Astra 复现成立 16 份**（`astra_<SHA-256 前 12 位>`）：GPT-6 Astra 两轮找缺陷交上来的候选输入，经主办方独立复现成立的。文件名是输入内容的 SHA-256 前 12 位，比赛时的文件名前面多一个场次前缀，内容相同。
- **`excluded/` 2 份**：独立复现时成立（基线在 10 秒内跑完），但统一评测时基线这两份分别用了 9.88 和 9.89 CPU 秒，墙钟超过了 10 秒的上限，被判"基线无效"，没有计入反例数。没有 `.out`。

判定规则和第 5 集第 2 轮相同：输入要通过 `tools/check_input.py`（不加 `--full`），基线 10 秒内正常结束；被测程序输出和基线不同（只统一 CRLF）、异常退出、编译失败，或者超过 60 秒没结束，算成立。超时先给 100 秒；在 60～100 秒之间结束的，再跑两次 60 秒，三次都超时才成立。**超时类反例依赖机器速度**，换一台机器结论可能不同。

反例按输入的 SHA-256 计数，**不是按根因**：同一个缺陷可能被几份输入触发，一份输入也可能同时让几份代码出错。逐份逐输入的判定在 `../结果.json` 每个提交的 `counterexamples` 字段，独立复现的逐次用时在 `../独立复现_结果.json`。

下表最后一列只列出出错的提交；本场之外的名字是参照行（第 5 集原主程、10-17 内部评测的提交，见 `../README.md`）。盘古太慢，没有跑统一评测的反例，它那几行只有独立复现的结果。LongCat 编译失败，不在表里。

| 文件 | 来源 | 基线 CPU 秒 | 统一评测里在哪些提交上成立 |
|---|---|---:|---|
| `stale_distance` | 统一池：旧距离被当成当前距离 | 0.00 | 豆包 Seed 2.1 Turbo（原主程）（输出错误） |
| `blocked_origin_charger` | 统一池：被封起点充电距离方向性 | 0.02 | Gemini 3.8 Flash（原主程）（输出错误） |
| `cache_eviction` | 统一池：缓存淘汰覆盖仍在使用的距离场 | 0.02 | Gemini 3.8 Flash（原主程）（输出错误） |
| `cache_thrashing` | 统一池：不可达取货点与缓存抖动性能 | 3.56 | — |
| `eager_tail` | 统一池：预先计算不可达订单距离性能 | 4.64 | DeepSeek V4.1 Flash（原主程）（超时） |
| `no_candidate_dispatch` | 统一池：无可派机器人时仍计算距离性能 | 2.00 | GLM 5.3（原主程）（超时） |
| `blocked_pickup_churn` | 统一池：被封取货点与动态封锁性能 | 6.80 | GLM 5.3（原主程）（超时）、MiMo V2.6 Pro（原主程）（超时） |
| `unreachable_charger_sentinel` | 统一池：不可达充电距离哨兵当作有限成本 | 0.00 | DeepSeek V4.1 Flash 1017 · high · 试跑1（输出错误） |
| `round2_4985a0f30877` | 统一池：二轮专项强输入（缓存/充电/封路压力） | 8.58 | Qwen（超时）、Mercury（超时）、Sonnet（超时）、Opus（超时）、GPT-6 Astra · medium（超时）、GPT-6.1 Sol · medium（超时）、GPT-5.6 Sol · medium（超时）、DeepSeek V4.1 Flash 1017 · max · 试跑0（超时）、DeepSeek V4.1 Flash 1017 · high · 试跑1（超时）、Gemini 3.8 Flash（原主程）（超时）、DeepSeek V4.1 Flash（原主程）（超时）、GLM 5.3（原主程）（超时）、MiMo V2.6 Pro（原主程）（超时） |
| `round2_21d6b9fb03b8` | 统一池：二轮专项强输入（缓存/充电/封路压力） | 7.56 | DeepSeek V4.1 Flash 1017 · max · 试跑0（超时）、Gemini 3.8 Flash（原主程）（超时） |
| `astra_75ed43fbe778` | Astra 第二轮 `Haiku/timeout_candidates/N01_unreachable_delivery_tail/case.in` | 7.27 | Haiku（超时）；独立复现：Haiku（超时） |
| `astra_5383cd84815e` | Astra 第二轮 `Haiku/timeout_candidates/unrun/irregular_walls24_s271828_p800_t2600.in` | 8.05 | Haiku（超时）；独立复现：Haiku（超时） |
| `astra_f8a9b32ef914` | Astra 第二轮 `Haiku/timeout_candidates/other_measured/haiku_irregular_1000_t2200/case.in` | 8.78 | Haiku（超时）；独立复现：Haiku（超时） |
| `astra_3e1ec47f6bbb` | Astra 第一轮 `Mercury/01_erased_order_id/case.in` | 0.00 | Mercury（输出错误）；独立复现：Mercury（输出错误） |
| `astra_c0585f15c564` | Astra 第二轮 `Mercury/timeout_candidates/N01_order_lookup/case.in` | 5.97 | Mercury（超时）；独立复现：Mercury（超时） |
| `astra_0385598ba299` | Astra 第二轮 `Mercury/timeout_candidates/unrun/mercury_capacity_2002_sources_t2200.in` | 7.77 | Mercury（超时）；独立复现：Mercury（超时） |
| `astra_aaade735b3bb` | Astra 第二轮 `Mercury/timeout_candidates/other_measured/mercury_hash_idle_19900_t50/case.in` | 8.22 | Mercury（超时）；独立复现：Mercury（超时） |
| `astra_c35ba7ab018e` | Astra 第一轮 `Pangu/01_dead_occupancy/case.in` | 0.00 | —；独立复现：盘古（输出错误） |
| `astra_37b3750fba7e` | Astra 第一轮 `Pangu/02_rescue_occupancy/case.in` | 0.00 | —；独立复现：盘古（输出错误） |
| `astra_35128cc3ff60` | Astra 第二轮 `Mercury/timeout_candidates/N01_order_lookup/controls/pangu_hash_2000_t400/case.in` | 4.09 | —；独立复现：盘古（超时） |
| `astra_29c23d01d967` | Astra 第二轮 `Pangu/timeout_candidates/N01_eager_dispatch/case.in` | 5.22 | —；独立复现：盘古（超时） |
| `astra_156e2d832dbc` | Astra 第二轮 `Pangu/timeout_candidates/N02_order_lookup/case.in` | 4.09 | —；独立复现：盘古（超时） |
| `astra_f1a67a37b94e` | Astra 第二轮 `Pangu/timeout_candidates/unrun/pangu_eager_busy_heavy.in` | 2.34 | —；独立复现：盘古（超时） |
| `astra_e8969d043245` | Astra 第二轮 `Pangu/timeout_candidates/unrun/pangu_hash_string_2000.in` | 9.88 | 未计入（统一评测时基线 10 秒内没跑完）；独立复现：盘古（超时） |
| `astra_67c40dc3e9e5` | Astra 第二轮 `Qwen27B/timeout_candidates/N01_order_lookup/case.in` | 6.02 | Qwen（超时）；独立复现：Qwen（超时） |
| `astra_fd101dc72289` | Astra 第二轮 `Qwen27B/timeout_candidates/unrun/qwen_hash_int_2000.in` | 9.89 | 未计入（统一评测时基线 10 秒内没跑完）；独立复现：Qwen（超时） |
| `astra_fea733719952` | Astra 第二轮 `Qwen27B/timeout_candidates/unrun/qwen_hash_int_8000.in` | 6.34 | Qwen（超时）；独立复现：Qwen（超时） |
| `astra_9f301b935003` | Astra 第二轮 `Qwen27B/timeout_candidates/N01_order_lookup/controls/qwen_hash_4000/case.in` | 8.02 | Qwen（超时）；独立复现：Qwen（超时） |
