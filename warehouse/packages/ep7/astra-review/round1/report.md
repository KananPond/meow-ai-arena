**仓储调度程序临时独立回归报告（第一轮，2026-10-20）**

12 份均已检查。确认 Pangu 两处输出错误、Mercury 一处输出错误；LongCat 按指定命令编译失败，单列为交付问题。Dots、Wenxin、Qwen27B、Mistral、Hunyuan、Step、Haiku、Sonnet、Opus 本次未找到有效缺陷。未发现目标运行时崩溃。

仅以 `baseline/` 的实际输出为对照。逐份核对提交哈希并阅读全部源码与现有 6 份交接文档后，进行定向构造、系统组合和小规模差分。未联网、未调用其他 AI 或子代理、未访问其他比赛目录、未修改目标或基线。初末哈希均一致，证据见 [初始核对](work/hash_verification.json)、[最终核对](work/final_integrity.json)。编译产物和临时资料均在 `build/`、`work/`。

所有编译均使用 `g++ -O2 -std=c++20 -static`，包含每份 `src` 顶层全部 `.cpp`；基线包含其全部顶层 `.cpp`。[编译命令和退出码](work/compile.json)、[编译器版本](build/compiler.txt) 已保留。编译及被测程序均串行执行，实际同时运行的被测程序最多 1 个。未做性能测试、测速排名、大负载或超时攻击；没有输入被安全期限中止。耗时只保留为复现元数据和基线 10 秒资格核对，未用于缺陷比较。

共运行 443 个统一小用例，每个可执行目标均参加：69 个代码导向用例、200 个固定种子随机用例、44 个深层状态/缓存用例、40 个基于基线实时位置生成的用例、81 个边界组合、9 个几何快照。最大 T=145、事件数 167；最大 60×60 地图仅运行一个 tick、没有事件，用于热点边界；最多 40 台机器人。所有输入均通过与 `tools/check_input.py` 相同的 `check(full=False)`，保留反例还逐一实际运行了不带 `--full` 的命令行检查器。所有基线运行正常且小于 10 秒。未运行公开大负载或 full 专属输入；没有范围外补充发现。

检查覆盖了数值订单排序、机器人平局、同 tick 事件顺序、非法坐标的合法 REJECT 输入、重复封解、取送同格、被封起点与目的地、可达/不可达充电桩、精确电量门槛、充电占用与回退、零电先到达、SIDESTEP、REQUEUE/LOST、100 tick 救援、90% 充电完成、取消失败计数、延迟四舍五入，以及热点半径 0/400 和两类热点算法。基线本身的这些怪行为全部保留。[输入及规模清单](work/test_inventory.json)、[差异根因归类](work/root_cause_triage.json) 可核查覆盖和去重。

统一用例结果（差异数量为输入变体数，缺陷数量按独立根因计）：

| 提交 | 编译退出码 | 实际运行用例 | 输出一致 | 输出差异 | 已验证运行时根因 |
|---|---:|---:|---:|---:|---:|
| Pangu | 0 | 443 | 439 | 4 | 2 |
| Dots | 0 | 443 | 443 | 0 | 0 |
| Wenxin | 0 | 443 | 443 | 0 | 0 |
| LongCat | 1 | 0 | — | — | 0 |
| Qwen27B | 0 | 443 | 443 | 0 | 0 |
| Mistral | 0 | 443 | 443 | 0 | 0 |
| Hunyuan | 0 | 443 | 443 | 0 | 0 |
| Step | 0 | 443 | 443 | 0 | 0 |
| Mercury | 0 | 443 | 432 | 11 | 1 |
| Haiku | 0 | 443 | 443 | 0 | 0 |
| Sonnet | 0 | 443 | 443 | 0 | 0 |
| Opus | 0 | 443 | 443 | 0 | 0 |

LongCat 的日志阻断标记不算运行次数，也不算重复编译缺陷。有效运行时根因共 3 个，编译问题 1 个；没有把同一错误的随机变体重复计数。

**Pangu：已验证 2 处正确性缺陷**

检查覆盖：对照全部增量改动，重点检查单源 BFS 缓存失效、占据布尔表的移动/停机/救援更新、预计算桩位和热点、日志计数。针对停机释放、同格救援分别构造反例。

Pangu-01（停机时未清除占据缓存）：移动阶段开头 build_occ() 将存活机器人写入布尔占据表；进入 DEAD 的分支没有清除该格，后续机器人在同一 tick 仍读到旧占位。基线 OCC 每次扫描实时状态并跳过 DEAD。 6×4、T=5、2 台机器人、3 条事件。R0 原本可完成订单，tick 1 封路迫使它绕路，在 (3,2) 耗尽电量。tick 4 R0 先停机，基线允许 R1 随后进入 (3,2) 取货；目标的占据缓存仍将该格标为已占。 tick 4 基线输出 PICK 2 R1，目标未输出；R1 错误等待一轮，位置、电量、状态和 REPORT 均不同。 证据：[说明与验证命令](findings/Pangu/01_dead_occupancy/README.md)、[输入](findings/Pangu/01_dead_occupancy/case.in)、[退出码及单次复现记录](findings/Pangu/01_dead_occupancy/validation.json)。

Pangu-02（救援时未登记新增占位）：救援分支把 DEAD 改成 IDLE 后，没有在占据缓存中登记它。紧随其后检查的另一台同格 DEAD 机器人仍认为格子空闲，从而重复救活。到移动阶段才重建占据表已经太晚。 6×5、T=113、3 台机器人、9 条事件。R2 在 tick 4、R1 在 tick 9 分别停机于 (3,2)；R0 在 tick 12 占住该格，到 tick 111 离开。tick 112 两台停机机器人都已等满 100 tick。前面的全部输出一致，首个差异恰在第二次救援，独立于 Pangu-01 的同 tick 停机清位问题。 tick 112 基线只有 RESCUE R1，目标多输出 RESCUE R2；目标最终有两台 IDLE 机器人同处 (3,2)。 证据：[说明与验证命令](findings/Pangu/02_rescue_occupancy/README.md)、[输入](findings/Pangu/02_rescue_occupancy/case.in)、[退出码及单次复现记录](findings/Pangu/02_rescue_occupancy/validation.json)。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Pangu/README.md](findings/Pangu/README.md)；执行记录在下面六个日志中，按 `targets.Pangu` 查询。

**Dots：本次未找到**

检查覆盖：逐文件确认 .cpp/.h 与基线原字节相同，并独立编译、运行全部统一用例；仍核对动态封路、取消计数、派单平局、零电到达及救援。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Dots/README.md](findings/Dots/README.md)；执行记录在下面六个日志中，按 `targets.Dots` 查询。

**Wenxin：本次未找到**

检查覆盖：检查 BFS/BFS2 扁平缓冲区与 50000 容量静态队列、CHG_LIST 行优先顺序、PEND swap-and-pop、禁用旧缓存的实际调用路径。覆盖封锁起点、封锁同格终点；静态检查队列容量大于合法格子数上限。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Wenxin/README.md](findings/Wenxin/README.md)；执行记录在下面六个日志中，按 `targets.Wenxin` 查询。

**LongCat：编译失败，运行时检查受阻**

检查覆盖：阅读全部代码并与基线比较，检查缓存版本、占据更新、事件分桶和状态迁移；按规则包含 old_stuff.cpp 在内编译全部四个顶层 .cpp。编译阻断后未改代码继续运行。

LongCat-01（固定编译命令下 __m128i 未声明）：RunSim() 使用 __m128i 和 _mm_setzero_si128()，当前源码包含链没有提供其声明，指定的全部顶层 .cpp 编译命令直接失败。 原样编译共两次，均退出 1。未修改源码、补头文件或绕过该语句，也未运行不存在的目标二进制。附 3×3、T=1 的合法烟雾输入和正常基线输出作为资料；这不是运行时输出不一致反例。 编译退出码 1，诊断为 sim.cpp:70:9: error: __m128i was not declared in this scope。 证据：[说明与验证命令](findings/LongCat/01_compile_failure/README.md)、[输入](findings/LongCat/01_compile_failure/case.in)、[退出码及单次复现记录](findings/LongCat/01_compile_failure/validation.json)。

未验证怀疑：无另行保留的具体怀疑。动态行为未验证，因为原提交不能编译。 逐份证据入口：[findings/LongCat/README.md](findings/LongCat/README.md)；执行记录在下面六个日志中，按 `targets.LongCat` 查询。

**Qwen27B：本次未找到**

检查覆盖：检查反向取货点距离场推导、被封机器人用邻居加一、取送同格、缓存失效/指针消费顺序、占据表及旋转坐标热点。定向覆盖全部端点封锁组合与电量边界。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Qwen27B/README.md](findings/Qwen27B/README.md)；执行记录在下面六个日志中，按 `targets.Qwen27B` 查询。

**Mistral：本次未找到**

检查覆盖：逐文件确认 .cpp/.h 与基线原字节相同，仍独立编译、运行全部统一用例；检查输入边界和基线怪行为，未将源码相同作为豁免。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Mistral/README.md](findings/Mistral/README.md)；执行记录在下面六个日志中，按 `targets.Mistral` 查询。

**Hunyuan：本次未找到**

检查覆盖：检查 stamp 距离场与轮转槽位、封路 epoch、充电桩多源距离场及被封起点补偿、待派表交换删除和占据计数；用小图两 tick 的 167 订单用例覆盖缓存槽位淘汰。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Hunyuan/README.md](findings/Hunyuan/README.md)；执行记录在下面六个日志中，按 `targets.Hunyuan` 查询。

**Step：本次未找到**

检查覆盖：检查 LRU 缓存版本与指针消费、blockedTabs 独立缓冲、noChg 负缓存失效、chgCnt 状态迁移增减、待派 set 和热点半径快捷分支。覆盖断电重排队/救援当 tick 派单、充电停机与解封恢复。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Step/README.md](findings/Step/README.md)；执行记录在下面六个日志中，按 `targets.Step` 查询。

**Mercury：已验证 1 处正确性缺陷**

检查覆盖：检查 uint16 INF 转换、缓存失效、占据和充电占用增量更新、热点差分、待派 set 的节点生命周期；围绕订单号长度 1/7/8/9/10 位构造并复现释放后使用。

Mercury-01（删除待派集合节点后继续使用其订单号引用）：派单排序快照保存 set<string> 元素地址；id 引用该节点内字符串。PEND.erase(id) 销毁节点后，下一行仍拼接 id 输出 ASSIGN，发生释放后使用。 3×3、T=1、1 台机器人、1 条事件。订单取送点均为唯一充电桩，不涉及复杂路径或大负载。保留的相同输入连续三次复现错误；具体垃圾字节随进程地址变化。三次只统计一个根因。 首行应为 0 ASSIGN 1000000000 R0 0，目标订单号变成包含 NUL 等字节的损坏内容；随后的 PICK 仍使用正确订单号。 证据：[说明与验证命令](findings/Mercury/01_erased_order_id/README.md)、[输入](findings/Mercury/01_erased_order_id/case.in)、[退出码及单次复现记录](findings/Mercury/01_erased_order_id/validation.json)。

输出含不可见字节，请看 [转义差异](findings/Mercury/01_erased_order_id/output.escaped.diff) 和原始 `target.stdout`。重复运行记录不重复计数。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Mercury/README.md](findings/Mercury/README.md)；执行记录在下面六个日志中，按 `targets.Mercury` 查询。

**Haiku：本次未找到**

检查覆盖：检查 distUntil 提前停止、被封充电桩起点、同格 b=0、缓存清空时机、候选删除保持编号顺序、chargeUse 阶段撤销及热点前缀和。覆盖零电先到达、失单/重排队/救援。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Haiku/README.md](findings/Haiku/README.md)；执行记录在下面六个日志中，按 `targets.Haiku` 查询。

**Sonnet：本次未找到**

检查覆盖：检查哨兵网格与环形缓存、被封起点的反向距离、待派 set 顺序、占据唯一性以及 posVersion 热点缓存；覆盖直接统计和前缀和两个热点分支、停机/救援变化。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Sonnet/README.md](findings/Sonnet/README.md)；执行记录在下面六个日志中，按 `targets.Sonnet` 查询。

**Opus：本次未找到**

检查覆盖：检查选择性缓存失效 affects/refresh、源格封解不改变单源场的特例、多源充电场、computeBC 曼哈顿保守筛选、nextStep 局部 BFS 的层界和平局；定向覆盖频繁封解与恢复稳定后的移动。

已验证缺陷：本次未找到。全部 443 个统一用例输出与基线一致。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。 逐份证据入口：[findings/Opus/README.md](findings/Opus/README.md)；执行记录在下面六个日志中，按 `targets.Opus` 查询。

**执行证据与复现**

- [directed：69 个用例](work/results/directed/runs.jsonl)，[汇总](work/results/directed/aggregate.json)。
- [fuzz：200 个用例](work/results/fuzz/runs.jsonl)，[汇总](work/results/fuzz/aggregate.json)。
- [deep：44 个用例](work/results/deep/runs.jsonl)，[汇总](work/results/deep/aggregate.json)。
- [guided：40 个用例](work/results/guided/runs.jsonl)，[汇总](work/results/guided/aggregate.json)。
- [boundaries：81 个用例](work/results/boundaries/runs.jsonl)，[汇总](work/results/boundaries/aggregate.json)。
- [snapshots：9 个用例](work/results/snapshots/runs.jsonl)，[汇总](work/results/snapshots/aggregate.json)。

验证单个反例的命令在对应 `README.md` 中。批量复现可先运行 `python work/setup_review.py` 编译原样提交，再运行 `python work/review.py suite work/cases/<suite> recheck_<suite>`；此脚本串行运行、严格比较字节，仅统一 CRLF。测试脚本与生成器均保存在 `work/`。

结论仅适用于本次有限的正确性检查。未打正式分，未做可维护性盲评，未将未复现的风险或超时计为缺陷。机器可读结果见 [summary.json](summary.json)。
