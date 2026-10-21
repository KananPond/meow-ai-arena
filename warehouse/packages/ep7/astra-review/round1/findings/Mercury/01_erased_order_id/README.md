**Mercury-01：删除待派集合节点后继续使用其订单号引用**

判定：已实际复现的有效正确性反例。合法性检查不使用 `--full`。

3×3、T=1、1 台机器人、1 条事件。订单取送点均为唯一充电桩，不涉及复杂路径或大负载。保留的相同输入连续三次复现错误；具体垃圾字节随进程地址变化。三次只统计一个根因。

根因：派单排序快照保存 set<string> 元素地址；id 引用该节点内字符串。PEND.erase(id) 销毁节点后，下一行仍拼接 id 输出 ASSIGN，发生释放后使用。

源码位置：`targets/Mercury/src/sim.cpp:75`；`targets/Mercury/src/sim.cpp:90`；`targets/Mercury/src/sim.cpp:118`；`targets/Mercury/src/sim.cpp:119`。

触发条件：订单获派后进入日志拼接。本机固定编译参数下，九位和十位合法订单号可实际触发；内存问题本身不限于这些编号长度。

首次差异：首行应为 0 ASSIGN 1000000000 R0 0，目标订单号变成包含 NUL 等字节的损坏内容；随后的 PICK 仍使用正确订单号。

复现实录：输入检查退出 0；基线退出 0，单次墙钟 0.004613 秒；目标退出 0；单次墙钟 0.009588 秒。时间仅作为本次复现元数据，不用于性能比较或缺陷判定；未做超时复测。

从本轮根目录验证：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/Mercury.exe targets/Mercury/src/main.cpp targets/Mercury/src/sim.cpp targets/Mercury/src/util.cpp
python tools/check_input.py findings/Mercury/01_erased_order_id/case.in
python work/review.py verify Mercury findings/Mercury/01_erased_order_id/case.in work/recheck_Mercury-01
```

编译使用全部顶层 `.cpp`。

证据：`baseline.stdout`、`target.stdout`、`output.diff`、两侧 `.stderr`；可读转义差异 `output.escaped.diff`、原始字节 `target.stdout.hex`，重复验证见 `repeat_2/` 与 `repeat_3/`。完整命令、PID、退出码、输出哈希及时间在 `validation.json` 中；比较只将 CRLF 统一为 LF。
