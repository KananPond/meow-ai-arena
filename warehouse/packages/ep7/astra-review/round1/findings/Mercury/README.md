**Mercury：已验证 1 处正确性缺陷**

Mercury-01：删除待派集合节点后继续使用其订单号引用（见 [01_erased_order_id/README.md](01_erased_order_id/README.md)）。

检查覆盖：检查 uint16 INF 转换、缓存失效、占据和充电占用增量更新、热点差分、待派 set 的节点生命周期；围绕订单号长度 1/7/8/9/10 位构造并复现释放后使用。

独立运行统一小用例 443 个：一致 432 个，差异 11 个。差异变体按根因去重。

未验证怀疑：无另行保留的具体怀疑。有限检查未构成无缺陷证明；未检查 full 专属规模和性能。

源码哈希核对通过且未改动。统一执行日志见 `work/results/<suite>/runs.jsonl`，输入清单见 `work/test_inventory.json`，完整汇总见根目录 `report.md`、`summary.json`。
