# Pangu 压力候选索引

已确认的新根因代表：

- [N01_eager_dispatch 没有可派机器人仍逐单计算取送及充电距离](N01_eager_dispatch/README.md)
- [N02_order_lookup 字符串订单表长链使每 tick 查找和排序退化](N02_order_lookup/README.md)

补充实测（筛查、不合格档或已知根因，不自动计新）：

没有另列 other_measured 档位；已确认代表的对照保留在对应 controls/。

未运行档位：5 条，仅作生成候选；[清单与说明](unrun/README.md)。

探索阶段审计文件中的 pending、预测耗时和旧复杂度描述是过程记录，不代表最终结论；最终以各代表 README、work/finding_catalog.py 及 work/agent_dmwl/hash_review.md 为准。

所有计数和执行入口见 [逐份 summary](../summary.json)；普通检查未加 `--full`。诊断前缀不计新增覆盖，截停缺尾不算新输出错误。
