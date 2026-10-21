# Haiku 第二轮报告

新增本地已复现根因 1 个。

先检查候选和 a；订单 b/c 按图版本缓存，distUntil 提前到目标/桩停止。b=INF 后仍计算 c 是新增短路遗漏。distFrom 的2048阈值仅在 tick 开头清理，没有跨活动指针清空。

源码覆盖和排除理由：[审计文档](../../work/agent_smh/static_audit.md)。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

- [N01_unreachable_delivery_tail 取送距离已不可达仍逐单搜索充电距离](timeout_candidates/N01_unreachable_delivery_tail/README.md)：基线 8.078/8.125 秒 CPU/墙钟；目标 75.031/75.879 完成 (exit=0); 59.500/60.004 截停 (exit=1); 59.547/60.002 截停 (exit=1)。

已知问题（不计新增）：现有学习材料没有该实现的已确认根因。

历史出处：`../../learning/first_round/report.md`、`../../learning/legacy_recheck/ep7_recheck.json`。旧超时记录不充当本轮实测；LongCat 的多个旧池编译错误不重复计数。

未另行确认其他独立性能根因；静态疑点、生成未运行档位和不合格输入没有升级为有效发现。

主测试：144 条目标测试记录，其中 144 条有实际目标执行（144 个独立输入 SHA），含复测共 146 次目标执行。主测试状态：`{"equal_under_threshold": 143, "timeout_confirmed_local": 1}`。

归因诊断：0 条记录、0 次目标执行；诊断状态：`{}`。两类合计 146 次目标执行。诊断仅解释既有差异，不扩充主测试覆盖；记录数也不等于独立 SHA 数或根因数。compile_blocked 是计划阻断标记，不是运行次数。完整逐次 CPU、墙钟、退出码、PID、输入与输出路径见 [summary.json](summary.json)。

压力候选、补充实测与未运行档位见 [timeout_candidates 索引](timeout_candidates/README.md)。

统一使用原输入检查器且不加 `--full`。原始输出只统一 CRLF 后精确比较；未删空白、未重排行、未忽略不可见字节。所有执行串行，截停只针对本轮启动的精确 PID。编译记录在 `../../build/compile.json`，原源码完整性与串行审计在 `../../work/integrity.json`。
