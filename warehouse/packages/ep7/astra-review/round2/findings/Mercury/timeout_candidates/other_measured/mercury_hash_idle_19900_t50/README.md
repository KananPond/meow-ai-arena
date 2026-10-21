# Mercury 补充实测：mercury_hash_idle_19900_t50

构造：19900个合法十进制订单号在本机 std::hash<string> 的20753桶中全部落桶0；T=50，机器人 IDLE，取送两点均封锁。

预期成本与检查目的：Mercury 构造排序键与派单正文各完整查询一次 ORD，链式桶模型下为 Θ(T·P²) 节点访问；键已预提取，不能再附加排序比较器的 log P 因子。无 ASSIGN，b=INF，无充电距离搜索。

根因归属：这是较长的基线资格筛查档；合格正式代表为 T=40，属于同一新查找退化根因。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 9.812/10.009 截停 (exit=1) 秒；基线资格 不合格。

结论：基线未满足正常退出且墙钟不超过10秒的资格；本档不计有效性能反例，未运行目标。

构造/源码依据：根目录 `work/hash_distribution/inspect.json；work/agent_dmwl/final_method_review.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
