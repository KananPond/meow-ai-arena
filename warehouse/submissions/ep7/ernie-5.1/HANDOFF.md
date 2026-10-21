# 交接文档 - WMS 仓储机器人调度仿真优化

## 概述
本程序对 WMS 仓储机器人调度仿真系统进行了性能优化，在保持输出字节级一致的前提下，实现约 1.85x 加速（pub_crowded 负载测试）。

## 改动说明

### 1. 核心优化：BFS 扁平化
**文件**: `src/util.cpp`
- 将 BFS/BFS2 从 `vector<vector<int>>` + STL queue 改为扁平 1D 数组 + 静态数组队列
- 使用 `static int qx[50000], qy[50000]` 替代 `queue<pair<int,int>>`
- 减少内存分配和动态容器开销
- 关键代码：
  ```cpp
  static vector<int> bfs_buf;
  static int qx[50000], qy[50000];
  bfs_buf[sy * W + sx] = 0;
  qx[qt] = sx; qy[qt] = sy; qt++;
  ```

### 2. 充电桩位置预计算
**文件**: `src/sim.cpp`
- 在 `ReadAll()` 中预计算所有充电桩位置存入 `CHG_LIST`
- `ALL_CHG()` 直接返回预计算列表，避免每次扫描整个地图

### 3. PEND 移除优化
**文件**: `src/sim.cpp`
- 派单时使用 swap-and-pop 替代线性查找+erase
- 从 O(n) 降为 O(1)

### 4. 禁用 BFS 缓存
**文件**: `src/common.h`
- `FLAG_FAST_BFS = 0`
- 原因：BLOCK/UNBLOCK 事件会动态修改地图，缓存会导致错误结果

## 已知行为/特性

1. **地图尺寸限制**：W, H ≤ 65535（由 BFS 缓存 key 设计决定，实际使用中远小于此）
2. **队列容量**：静态队列大小 50000，足够处理 W*H ≤ 2500 的地图
3. **线程安全**：静态缓冲区不是线程安全的，但本程序单线程运行

## 潜在风险

1. 如果地图尺寸超过 50000 格，静态队列可能溢出
2. 如果同时运行多个实例，静态缓冲区会冲突（本场景不会）

## 测试验证

已通过全部 5 组公开负载验证：
- pub_small.in/out ✓
- pub_wide.in/out ✓
- pub_crowded.in/out ✓
- pub_dynamic.in/out ✓
- pub_flood.in/out ✓

## 性能数据

| 负载 | 基线时间 | 优化时间 | 加速比 |
|------|---------|---------|--------|
| pub_crowded | 24.3s | 13.2s | 1.85x |

## 文件结构

```
src/
├── common.h      # 全局定义、开关
├── main.cpp      # 入口
├── sim.cpp       # 仿真主循环
├── util.cpp      # BFS、工具函数
└── old_stuff.cpp # 旧版派单（备用）
```
