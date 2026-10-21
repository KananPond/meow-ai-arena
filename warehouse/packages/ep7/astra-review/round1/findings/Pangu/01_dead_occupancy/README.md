**Pangu-01：停机时未清除占据缓存**

判定：已实际复现的有效正确性反例。合法性检查不使用 `--full`。

6×4、T=5、2 台机器人、3 条事件。R0 原本可完成订单，tick 1 封路迫使它绕路，在 (3,2) 耗尽电量。tick 4 R0 先停机，基线允许 R1 随后进入 (3,2) 取货；目标的占据缓存仍将该格标为已占。

根因：移动阶段开头 build_occ() 将存活机器人写入布尔占据表；进入 DEAD 的分支没有清除该格，后续机器人在同一 tick 仍读到旧占位。基线 OCC 每次扫描实时状态并跳过 DEAD。

源码位置：`targets/Pangu/src/sim.cpp:314`；`targets/Pangu/src/util.cpp:126`；`targets/Pangu/src/util.cpp:136`。

触发条件：较小编号机器人耗尽电量后停机；较大编号机器人在同一 tick 需要进入其原位置。

首次差异：tick 4 基线输出 PICK 2 R1，目标未输出；R1 错误等待一轮，位置、电量、状态和 REPORT 均不同。

复现实录：输入检查退出 0；基线退出 0，单次墙钟 0.007127 秒；目标退出 0；单次墙钟 0.008713 秒。时间仅作为本次复现元数据，不用于性能比较或缺陷判定；未做超时复测。

从本轮根目录验证：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Pangu.exe targets/Pangu/src/main.cpp targets/Pangu/src/old_stuff.cpp targets/Pangu/src/sim.cpp targets/Pangu/src/util.cpp
python tools/check_input.py findings/Pangu/01_dead_occupancy/case.in
python work/review.py verify Pangu findings/Pangu/01_dead_occupancy/case.in work/recheck_Pangu-01
```

编译使用全部顶层 `.cpp`。

证据：`baseline.stdout`、`target.stdout`、`output.diff`、两侧 `.stderr`。完整命令、PID、退出码、输出哈希及时间在 `validation.json` 中；比较只将 CRLF 统一为 LF。
