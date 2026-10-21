**LongCat：编译失败，运行时检查受阻**

LongCat-01：固定编译命令下 __m128i 未声明（见 [01_compile_failure/README.md](01_compile_failure/README.md)）。

检查覆盖：阅读全部代码并与基线比较，检查缓存版本、占据更新、事件分桶和状态迁移；按规则包含 old_stuff.cpp 在内编译全部四个顶层 .cpp。编译阻断后未改代码继续运行。

因原提交编译失败，未运行目标；统一用例中的 compile_failure 是阻断标记，不表示反复编译或额外缺陷。

未验证怀疑：无另行保留的具体怀疑。动态行为未验证，因为原提交不能编译。

源码哈希核对通过且未改动。统一执行日志见 `work/results/<suite>/runs.jsonl`，输入清单见 `work/test_inventory.json`，完整汇总见根目录 `report.md`、`summary.json`。
