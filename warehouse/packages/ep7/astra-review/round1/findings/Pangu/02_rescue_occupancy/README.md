**Pangu-02：救援时未登记新增占位**

判定：已实际复现的有效正确性反例。合法性检查不使用 `--full`。

6×5、T=113、3 台机器人、9 条事件。R2 在 tick 4、R1 在 tick 9 分别停机于 (3,2)；R0 在 tick 12 占住该格，到 tick 111 离开。tick 112 两台停机机器人都已等满 100 tick。前面的全部输出一致，首个差异恰在第二次救援，独立于 Pangu-01 的同 tick 停机清位问题。

根因：救援分支把 DEAD 改成 IDLE 后，没有在占据缓存中登记它。紧随其后检查的另一台同格 DEAD 机器人仍认为格子空闲，从而重复救活。到移动阶段才重建占据表已经太晚。

源码位置：`targets/Pangu/src/sim.cpp:146`；`targets/Pangu/src/util.cpp:136`。

触发条件：多台 DEAD 机器人停在同一格且已达到救援等待期；活机器人离开该格后，两台停机机器人在同一个救援阶段被依次检查。

首次差异：tick 112 基线只有 RESCUE R1，目标多输出 RESCUE R2；目标最终有两台 IDLE 机器人同处 (3,2)。

复现实录：输入检查退出 0；基线退出 0，单次墙钟 0.005533 秒；目标退出 0；单次墙钟 0.004659 秒。时间仅作为本次复现元数据，不用于性能比较或缺陷判定；未做超时复测。

从本轮根目录验证：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Pangu.exe targets/Pangu/src/main.cpp targets/Pangu/src/old_stuff.cpp targets/Pangu/src/sim.cpp targets/Pangu/src/util.cpp
python tools/check_input.py findings/Pangu/02_rescue_occupancy/case.in
python work/review.py verify Pangu findings/Pangu/02_rescue_occupancy/case.in work/recheck_Pangu-02
```

编译使用全部顶层 `.cpp`。

证据：`baseline.stdout`、`target.stdout`、`output.diff`、两侧 `.stderr`。完整命令、PID、退出码、输出哈希及时间在 `validation.json` 中；比较只将 CRLF 统一为 LF。
