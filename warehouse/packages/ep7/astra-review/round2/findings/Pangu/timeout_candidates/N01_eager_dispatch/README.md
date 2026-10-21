# Pangu N01_eager_dispatch：没有可派机器人仍逐单计算取送及充电距离

本轮新增、按指定规则在本机复现；主办方空闲时复核仍为最终超时判定依据。

R0 在 tick 0 取货，tick 1 封住送货点后始终 DELIVERING。600 张新单一直待派，地图此后不再变化。目标在检查机器人状态之前算 b、c，其中 c 每次独立全图 BFS；基线除 tick 0 首单的有限搜索外，从 tick 1 起没有派单 BFS。单槽 b 缓存也在不同取货来源之间反复覆盖。

本输入目标新增 Θ(T·P·V) 搜索；基线只承担事件扫描、待派排序和常数机器人检查。这里 P=600、T=3000，每段各有 1,799,400 次搜索机会。b/c 提前求值按一个控制流根因计数。

去重：第一轮两个 Pangu 占据根因不涉及此路径；本输入没有 DEAD、RESCUE 或多机器人占据。旧统一复测中 Pangu 记录为空。

代码位置：targets/Pangu/src/sim.cpp:189, targets/Pangu/src/util.cpp:159。详细源码审计见根目录 `work/agent_pqh/audit.json`。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

普通输入检查退出码 0，没有使用 `--full`。基线 CPU/墙钟：5.281/5.327 秒，退出码 0。目标 CPU/墙钟：97.688/100.004 截停 (exit=1)。

首测上限100秒；首测在60至100秒完成时，只有追加两次各60秒都截停才确认。本目录 `evidence.json` 保存每次记录，原始 stdout/stderr、完整字节差异和转义差异均保留。截停进程的输出标为不完整，缺少尾部日志不另算输出错误；exit=1 如由 harness 终止也不算程序自行异常退出。

从工作区根目录复现：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Pangu.exe targets/Pangu/src/main.cpp targets/Pangu/src/old_stuff.cpp targets/Pangu/src/sim.cpp targets/Pangu/src/util.cpp
python tools/check_input.py findings/Pangu/timeout_candidates/N01_eager_dispatch/case.in
python work/audit.py case findings/Pangu/timeout_candidates/N01_eager_dispatch/case.in recheck_Pangu_N01_eager_dispatch --targets Pangu
```

精确编译参数与全部顶层源文件在根目录 `build/compile.json`，统一 `g++ -O2 -std=c++20 -static`。每份 `src` 的顶层全部 `.cpp` 均参与，基线也包含全部顶层 `.cpp`。运行器对基线限10秒、对目标按正式规则限时，且使用独占锁串行执行。复测标签须使用尚不存在的新名称。

对照/筛查记录位于 `controls/`；这些输入变体不重复计根因。
