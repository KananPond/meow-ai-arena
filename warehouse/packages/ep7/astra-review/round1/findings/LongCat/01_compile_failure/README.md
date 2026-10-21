**LongCat-01：固定编译命令下 __m128i 未声明**

判定：编译失败（单列交付问题）。合法性检查不使用 `--full`。

原样编译共两次，均退出 1。未修改源码、补头文件或绕过该语句，也未运行不存在的目标二进制。附 3×3、T=1 的合法烟雾输入和正常基线输出作为资料；这不是运行时输出不一致反例。

根因：RunSim() 使用 __m128i 和 _mm_setzero_si128()，当前源码包含链没有提供其声明，指定的全部顶层 .cpp 编译命令直接失败。

源码位置：`targets/LongCat/src/sim.cpp:70`。

触发条件：按 g++ -O2 -std=c++20 -static 编译封存 src 顶层全部 .cpp；不依赖输入。

首次差异：编译退出码 1，诊断为 sim.cpp:70:9: error: __m128i was not declared in this scope。

复现实录：输入检查退出 0；基线退出 0，单次墙钟 0.004363 秒；目标未运行（编译退出 1）。时间仅作为本次复现元数据，不用于性能比较或缺陷判定；未做超时复测。

从本轮根目录验证：

```powershell
g++ -O2 -std=c++20 -static -o build/baseline.exe baseline/main.cpp baseline/old_stuff.cpp baseline/sim.cpp baseline/util.cpp
g++ -O2 -std=c++20 -static -o build/LongCat.exe targets/LongCat/src/main.cpp targets/LongCat/src/old_stuff.cpp targets/LongCat/src/sim.cpp targets/LongCat/src/util.cpp
python tools/check_input.py findings/LongCat/01_compile_failure/case.in
python work/review.py verify LongCat findings/LongCat/01_compile_failure/case.in work/recheck_LongCat-01
```

编译使用全部顶层 `.cpp`。LongCat 的第二条命令预期编译失败，最后一条会记录 `compile_failure`，不会尝试启动目标。

证据：`compile.stderr`、`compile.json`、`NOT_RUN.txt`。完整命令、PID、退出码、输出哈希及时间在 `validation.json` 中；比较只将 CRLF 统一为 LF。
