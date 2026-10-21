# Qwen27B N01_order_lookup：整数订单表长链使每 tick 查找和排序退化

本轮新增、按指定规则在本机复现；主办方空闲时复核仍为最终超时判定依据。

4000 个合法编号取 1+5087·k，本机整数哈希为恒等映射、最终表为 5087 桶，因此全部进入同桶。目标每 tick 在排序及正文反复按编号查询 ORD。机器人一直 IDLE，取送点被封，固定三格距离场仅首次建立，后续不搜索。连续编号对照只更换 ID。

完整遍历对应 Θ(T·P²) 链节点访问；比较器的最坏上界 O(T·P² log P)。基线平衡树排序 O(T·P log² P)，并有 O(T·E) 事件扫描。

去重：旧池确认的是提前距离计算和地图失效。本输入无地图变化，距离搜索常量且微小；更换订单存储可消除本根因，但不改变提前距离计算位置。

代码位置：targets/Qwen27B/src/common.h:51, targets/Qwen27B/src/sim.cpp:214, targets/Qwen27B/src/sim.cpp:223。详细源码审计见根目录 `work/agent_dmwl/hash_review.md`。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

普通输入检查退出码 0，没有使用 `--full`。基线 CPU/墙钟：6.484/6.495 秒，退出码 0。目标 CPU/墙钟：99.016/100.004 截停 (exit=1)。

首测上限100秒；首测在60至100秒完成时，只有追加两次各60秒都截停才确认。本目录 `evidence.json` 保存每次记录，原始 stdout/stderr、完整字节差异和转义差异均保留。截停进程的输出标为不完整，缺少尾部日志不另算输出错误；exit=1 如由 harness 终止也不算程序自行异常退出。

从工作区根目录复现：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Qwen27B.exe targets/Qwen27B/src/main.cpp targets/Qwen27B/src/sim.cpp targets/Qwen27B/src/util.cpp
python tools/check_input.py findings/Qwen27B/timeout_candidates/N01_order_lookup/case.in
python work/audit.py case findings/Qwen27B/timeout_candidates/N01_order_lookup/case.in recheck_Qwen27B_N01_order_lookup --targets Qwen27B
```

精确编译参数与全部顶层源文件在根目录 `build/compile.json`，统一 `g++ -O2 -std=c++20 -static`。每份 `src` 的顶层全部 `.cpp` 均参与，基线也包含全部顶层 `.cpp`。运行器对基线限10秒、对目标按正式规则限时，且使用独占锁串行执行。复测标签须使用尚不存在的新名称。

对照/筛查记录位于 `controls/`；这些输入变体不重复计根因。哈希桶构造在本机编译器/标准库下验证，分布详情在根目录 `work/hash_distribution/inspect.json`；更换标准库时需重新核对桶策略。
