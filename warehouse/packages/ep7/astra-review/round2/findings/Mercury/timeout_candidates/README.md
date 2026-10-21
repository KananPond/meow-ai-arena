# Mercury 压力候选索引

已确认的新根因代表：

- [N01_order_lookup 订单表同桶长链使两次待派遍历退化](N01_order_lookup/README.md)

补充实测（筛查、不合格档或已知根因，不自动计新）：

- [mercury_hash_idle_19900_t50](other_measured/mercury_hash_idle_19900_t50/README.md)
- [mercury_capacity_1998_sources_t20](other_measured/mercury_capacity_1998_sources_t20/README.md)
- [mercury_capacity_2000_sources_t20](other_measured/mercury_capacity_2000_sources_t20/README.md)
- [mercury_capacity_2002_sources_t20](other_measured/mercury_capacity_2002_sources_t20/README.md)
- [sonnet_two_sources_screen](other_measured/sonnet_two_sources_screen/README.md)
- [opus_detour_screen](other_measured/opus_detour_screen/README.md)

未运行档位：1 条，仅作生成候选；[清单与说明](unrun/README.md)。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

所有计数和执行入口见 [逐份 summary](../summary.json)；普通检查未加 `--full`。诊断前缀不计新增覆盖，截停缺尾不算新输出错误。
