# 交接文档（给接手改造需求的队友）

**先读这一段**：本目录是仓储调度仿真 `src/` 的性能重写版。**对外行为与 `baseline/` 逐字节一致**（5 组公开负载 + 多组自造负载与基线差分对拍通过），CPU 时间约为基线的 **1/90 ~ 1/300**（公开负载合计 92.2s → 0.95s）。改造需求（派单 / 电量 / 报表）时，请按下面的地图去找代码，并随时用 `python tools/bench.py` 和文末的对拍方法验证行为。

## 1. 文件结构

| 文件 | 内容 |
|---|---|
| `src/common.h` | 全部全局变量、枚举、数据结构。字段含义与基线的字符串状态一一对应 |
| `src/main.cpp` | 全局变量定义 + `main`（读入 → 仿真 → 收尾 → 输出） |
| `src/util.cpp` | 输出缓冲、BFS、**距离表 LRU 缓存**（性能核心） |
| `src/sim.cpp` | `ReadAll`（输入解析）+ `RunSim`（7 个阶段的主循环）+ `Finish`（收尾统计） |
| ~~`src/old_stuff.cpp`~~ | **已删除**。它是 `USE_NEW_DISPATCH=0` 时才走的旧派单路径，基线里从未启用；`common.h` 里对应的声明也一并删了。基线行为完全不受影响 |

编译仍是固定命令 `g++ -O2 -std=c++20 src/*.cpp`（Windows 加 `-static`）。

## 2. 数据表示（和基线的对应关系）

| 基线 | 现在 |
|---|---|
| `RST[i]`: `"IDLE"/"TO_PICKUP"/...` 字符串 | `vector<int> RST`，枚举 `ST_IDLE, ST_TO_PICKUP, ST_DELIVERING, ST_TO_CHARGER, ST_CHARGING, ST_DEAD`（输出时用 `RSTNAME[]` 转回字符串） |
| `ORD`: `map<string, vector<string>>`，字段全是字符串 | `vector<Order> ORD` + `unordered_map<long long,int> ORDIDX`（订单编号是规范十进制正整数，可直接当整数键）。字段：`id px py dx dy prio arrive status robot` |
| `PEND`: `vector<string>`，每 tick 拷贝+`sort` | `set<PendNode, PendCmp> PENDQ`，比较器 = **优先级降 → 到达 tick 升 → 编号升**，与基线 `sort` 的顺序完全一致，增删都是 `O(log n)` |
| `LOGBUF` + `writeLog`/`writeRaw` | `OUTBUF`（一个大气 string）+ `outInt/outS/outEnd`，最后一次性 `fwrite` 到 stdout |
| `OCC(x,y,except)` 全机器人扫描 | `vector<int> OCCG`（格子 → 机器人下标+1，**DEAD 不占格**），移动/停机/救援时增量维护 |
| `set<pair<int,int>> BLK` | `vector<uint8_t> BLKF` + `PASS`（`!墙 && !封锁`），`PASSVER` 记录通行性版本号 |
| 每 tick 从头扫全部事件 | `vector<vector<Ev>> EVT`，`ReadAll` 时按 tick 分好桶，顺序不变 |

`MAP`（原始字符串地图）只保留 `WALL`/`PASS`/充电桩列表由它派生，报表和收尾不再直接用它。

## 3. 主循环（`RunSim`）的阶段顺序 —— 和基线一模一样，别改顺序

1. **事件**（`EVT[t]`，输入顺序）2. **救援** 3. **充电** 4. **派单** 5. **低电量去充电** 6. **移动** 7. **报表**。
   后面的阶段能看到前面的状态变化（例如派单会把机器人变成 `TO_PICKUP`，移动按编号顺序、前车会影响后车的占位判断）。
   收尾在 `Finish()`：SUMMARY / LATENCY / 每车一行。

## 4. 性能优化都改在哪（想再优化就看这里）

1. **距离表 LRU 缓存**（`util.cpp` `DistCache`）：BFS 结果只取决于（起点, 通行性）。通行性只在 `BLOCK/UNBLOCK` **真正改变格子状态**时变化，此时 `PASSVER++`，缓存即整体失效。所以同一版本内、同一起点的 BFS 只算一次（基线每次都重算）。移动阶段的 `BFS2(目标)`、派单的 `DIST(取货点)`、`CHG_DIST(送货点)` 全部走这个缓存。
   - 缓存容量按 288MB 内存预算自适应（`W*H ≤ 40000` 时约 1800 张表），远低于 1GB 上限。
   - **注意**：`dcacheGet()` 返回的指针只在**下一次 `dcacheGet()` 之前**有效（LRU 淘汰可能复用槽位）。派单里因此先把 `a`、`b` 从取货点表里读出来，再去取送货点表。
2. **事件预解析分桶**：基线每 tick 都 `SPLIT` 全部 `E` 条事件（`O(T*E)`，这是最大的黑洞），现在 `O(E)`。
3. **派单的两段式 + 惰性 b/c**：先对每台候选车算 `a = dist(车→取货点)`，只有当"存在电量足够大的候选车"（`RB >= a + safetyMargin`，b/c 非负所以这是必要条件）时才算 `b = dist(取货→送货)` 和 `c = min dist(送货→充电桩)`。不可达/电不够的订单一棵 BFS 都不做。
4. **占用网格 / 充电桩占用计数**：`OCC` 从 `O(NR)` 降为 `O(1)`；"充电桩被占用"（`CHARGING` 站在桩上 或 `TO_CHARGER` 目标是该桩）用 `chgCnt[]` 计数，在状态迁移处增减（`GO_CHARGE` +1、`CHARGED` -1、停机 -1；`TO_CHARGER` 到桩变 `CHARGING` 不增减，因为占用关系没变）。
5. **报表热点**：原来是每个非墙格子 × 每台车（`O(W*H*NR)`）。现在按车的菱形窗口累加计数（带 `stamp` 避免清网格），再行优先扫一遍取最大值；热点半径 ≥ `(W-1)+(H-1)` 时所有格子结果相同，直接取第一个非墙格。
6. **待派单集合**：`PENDQ` 就是一个有序集合，Report 的 `pending=` 直接取 `size()`。

## 5. 保留的"怪行为"（**改需求时千万别顺手修掉，输出是以基线为准的**）

以下都是基线代码的真实行为，与 `SPEC.md` 不一致，已原样保留：

- **派单平局取编号最大**：`ASSIGN` 选 `a` 最小的候选车，但代码是 `if (a <= best)`，**距离相同时取下标最大的车**（SPEC 写的是"编号小"，代码不是）。见 `sim.cpp` 派单最后一段的 `if (a <= best)`。
- **救援是半电**：`RESCUE` 后电量 = `PRM[0]/2`（整除），不是满电；且要求停机满 100 tick（`t - RDS >= 100`）且格子上没有别的车。
- **充电 90% 就走**：`CHARGED` 的判定是 `RB*10 >= PRM[0]*9`，不是满电。
- **取消计数**：`CANCEL` 只要编号存在就 `cancelled++`，哪怕订单已经取货/送达、最终输出的是 `CANCEL_FAIL`。
- **平均延迟是四舍五入**：`avg = (sum + delivered/2) / delivered`，不是截断；`delivered=0` 时为 0。
- **BFS 起点不需要可通行**：`DIST(a,b)` 里即使 `a` 被封锁也以它为源搜索；所以 `dist(车→取货点)` 和 `dist(取货点→车)` 可能一边可达一边 `INF`。代码里对应：取货点表 `tp` + 站在封锁格上的车用 `blockedTabs`（以车自己为源另算）。
- **订单取货/送货点是墙**就 `REJECT`，但**被封锁不拒绝**（拒绝只看地图和墙，不看 `BLOCK`）。
- **订单状态字符串对应关系**：`PEND/ASSIGNED/PICKED/DONE/CANCELLED/LOST` → `O_*` 枚举；`open` 只数前三种。
- **事件 tick 非递减**是输入保证；未知类型事件、空行一律忽略（合法输入里不会出现）。

## 6. 想做需求变更，动哪里

| 需求 | 位置 |
|---|---|
| 派单规则（优先级 / 选车 / 电量门槛） | `sim.cpp` 阶段 4 整块 + `common.h` 的 `PendCmp`（改排序就改这里，`PENDQ` 自动跟着变） |
| 电量（充电速度 / 低电量阈值 / 停机阈值 / 救援电量） | `PRM[0..3]` 的使用点：阶段 3（充电）、阶段 4（`PRM[2]`/`PRM[3]`）、阶段 5（低电量去充电，`noChg` 缓存要一起改）、阶段 6（`RB==0` 停机）、阶段 2（救援`PRM[0]/2`） |
| 报表（间隔 / 状态统计 / 热点） | `sim.cpp` 阶段 7（`hot=` 的累加与扫描）、`Finish()` |
| 新事件类型 | `common.h` 的 `Ev`/`EV_*` + `ReadAll` 的解析 + 阶段 1 的处理 |
| 新订单字段（ agingEvery / carryCost 之类） | `Order` 结构 + `ReadAll` 解析 + 收尾统计；注意 `check_input.py --p2` 暗示过第二阶段可能多两个参数，`ReadAll` 里 PARAMS 现在固定读 6 个 |

**风险点 / 已知边界**：
- 距离表指针跨 `dcacheGet()` 失效（见 §4.1），新增 BFS 调用时注意别持有旧指针。
- `chgCnt` 是增量维护的，新增"占充电桩"的状态时要同步增减，否则阶段 5 的排除逻辑会错。
- 内存预算 288MB 留给距离表；如果地图/机器人数大幅上调，看一下 `ReadAll` 里 `cap` 的计算。
- 输入解析假定输入合法（`check_input.py` 定义的格式：单空格分隔、规范整数、事件按 tick 非递减、地图四周是墙）。对非法输入的行为不保证与基线一致（基线本身也未必定义良好）。

## 7. 怎么验证改动没破坏行为

```bash
python tools/bench.py                 # 5 组公开负载，逐字节比对
python tools/bench.py --baseline      # 顺带看加速比
```

更严格的做法是**和基线差分对拍**（我用的就是这套），换种子/换参数能造出大量同类输入：

```bash
python tools/gen.py pub_dynamic 123 > work/t.in
build/baseline.exe  < work/t.in > work/b.out
build/sim.exe      < work/t.in > work/m.out     # tools/bench.py 编出来的就是 build/sim.exe
# 比较时把 \r\n 当 \n：python 里 .replace(b'\r\n', b'\n') 后逐字节比
```

自造边界用例建议覆盖：极小地图（3x3）、`reportEvery=1`、`hotRadius=0` 和极大、`E=0`、block 频繁（通行性一直变）、把机器人起始点/取货点/送货点所在格 `BLOCK` 掉、`CANCEL` 不存在的编号、机器人初始点放在充电桩上、电量参数极端（`lowThreshold=0`、`safetyMargin=0`、满电很小导致频繁停机）。
