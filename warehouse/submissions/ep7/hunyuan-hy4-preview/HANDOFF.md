# HANDOFF —— 给接手的队友

仿真系统重写完毕：**输出与 baseline/ 逐字节一致**（公开 5 组负载 + 换种子对拍 + 手写边界用例），
公开负载总 CPU 时间 89.5s → 0.98s（**约 90×**，各负载 30×～200×）。

下面按「你要知道什么才能安全地改这个系统」来组织。

---

## 1. 文件结构（改动后的）

| 文件 | 内容 |
|---|---|
| `common.h` | 全部接口声明 + `INF`（原 `BIGNUM`）+ `Field` 结构。注释即文档 |
| `util.cpp` | ① 字符串/数字工具（快版 `S2I` / `APPEND_INT`）② 地图网格（邻居表、通行表、占据计数、封锁）③ BFS 距离场与缓存 ④ 充电桩多源距离场 |
| `sim.cpp` | 仿真主体：`ReadAll` / `RunSim` / `Finish`，按 tick 的 7 个阶段顺序与基线完全一致 |
| `main.cpp` | 只有全局变量定义和 4 行驱动 |
| ~~`old_stuff.cpp`~~ | **已删除**：里面的 `oldDispatch`（开关 `USE_NEW_DISPATCH=0` 才用）和 `bfs_fast_cached`（`FLAG_FAST_BFS` 未开）都是死代码，且那位同事的注释里写着"开了之后有负载对不上" |

编译命令没变：`g++ -O2 -std=c++20 src/*.cpp`。

**每个 tick 的阶段顺序**（`RunSim` 里就是这 7 段，编号和注释一一对应，别调顺序）：
1 外部事件 → 2 救援 → 3 充电 → 4 派单 → 5 低电量去充电 → 6 移动 → 7 报表。

---

## 2. 怪行为清单（**都是基线实际行为，别当成 bug 修掉**）

SPEC.md 是 2022 年的旧文档，下面这些地方**代码和文档不一致，以代码为准**（我全部原样保留）：

1. **救援只补半格电**：`RESCUE` 后电量 = `batteryMax / 2`，不是 SPEC 说的"充满"。
2. **充到 90% 就走**：`电量 * 10 >= batteryMax * 9` 就 `CHARGED` 并转 `IDLE`，不是充满。
3. **平均延迟是四舍五入**：`(延迟和 + 送达数/2) / 送达数`，SPEC 写的是整除。
4. **取消失败也计入 cancelled**：`CANCEL` 时只要订单**存在**，就先 `CNT[3]++`，状态不对才打 `CANCEL_FAIL`。所以 `cancelled` 统计里混着失败的单。
5. **派单平局取编号大的**：`if (a <= best)` 用的是 `<=`，同样距离时**编号大的机器人**中标（SPEC 写的是编号小的）。
6. **`writeLog` 是按输出的文字反查统计的**：`DELIVER`/`LOST`/`REJECT` 三类行的计数在打印时累加（`sim.cpp` 里分别在 `arrive()`、`killRobot()`、事件处理里手动累加，等价）。**如果你新增了一类以这些词开头的日志行，统计会串**；改输出格式时留意。
7. **距离是不对称的**：`dist(a,b)` 从 a 出发 BFS，**a 无论能不能走都算起点**，之后只走可通行格。所以 a 不可通行时 `dist(a,b) != dist(b,a)`。`util.cpp` 的 `fieldFrom()` 就是照这个语义实现的。
8. **`PEND` 里可能出现重复 id**（只有当输入里订单号重复时，而 `check_input.py` 认为这是非法输入）。我保留了基线的行为：会连打两遍 `ASSIGN`，`pendRemove` 里的 `g_hasDupPend` 分支就是为这个服务的，别删。

---

## 3. 数据结构与关键实现（`util.cpp`）

* **坐标 → cell**：`cell = y*W + x`，`cellOf(x,y)` 出图返回 -1。机器人位置/目标都存在 `RC[] / RTC[]`（cell 整数）里，`RC[i]%W`、`RC[i]/W` 取回 x,y。
* **`NB4(cell)`**：预处理好的四邻居（顺序 **上右下左，不能改**，平局全靠它）。
* **`PASSABLE(cell)`** = 在图内 且 不是墙 且 没被封锁（就是原来的 `OK()`）。`setBlocked()` 改动封锁时会 bump `epoch`。
* **占据 `OCC(cell, selfCell)`**：`g_occ[]` 计数网格（只有非 `DEAD` 机器人占格子），移动/停机/被救时用 `occAdd()` 增量维护。`selfCell` 传自己当前 cell（自己不算占据），机器人已 `DEAD` 传 -1。
* **BFS 距离场 `Field`**：`dist[] + stamp[]`，`stamp[c]==gen` 时 `dist[c]` 有效，否则视为 `INF`。这样每次 BFS **不用清零整个数组**。
  * `fieldFrom(cell)` 返回"从 cell 出发的 BFS 距离场"，结果**按 (起点, epoch) 缓存**（槽位池 + 轮转淘汰，内存上限 ~64MB）。
  * 封锁集合一变 → `invalidateCache()` → 所有缓存作废。**所以：任何改动封锁的地方必须走 `setBlocked()`**，直接改数组会让缓存拿到过期数据。
* **`chargerDistFrom(x,y)`**（原 `CHG_DIST`）：改成了**一次多源 BFS**（从所有可通行充电桩同时出发）预计算，查询 O(1)。可通行格直接查；不可通行格 = `1 + min(可通行邻居)`（推导：起点不可通行时 BFS 的第一步必须走到可通行格）。

---

## 4. 这次做了哪些优化（以及为什么安全）

| 原来 | 现在 | 为什么等价 |
|---|---|---|
| 每 tick 扫**全部**事件并重新切词 (`O(T*E)`) | 事件预先切词一次，按 tick **稳定分桶**（计数排序，同 tick 保持输入顺序） | 事件按 tick 触发，顺序不变 |
| 派单里对每个 (订单, 机器人) 做 3 次 BFS | 机器人侧距离场缓存复用；`b=dist(取货,送货)` 按 (订单, epoch) 缓存；`c` 用多源 BFS 变 O(1) | b/c 与机器人无关，纯函数，可提升 |
| `CHG_DIST` 每次全图 BFS + 每调用全图扫充电桩 | 多源 BFS + 预计算充电桩清单 | 见上 |
| `OCC` 每次扫全部机器人（还用 string 比状态） | 计数网格，O(1) | 同上语义 |
| `BFS` 每次 `new vector<vector<int>>` | 定长缓存池 + stamp | 同 |
| 状态用 `string`（`"IDLE"`…）、订单用 `map<string,vector<string>>` | 整数枚举 + `struct Order` + `unordered_map<string,int>` | 只是表示方式变了 |
| 输出 `cout << ... << endl`（每行 flush） + `stringstream` 做 `I2S` | 拼进一个大 `OUT`，最后一次 `fwrite`；`APPEND_INT` 手搓 | 输出内容不变 |
| 报表热点 `O(W*H*R)` | 旋转坐标 `(x+y, x−y)` 把菱形变矩形 + 二维差分，`O((W+H)² + W*H)` | 平局仍取行优先靠前的格子 |

---

## 5. 你要改需求，动哪里

* **派单规则**（优先级、候选条件、选谁）：`sim.cpp` 第 4 段。候选筛选用 `cand` 数组收集 `(机器人, a)`，中标逻辑就一行 `if (a <= best)`。
* **电量 / 充电 / 停机 / 救援**：`sim.cpp` 第 2、3 段和 `killRobot()`。`RB[]`、`PRM[0..3]`。
* **报表（含热点）**：`sim.cpp` 第 7 段；`hot=` 那段是旋转坐标差分，想改统计口径（比如按区域、按载重）就替换 `imos` 那块，外层 `REPORT` 拼串不用动。
* **订单状态机**：`enum { O_PENDING, O_ASSIGNED, O_PICKED, O_DONE, O_CANCELLED, O_LOST }`，状态迁移集中在 ① 事件处理 ② `arrive()` ③ `killRobot()` 三处。`Finish()` 里 `open` 的口径也在那。
* **新增事件类型**：在 `enum { EV_ORDER, EV_CANCEL, EV_BLOCK, EV_UNBLOCK, EV_OTHER }` 加一项，在 `ReadAll` 的切词处加分支，在 `RunSim` 第 1 段加处理。

---

## 6. 已知风险 / 我留给你的坑

1. **BFS 缓存的正确性依赖 `epoch`**：新增任何改变"可通行性"的逻辑（比如新的封路方式、临时墙），**必须**走 `setBlocked()` 或手动 `invalidateCache()`，否则会读到过期距离场（症状：机器人往被封的格子里撞）。
2. **缓存池有上限**（最多 256 张场，按 64MB 内存算）。如果一个 tick 内 BFS 起点数远超槽位数（例如 300 个机器人各自分散 + 上百个待派订单），会退化成"每次都重算"，**不会算错，只是变慢**。
3. **`REPORT` 的热点统计** 在 `reportEvery=1` 且地图 200×200、T=20000 的极端组合下，本身是 `O(T*(W+H)²)`，可能还是有点重（基线那种 `O(W*H*R)` 更重）。如果新需求要频繁出报表，考虑改成增量维护。
4. **内存**：距离场缓存池最多 ~64MB，加上差分数组，远低于 1GB；W,H=200 时实测 ~15MB。
5. 整数运算：`RB * 10 >= batteryMax * 9` 我改成了 `long long` 比较（避免极端参数下溢出 UB）；`a+b+c+margin` 仍是 int，但距离 ≤ `W*H`，不会溢出。

---

## 7. 怎么验证

```bash
python3 tools/bench.py --baseline     # 公开 5 组：输出比对 + 加速比
python3 tools/gen.py pub_small 7 > /tmp/a.in    # 换种子造负载
                                      # 再用 baseline 编译出的程序对拍
work/difftest.py                      # 我写的对拍脚本（换种子 + 边界用例）
```
`work/difftest.py` 会拿 `build/baseline.exe` 和 `build/sim_new.exe` 对拍；新增需求后请继续用它。
