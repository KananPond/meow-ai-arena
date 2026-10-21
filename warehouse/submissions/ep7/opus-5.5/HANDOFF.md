# 交接文档（主程 → 攻手/接手人）

## 结构
- `main.cpp`：读全部 stdin → `Sim::read/run/finish` → 一次性写出。
- `sim.h / sim.cpp`：仿真。`Robot`/`Order` 结构体 + 枚举状态；每个 tick 按
  事件 → 救援 → 充电 → 派单 → 低电量去充电 → 移动 → 报表，每阶段一个函数（`rescue`、`charge`、`dispatch`/`chooseRobot`、`goCharge`、`move`/`arrive`/`die`、`report`）。
- `grid.h / grid.cpp`：地图 + 封锁状态 + 封锁变化日志 `Grid::log`，以及 `FieldCache`（BFS 距离场缓存）。
- 旧的 util.cpp / old_stuff.cpp / common.h（字符串数组式全局变量）已删除。

## 必须保留的基线怪行为（与 SPEC.md 不一致，代码里用【基线】标注）
1. 派单平局用 `a <= best`：距离相同选**编号大**的机器人。
2. 救援后电量是 `batteryMax / 2`，不是充满。
3. 充电到 `battery*10 >= batteryMax*9`（90%）就 CHARGED；CHARGING 期间 `tx/ty` 仍是充电桩坐标，CHARGED 时清为 -1。
4. `cancelled` 计数：只要订单存在（未被 REJECT）就 +1，**即使输出 CANCEL_FAIL**。
5. `LATENCY avg = (sum + delivered/2) / delivered`（四舍五入）。
6. 已在目标格时先处理到达，再判电量 0；到达后本 tick 不再移动。
7. 热点统计包含被封锁格（只排除墙）；REPORT 的 pending 是待派订单数。
8. REQUEUE 保留原到达 tick；待派队列用 `std::set<PendKey>`（优先级大→到达早→编号小）。

## 性能做法
- 事件预解析，按 tick 指针消费（输入保证 tick 非递减）。
- 占用用 `occ[cell]` 计数（非 DEAD）。
- 距离：`FieldCache::get(src)` 缓存单源 BFS 场（uint16，LRU，约 300MB 上限）。封锁真正改变时记日志；
  取用场时逐条检查日志：封锁 X 只在 X 原本可达时、解封 X 只在 X 有可达邻居（或 X 是充电桩，多源场）时才使场失效。
- `dist(r, p)`（r 可能在被封格上）用 p 的场：`distToSource()`（r 可通行取 F[r]，否则取可通行邻居 +1）。
- 派单 b、c 按订单缓存，用"曼哈顿和"保守判定封锁变化是否可能影响（`computeBC` / `mayChange`）；c 用充电桩多源场。
  派单前先用曼哈顿下界粗筛候选；候选清空即停止遍历。
- 移动：完整场仍新鲜就用；地图 8 tick 内没变则重建完整场；否则用 `nextStep()` 局部 BFS（到达邻居所在层即停）。
- 报表热点：旋转坐标 (x+y, x-y) 二维前缀和，O(1) 查每格。

## 风险 / 待改进
- 封路非常频繁的大图（每 tick 都有封路）上，派单的 `fields.get(p)` 和 `goCharge` 的 `fields.get(robot)` 仍会频繁整图重建，是剩余瓶颈；
  可以仿照 `nextStep` 做"到够用为止"的局部 BFS。
- 依赖"地图四周是墙"（check_input 保证），BFS 不做越界检查。
- 订单编号按 int 存（≤1e9），输出用整数格式，与原串一致（无前导零）。

## 改需求时动哪里
- 派单规则：`Sim::chooseRobot`（候选、合格条件、平局）与 `Sim::dispatch`（排序在 `PendKey`）。
- 电量/充电：`charge`、`goCharge`、`move` 里扣电、`die`。若新增与电量相关的派单条件，注意曼哈顿粗筛必须仍是合法下界。
- 报表：`Sim::report`、`Sim::finish`。
- 对拍脚本在 `work/fuzz.py`（工作区，不在 src）：随机小图 + gen.py 配置，与基线逐字节比对。
