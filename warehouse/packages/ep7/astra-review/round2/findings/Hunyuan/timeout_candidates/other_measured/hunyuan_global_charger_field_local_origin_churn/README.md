# Hunyuan 补充实测：hunyuan_global_charger_field_local_origin_churn

构造：孤立机器人所在小分量中 a=b=0，充电桩在远方大分量；每轮实际封解使 epoch 更新。

预期成本与检查目的：基线的充电 BFS 只遍历本地分量（仍有初始化/桩扫描），Hunyuan 会重建远方多源充电场。普通上限仅约千万级访问，静态分析未预测必然超过60秒。

根因归属：保留全局场重建方向的实测或未运行状态，不以速度比认定缺陷。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 1.859/1.921 完成 (exit=0) 秒；基线资格 合格。

Hunyuan CPU/墙钟：0.047/0.045 完成 (exit=0) 秒；状态 `equal_under_threshold`。

结论：目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

构造/源码依据：根目录 `work/agent_pqh/hunyuan/audit.json`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
