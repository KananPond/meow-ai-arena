# Pangu 第二轮报告

新增本地已复现根因 2 个。

单源距离缓存仅保留一张表，充电距离未缓存；派单先算 b/c 再找候选。订单改用字符串 unordered_map，比较器重复查 ORD。两个新增性能根因分开归类。

源码覆盖和排除理由：[审计文档](../../work/agent_pqh/audit.json)。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

- [N01_eager_dispatch 没有可派机器人仍逐单计算取送及充电距离](timeout_candidates/N01_eager_dispatch/README.md)：基线 5.281/5.327 秒 CPU/墙钟；目标 97.688/100.004 截停 (exit=1)。
- [N02_order_lookup 字符串订单表长链使每 tick 查找和排序退化](timeout_candidates/N02_order_lookup/README.md)：基线 4.844/4.902 秒 CPU/墙钟；目标 99.125/100.013 截停 (exit=1)。

已知问题（不计新增）：停机未清除占据缓存（第一轮）；救援未登记新增占据（第一轮）

历史出处：`../../learning/first_round/report.md`、`../../learning/legacy_recheck/ep7_recheck.json`。旧超时记录不充当本轮实测；LongCat 的多个旧池编译错误不重复计数。

未另行确认其他独立性能根因；静态疑点、生成未运行档位和不合格输入没有升级为有效发现。

主测试：143 条目标测试记录，其中 143 条有实际目标执行（143 个独立输入 SHA），含复测共 143 次目标执行。主测试状态：`{"equal_under_threshold": 127, "output_difference": 13, "timeout_confirmed_local": 3}`。

归因诊断：30 条记录、30 次目标执行；诊断状态：`{"equal_under_threshold": 13, "output_difference": 17}`。两类合计 173 次目标执行。诊断仅解释既有差异，不扩充主测试覆盖；记录数也不等于独立 SHA 数或根因数。compile_blocked 是计划阻断标记，不是运行次数。完整逐次 CPU、墙钟、退出码、PID、输入与输出路径见 [summary.json](summary.json)。

主测试 13 个输入的正常完成输出差异均逐案归于第一轮 Pangu-01「停机未清除占据缓存」；新增正确性根因0、程序自行异常退出0。[逐案因果复核](../../work/output_triage.json)。诊断前缀产生的差异单列，不重新计缺陷。

压力候选、补充实测与未运行档位见 [timeout_candidates 索引](timeout_candidates/README.md)。

统一使用原输入检查器且不加 `--full`。原始输出只统一 CRLF 后精确比较；未删空白、未重排行、未忽略不可见字节。所有执行串行，截停只针对本轮启动的精确 PID。编译记录在 `../../build/compile.json`，原源码完整性与串行审计在 `../../work/integrity.json`。
