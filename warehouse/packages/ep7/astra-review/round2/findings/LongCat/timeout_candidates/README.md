# LongCat 压力候选索引

指定命令原样编译失败，属于已知 __m128i 声明问题；没有运行目标。静态设计不等于实测性能结论，也没有修复后代替原提交运行。

[编译诊断](../../../build/compile.json)；[源码审计](../../../work/agent_dmwl/longcat_audit.md)。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

所有计数和执行入口见 [逐份 summary](../summary.json)；普通检查未加 `--full`。诊断前缀不计新增覆盖，截停缺尾不算新输出错误。
