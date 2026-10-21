# Hunyuan 第二轮报告

本轮未复现新增有效回归；这不是无缺陷证明。

普通容量 min(512,3R+128)=131..248，stamp 场轮转；每订单 b 按 epoch 缓存、多源充电场按 epoch 缓存，先看候选及 a。跨248场边界已检查，未发现存活引用跨 fieldFrom 被覆盖。

源码覆盖和排除理由：[审计文档](../../work/agent_pqh/hunyuan/audit.json)。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。


已知问题（不计新增）：现有学习材料没有该实现的已确认根因。

历史出处：`../../learning/first_round/report.md`、`../../learning/legacy_recheck/ep7_recheck.json`。旧超时记录不充当本轮实测；LongCat 的多个旧池编译错误不重复计数。

孤立查询仍按 epoch 重建远方多源充电场：0.047/0.045 完成 (exit=0)。目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

主测试：138 条目标测试记录，其中 138 条有实际目标执行（138 个独立输入 SHA），含复测共 138 次目标执行。主测试状态：`{"equal_under_threshold": 138}`。

归因诊断：0 条记录、0 次目标执行；诊断状态：`{}`。两类合计 138 次目标执行。诊断仅解释既有差异，不扩充主测试覆盖；记录数也不等于独立 SHA 数或根因数。compile_blocked 是计划阻断标记，不是运行次数。完整逐次 CPU、墙钟、退出码、PID、输入与输出路径见 [summary.json](summary.json)。

压力候选、补充实测与未运行档位见 [timeout_candidates 索引](timeout_candidates/README.md)。

统一使用原输入检查器且不加 `--full`。原始输出只统一 CRLF 后精确比较；未删空白、未重排行、未忽略不可见字节。所有执行串行，截停只针对本轮启动的精确 PID。编译记录在 `../../build/compile.json`，原源码完整性与串行审计在 `../../work/integrity.json`。
