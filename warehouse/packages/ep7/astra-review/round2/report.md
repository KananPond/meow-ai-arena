# 第二轮本地回归报告

检查了指定12份提交。按实现计数，新增本地已复现性能根因 5 项；跨实现归并为3类机制：无可派机器人仍求距离（Pangu）、订单表同桶查找退化（Pangu/Qwen27B/Mercury）、b已不可达仍求c（Haiku）。同桶机制在三份实现上分别实测确认，不代表三种不同算法思想。第一轮和旧统一复测池已知问题单列、不重复计数。

主测试 13 个输入的正常完成输出差异均逐案归于第一轮 Pangu-01「停机未清除占据缓存」，没有新增正确性根因，也没有观察到程序自行异常退出。相邻 tick 的状态快照与因果证据见 [输出差异逐案复核](work/output_triage.json)。30 条诊断前缀只用于归因，不膨胀主测试覆盖。LongCat 仍因已知声明错误不能按指定命令编译。未修复代码、不打分。

| 提交 | 新增根因 | 已知根因 | 本轮逐份报告 |
|---|---:|---:|---|
| Pangu | 2 | 2 | [报告 / 机器记录](findings/Pangu/report.md) |
| Dots | 0 | 0 | [报告 / 机器记录](findings/Dots/report.md) |
| Wenxin | 0 | 0 | [报告 / 机器记录](findings/Wenxin/report.md) |
| LongCat | 0 | 1 | [报告 / 机器记录](findings/LongCat/report.md) |
| Qwen27B | 1 | 1 | [报告 / 机器记录](findings/Qwen27B/report.md) |
| Mistral | 0 | 0 | [报告 / 机器记录](findings/Mistral/report.md) |
| Hunyuan | 0 | 0 | [报告 / 机器记录](findings/Hunyuan/report.md) |
| Step | 0 | 0 | [报告 / 机器记录](findings/Step/report.md) |
| Mercury | 1 | 2 | [报告 / 机器记录](findings/Mercury/report.md) |
| Haiku | 1 | 0 | [报告 / 机器记录](findings/Haiku/report.md) |
| Sonnet | 0 | 1 | [报告 / 机器记录](findings/Sonnet/report.md) |
| Opus | 0 | 1 | [报告 / 机器记录](findings/Opus/report.md) |

## 新增代表及正式计时

秒数顺序均为 CPU/墙钟。

| 提交 | 根因 | 基线 | 目标各次 | 代表证据 |
|---|---|---|---|---|
| Pangu | N01_eager_dispatch | 5.281/5.327 | 97.688/100.004 截停 (exit=1) | [输入 / 证据](findings/Pangu/timeout_candidates/N01_eager_dispatch/README.md) |
| Pangu | N02_order_lookup | 4.844/4.902 | 99.125/100.013 截停 (exit=1) | [输入 / 证据](findings/Pangu/timeout_candidates/N02_order_lookup/README.md) |
| Qwen27B | N01_order_lookup | 6.484/6.495 | 99.016/100.004 截停 (exit=1) | [输入 / 证据](findings/Qwen27B/timeout_candidates/N01_order_lookup/README.md) |
| Mercury | N01_order_lookup | 7.297/7.390 | 87.703/88.654 完成 (exit=0); 58.109/60.013 截停 (exit=1); 58.625/60.015 截停 (exit=1) | [输入 / 证据](findings/Mercury/timeout_candidates/N01_order_lookup/README.md) |
| Haiku | N01_unreachable_delivery_tail | 8.078/8.125 | 75.031/75.879 完成 (exit=0); 59.500/60.004 截停 (exit=1); 59.547/60.002 截停 (exit=1) | [输入 / 证据](findings/Haiku/timeout_candidates/N01_unreachable_delivery_tail/README.md) |

首测限100秒；在60至100秒完成的目标追加两次各60秒，仅两次均截停才确认。截停进程的退出码来自本轮运行器，不能当作自行崩溃；其不完整输出和完整差异仍保存，但不另计输出错误。本机有其他任务，记录了系统繁忙核数估计和进程CPU，最终超时以主办方空闲复现为准。

主测试共 166 条输入测试记录，按输入原始字节 SHA 去重为 165 个（只统一 CRLF 后也单独统计在清单中）；其中 164 条基线正常且在10秒内完成。另有 30 条诊断记录，合计 196 条证据记录。输入测试记录按标签计数，不等于独立 SHA 数或根因数。

实际执行次数：主测试基线 166 次、目标含复测 1551 次，共 1717 次；诊断基线 30 次、目标 30 次，共 60 次。全部合计 1777 次进程执行（基线 196 + 目标 1581）。编译和检查器不计入这些执行次数，compile_blocked 也不计实际运行。

不合格输入单列于 summary.json，没有运行其目标或纳入有效结论。没有运行 full 专属输入。目标与基线全部串行，实际被测进程同时最多1个。完整输入/输出/差异/退出码/CPU/墙钟/PID/起止时间在 [逐次记录](work/runs.jsonl) 和各输入的 evidence.json。

## 学习、覆盖与未成立结果

[历史学习与去重依据](work/learning_review.md)、[新输入与历史重复检查](work/test_inventory.json)、[整体运行汇总](work/result_review.json)。主测试中有116条长程状态记录（T=260..340），另有最大地图、蛇形长路、3364桩、20000事件、真实缓存容量边界、动态封解与编号对照。实际状态日志覆盖计数只用主测试的合格基线日志，保存在 summary.json；诊断日志另列，不把生成意图等同于实际覆盖。

Step 最大普通范围的无待派单预计算仅约1.7秒，未成立。Hunyuan 的全局充电场方向和容量/引用边界按实测记录保留，未据相对速度声称超时。Mercury 的1998/2000/2002来源断层属于已知提前计算根因的放大，不单独计新。Qwen27B、Sonnet、Opus 等缓存容量在普通范围足以覆盖所有来源，没有伪造不可达容量边界。生成但未执行的压力档位显式标为 unrun。

每份提交的 timeout_candidates/README.md 均提供索引；补充实测目录解释构造、预期成本、基线资格与实际结论。Dots/Mistral/Wenxin 的索引链接共同压力输入实测，LongCat 单列编译阻断。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

## 编译、复现与范围

所有目标与基线统一 `g++ -O2 -std=c++20 -static`，包含各 src 顶层全部 `.cpp`；基线包含其所有顶层 `.cpp`。[编译命令和诊断](build/compile.json)、[环境](build/environment.json)。单例复现命令在各代表 README 中；批量执行脚本 [audit.py](work/audit.py)、[run_plan.py](work/run_plan.py)，所有源生成脚本也保留在 work/。

没有改动 targets/、baseline/ 或原材料，没有联网或访问当前目录之外的比赛材料；按授权使用子代理做源码审计，所有目标与基线执行集中在主流程。[前后哈希与执行无重叠检查](work/integrity.json)。只终止本轮 Popen 启动的精确进程，不按进程名终止。

这些是有限测试和当前编译环境下的结论，未发现新增不等于无缺陷。完整机器可读结果见 [summary.json](summary.json)。
