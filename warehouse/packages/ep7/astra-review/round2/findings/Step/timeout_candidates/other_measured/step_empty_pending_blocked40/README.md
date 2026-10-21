# Step 补充实测：step_empty_pending_blocked40

构造：60×60、T=3000、40台 IDLE 机器人；无订单，初始40个机器人位置被封且邻格可走。

预期成本与检查目的：Step 在判断待派集合为空之前为被封候选预建 blockedTabs，预期产生 Θ(T·R·V) 的额外全图搜索；基线无需派单搜索。

根因归属：这是独立静态怀疑的最大普通档实测；只有正式计时达标才可计缺陷。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 0.031/0.029 完成 (exit=0) 秒；基线资格 合格。

Step CPU/墙钟：1.672/1.718 完成 (exit=0) 秒；状态 `equal_under_threshold`。

结论：目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

构造/源码依据：根目录 `work/agent_smh/static_audit.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
