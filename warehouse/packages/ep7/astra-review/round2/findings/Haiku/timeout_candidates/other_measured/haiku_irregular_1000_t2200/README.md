# Haiku 补充实测：haiku_irregular_1000_t2200

构造：固定种子不规则墙、1000张同端点订单、T=2200；取货点等于空闲机器人位置，送货点永久封锁，充电桩与大分量隔离；每 tick 封解一格。

预期成本与检查目的：a=0 且 b=INF，基线立即短路；Haiku 仍逐单求 c，地图版本变化使订单缓存失效，预期 Θ(T·P·V) 搜索。

根因归属：这是较长的资格筛查档；合格正式代表为同机制的 haiku_irregular_1000_t1800，不重复计根因。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 9.906/10.016 截停 (exit=1) 秒；基线资格 不合格。

结论：基线未满足正常退出且墙钟不超过10秒的资格；本档不计有效性能反例，未运行目标。

构造/源码依据：根目录 `work/agent_smh/static_audit.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
