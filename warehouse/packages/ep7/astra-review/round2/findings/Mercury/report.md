# Mercury 第二轮报告

新增本地已复现根因 1 个。

2000场缓存满时全清，1998/2000/2002来源测到断层，归已知提前计算根因。排序键已预提取，字符串 ORD 长链仍使两次全遍历为 Θ(P²)。

源码覆盖和排除理由：[审计文档](../../work/agent_smh/static_audit.md)。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

- [N01_order_lookup 订单表同桶长链使两次待派遍历退化](timeout_candidates/N01_order_lookup/README.md)：基线 7.297/7.390 秒 CPU/墙钟；目标 87.703/88.654 完成 (exit=0); 58.109/60.013 截停 (exit=1); 58.625/60.015 截停 (exit=1)。

已知问题（不计新增）：erase 待派节点后继续使用订单号引用（第一轮）；提前计算订单距离及地图变化失效（旧统一池 round2_4985a0f30877）

历史出处：`../../learning/first_round/report.md`、`../../learning/legacy_recheck/ep7_recheck.json`。旧超时记录不充当本轮实测；LongCat 的多个旧池编译错误不重复计数。

未另行确认其他独立性能根因；静态疑点、生成未运行档位和不合格输入没有升级为有效发现。

主测试：149 条目标测试记录，其中 149 条有实际目标执行（149 个独立输入 SHA），含复测共 151 次目标执行。主测试状态：`{"equal_under_threshold": 148, "timeout_confirmed_local": 1}`。

归因诊断：0 条记录、0 次目标执行；诊断状态：`{}`。两类合计 151 次目标执行。诊断仅解释既有差异，不扩充主测试覆盖；记录数也不等于独立 SHA 数或根因数。compile_blocked 是计划阻断标记，不是运行次数。完整逐次 CPU、墙钟、退出码、PID、输入与输出路径见 [summary.json](summary.json)。

压力候选、补充实测与未运行档位见 [timeout_candidates 索引](timeout_candidates/README.md)。

统一使用原输入检查器且不加 `--full`。原始输出只统一 CRLF 后精确比较；未删空白、未重排行、未忽略不可见字节。所有执行串行，截停只针对本轮启动的精确 PID。编译记录在 `../../build/compile.json`，原源码完整性与串行审计在 `../../work/integrity.json`。
