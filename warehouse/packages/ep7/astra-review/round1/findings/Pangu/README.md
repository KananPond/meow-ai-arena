**Pangu：已验证 2 处正确性缺陷**

Pangu-01：停机时未清除占据缓存（见 [01_dead_occupancy/README.md](01_dead_occupancy/README.md)）；Pangu-02：救援时未登记新增占位（见 [02_rescue_occupancy/README.md](02_rescue_occupancy/README.md)）。

检查覆盖：对照全部增量改动，重点检查单源 BFS 缓存失效、占据布尔表的移动/停机/救援更新、预计算桩位和热点、日志计数。针对停机释放、同格救援分别构造反例。

独立运行统一小用例 443 个：一致 439 个，差异 4 个。差异变体按根因去重。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。

源码哈希核对通过且未改动。统一执行日志见 `work/results/<suite>/runs.jsonl`，输入清单见 `work/test_inventory.json`，完整汇总见根目录 `report.md`、`summary.json`。
