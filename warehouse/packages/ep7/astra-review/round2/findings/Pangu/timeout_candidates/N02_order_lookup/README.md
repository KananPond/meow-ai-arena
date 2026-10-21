# Pangu N02_order_lookup：字符串订单表长链使每 tick 查找和排序退化

本轮新增、按指定规则在本机复现；主办方空闲时复核仍为最终超时判定依据。

2000 个合法十进制订单号在本机 GNU 标准库的 2357 桶表中落入同一桶。机器人始终 IDLE，取货点和送货点都被封住；b 立即不可达，c 不被调用。排序比较器和派单正文反复查询 unordered_map，编号改为连续整数后长链消失。

在该链式表模型下，每 tick 完整查找 P 个键需要 Θ(P²) 节点访问；排序查找最坏上界 O(P² log P)，不声称每个比较都扫描整条链。基线使用平衡树，相关排序上界 O(P log² P)。

去重：与提前 BFS、停机清位、救援占位均不同。该代表中有空闲机器人、没有动态封解、没有 BFS 洪泛和状态迁移。标准库桶策略相关，证据针对指定本机编译环境。

代码位置：targets/Pangu/src/common.h:50, targets/Pangu/src/sim.cpp:179, targets/Pangu/src/sim.cpp:187。详细源码审计见根目录 `work/agent_dmwl/hash_review.md`。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

普通输入检查退出码 0，没有使用 `--full`。基线 CPU/墙钟：4.844/4.902 秒，退出码 0。目标 CPU/墙钟：99.125/100.013 截停 (exit=1)。

首测上限100秒；首测在60至100秒完成时，只有追加两次各60秒都截停才确认。本目录 `evidence.json` 保存每次记录，原始 stdout/stderr、完整字节差异和转义差异均保留。截停进程的输出标为不完整，缺少尾部日志不另算输出错误；exit=1 如由 harness 终止也不算程序自行异常退出。

从工作区根目录复现：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Pangu.exe targets/Pangu/src/main.cpp targets/Pangu/src/old_stuff.cpp targets/Pangu/src/sim.cpp targets/Pangu/src/util.cpp
python tools/check_input.py findings/Pangu/timeout_candidates/N02_order_lookup/case.in
python work/audit.py case findings/Pangu/timeout_candidates/N02_order_lookup/case.in recheck_Pangu_N02_order_lookup --targets Pangu
```

精确编译参数与全部顶层源文件在根目录 `build/compile.json`，统一 `g++ -O2 -std=c++20 -static`。每份 `src` 的顶层全部 `.cpp` 均参与，基线也包含全部顶层 `.cpp`。运行器对基线限10秒、对目标按正式规则限时，且使用独占锁串行执行。复测标签须使用尚不存在的新名称。

对照/筛查记录位于 `controls/`；这些输入变体不重复计根因。哈希桶构造在本机编译器/标准库下验证，分布详情在根目录 `work/hash_distribution/inspect.json`；更换标准库时需重新核对桶策略。
