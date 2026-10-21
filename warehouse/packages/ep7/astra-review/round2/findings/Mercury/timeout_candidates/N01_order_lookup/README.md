# Mercury N01_order_lookup：订单表同桶长链使两次待派遍历退化

本轮新增、按指定规则在本机复现；主办方空闲时复核仍为最终超时判定依据。

19900 个 1..10^9 内的十进制编号按本机 std::hash<string> 生成，全部落入 20753 桶表的桶 0。目标在构造排序键和派单正文中各遍历所有订单并查 ORD。机器人始终 IDLE，取送点被封；没有 ASSIGN、节点删除或任何充电距离搜索。

每轮两次完整查询集合造成 Θ(P²) 链节点访问，总 Θ(T·P²)；排序键已预提取，因此不把 Pangu/Qwen 的比较器额外 log P 因子套在 Mercury 上。基线平衡树避免此线性桶链。

去重：独立于第一轮 erase 后引用错误，也独立于旧池提前 b/c 的已知性能根因。代表没有 ASSIGN，不会触发旧订单号错误；b 直接 INF，距离缓存不参与放大。

代码位置：targets/Mercury/src/main.cpp:16, targets/Mercury/src/sim.cpp:80, targets/Mercury/src/sim.cpp:91。详细源码审计见根目录 `work/agent_dmwl/final_method_review.md`。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

普通输入检查退出码 0，没有使用 `--full`。基线 CPU/墙钟：7.297/7.390 秒，退出码 0。目标 CPU/墙钟：87.703/88.654 完成 (exit=0); 58.109/60.013 截停 (exit=1); 58.625/60.015 截停 (exit=1)。

首测上限100秒；首测在60至100秒完成时，只有追加两次各60秒都截停才确认。本目录 `evidence.json` 保存每次记录，原始 stdout/stderr、完整字节差异和转义差异均保留。截停进程的输出标为不完整，缺少尾部日志不另算输出错误；exit=1 如由 harness 终止也不算程序自行异常退出。

从工作区根目录复现：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Mercury.exe targets/Mercury/src/main.cpp targets/Mercury/src/sim.cpp targets/Mercury/src/util.cpp
python tools/check_input.py findings/Mercury/timeout_candidates/N01_order_lookup/case.in
python work/audit.py case findings/Mercury/timeout_candidates/N01_order_lookup/case.in recheck_Mercury_N01_order_lookup --targets Mercury
```

精确编译参数与全部顶层源文件在根目录 `build/compile.json`，统一 `g++ -O2 -std=c++20 -static`。每份 `src` 的顶层全部 `.cpp` 均参与，基线也包含全部顶层 `.cpp`。运行器对基线限10秒、对目标按正式规则限时，且使用独占锁串行执行。复测标签须使用尚不存在的新名称。

对照/筛查记录位于 `controls/`；这些输入变体不重复计根因。哈希桶构造在本机编译器/标准库下验证，分布详情在根目录 `work/hash_distribution/inspect.json`；更换标准库时需重新核对桶策略。
