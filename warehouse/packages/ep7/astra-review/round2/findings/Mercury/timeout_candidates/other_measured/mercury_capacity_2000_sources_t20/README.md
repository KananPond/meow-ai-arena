# Mercury 补充实测：mercury_capacity_2000_sources_t20

构造：T=20，1000个不同且封锁的取货点配同样数量不同可走送货点，总计2000个来源；初始封锁后地图不再变化。

预期成本与检查目的：基线因 a=INF 跳过；Mercury 提前计算 b/c，请求两组来源。2000场缓存满时全清，1998/2000/2002对照检查容量阈值是否产生重复重建。

根因归属：容量断层放大了旧池已确认的提前距离计算根因，不单独计新；短档的实际耗时只按记录报告。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 0.109/0.104 完成 (exit=0) 秒；基线资格 合格。

Mercury CPU/墙钟：0.047/0.064 完成 (exit=0) 秒；状态 `equal_under_threshold`。

结论：目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

构造/源码依据：根目录 `work/agent_smh/static_audit.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
