# Mistral 压力候选索引

全部源文件与基线原字节相同，未发现可隔离的新增算法成本。因此没有单独列为新增强候选；共同压力输入仍独立执行，未凭源码分析豁免测试。

共同压力输入 `pangu_busy_strongest`：唯一机器人被送货点封锁而持续忙碌，600张新单待派，T=3000。

基线 CPU/墙钟：5.281/5.327 完成 (exit=0) 秒。Mistral CPU/墙钟：4.875/5.013 完成 (exit=0) 秒。目标正常完成，输出仅统一 CRLF 后与基线相同；未达到正式超时条件，不计新增性能缺陷。

[共同输入](../../../work/runs/pangu_busy_strongest/case.in)；[完整实测证据](../../../work/runs/pangu_busy_strongest/evidence.json)；[源码审计](../../../work/agent_dmwl/audit.md)。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

所有计数和执行入口见 [逐份 summary](../summary.json)；普通检查未加 `--full`。诊断前缀不计新增覆盖，截停缺尾不算新输出错误。
