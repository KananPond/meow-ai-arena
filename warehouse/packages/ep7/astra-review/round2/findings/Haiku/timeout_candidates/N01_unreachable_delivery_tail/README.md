# Haiku N01_unreachable_delivery_tail：取送距离已不可达仍逐单搜索充电距离

本轮新增、按指定规则在本机复现；主办方空闲时复核仍为最终超时判定依据。

1000 单取货点等于唯一空闲机器人位置，a=0；送货点永久封锁且不同于取货点，b=INF。基线立即短路。目标 routeCost 仍从送货点搜索 c，因充电桩隔离而遍历整个可达大区。每 tick 一处封解让所有订单的 costVersion 失效，重复送货点没有共享 c。固定种子不规则墙仅改变 BFS 常数，不增加基线搜索。

每次实际地图变化增加 Θ(P·V) 无用搜索，总 Θ(T·P·V)。1800 tick 共 1,800,000 次 c 搜索，两个封解状态分别访问 2533/2532 个节点。基线没有 a、b、c 的 BFS。

去重：Haiku 已正确跳过无候选或 a 不可达情形，旧统一池均 match。这里是 b 已不可达后的另一个短路位置，第一轮没有性能检查。

代码位置：targets/Haiku/src/sim.cpp:227, targets/Haiku/src/sim.cpp:239, targets/Haiku/src/grid.cpp:64。详细源码审计见根目录 `work/agent_smh/static_audit.md`。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

普通输入检查退出码 0，没有使用 `--full`。基线 CPU/墙钟：8.078/8.125 秒，退出码 0。目标 CPU/墙钟：75.031/75.879 完成 (exit=0); 59.500/60.004 截停 (exit=1); 59.547/60.002 截停 (exit=1)。

首测上限100秒；首测在60至100秒完成时，只有追加两次各60秒都截停才确认。本目录 `evidence.json` 保存每次记录，原始 stdout/stderr、完整字节差异和转义差异均保留。截停进程的输出标为不完整，缺少尾部日志不另算输出错误；exit=1 如由 harness 终止也不算程序自行异常退出。

从工作区根目录复现：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Haiku.exe targets/Haiku/src/grid.cpp targets/Haiku/src/main.cpp targets/Haiku/src/sim.cpp
python tools/check_input.py findings/Haiku/timeout_candidates/N01_unreachable_delivery_tail/case.in
python work/audit.py case findings/Haiku/timeout_candidates/N01_unreachable_delivery_tail/case.in recheck_Haiku_N01_unreachable_delivery_tail --targets Haiku
```

精确编译参数与全部顶层源文件在根目录 `build/compile.json`，统一 `g++ -O2 -std=c++20 -static`。每份 `src` 的顶层全部 `.cpp` 均参与，基线也包含全部顶层 `.cpp`。运行器对基线限10秒、对目标按正式规则限时，且使用独占锁串行执行。复测标签须使用尚不存在的新名称。

对照/筛查记录位于 `controls/`；这些输入变体不重复计根因。
