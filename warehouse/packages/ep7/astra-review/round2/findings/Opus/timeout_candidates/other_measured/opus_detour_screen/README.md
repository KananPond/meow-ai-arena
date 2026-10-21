# Opus 补充实测：opus_detour_screen

构造：60×60、T=120、2000张重复端点订单；U形墙迫使封锁取货点到送货点绕行171步，送货点就是充电桩；机器人无法进入取货点，每 tick 切换(30,30)。

预期成本与检查目的：基线在 a=INF 处跳过。此构造主要筛查 Opus：变更格的曼哈顿下界115小于171，两种封解均触发 mayChange，每单独立 pointDist 接近遍历大分量；Sonnet 可共享两张来源场。其他交叉测试按各自实测判断。

根因归属：属于已知提前距离计算根因的放大；重复端点与选择性失效没有另计新根因。

普通输入检查退出码 0；full=False。基线 CPU/墙钟 0.969/0.990 完成 (exit=0) 秒；基线资格 合格。

Opus CPU/墙钟：2.188/2.275 完成 (exit=0) 秒；状态 `equal_under_threshold`。

结论：目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

构造/源码依据：根目录 `work/agent_so/audit.md`。探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

输入为 [case.in](case.in)，完整记录为 [evidence.json](evidence.json)。该目录保留原始 stdout/stderr 与差异；多个目标共用的证据文件保留所有目标记录，本文结论只针对标题目标。只统一 CRLF 后比较，截停的输出缺尾不算新正确性缺陷。
