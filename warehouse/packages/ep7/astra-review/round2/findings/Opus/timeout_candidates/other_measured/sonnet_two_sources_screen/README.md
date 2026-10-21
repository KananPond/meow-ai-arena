# Opus 补充实测：sonnet_two_sources_screen

构造：60×60、T=160、700个不同且永久封锁的取货点与700个不同可走送货点；每 tick 切换(57,58)，唯一机器人无法进入任何取货点。

预期成本与检查目的：基线在 a=INF 处跳过。此构造主要筛查 Sonnet 的提前 b/c 求值与全局版本失效：封源仍可向外搜索，因而可能重建两组来源的场。交叉测试其他目标只记录各自实测，不能直接套用 Sonnet 的内部机制。

根因归属：提前距离计算在相应旧池中已有记录；本档用于新构造筛查，不增加独立根因。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 0.375/0.404 完成 (exit=0) 秒；基线资格 合格。

Opus CPU/墙钟：0.219/0.219 完成 (exit=0) 秒；状态 `equal_under_threshold`。

结论：目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

构造/源码依据：根目录 `work/agent_so/audit.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
