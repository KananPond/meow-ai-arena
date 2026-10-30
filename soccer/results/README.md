[English](README_en.md)

# 两张榜和逐局结果

正式公开成绩以 [团队榜](团队榜.md)、[个人榜](个人榜.md) 为准，均为 10-29 重赛后最终版；不公开存档合并总分。

| 目录 | 场次 | 局数 | 每对种子数 |
|---|---|---:|---:|
| `halftime/` | 上半场程序，中场赛 | 2112 | 32 |
| `final/` | 下半场程序，终场团队赛 | 2904 | 44 |
| `individual/` | 本人 player + partner，个人赛 | 3264 | 16 |

每个目录都有 `results.json.gz`（完整统计与全部逐局数据）和 `programs.json`（实际使用版本、源码与可执行文件哈希、回退说明）。中场和终场另保留比赛时生成的 `成绩.md`；它们是原始简表，面向读者的最终完整表是上面的两张榜。原 `.jsonl` 内容已在 JSON 的 `games` 中逐局保留，不再重复提交。

`results.json.gz` 顶层包括 `standings`、`games`、`programs`、`references`、`bt`、`seeds`；个人赛另有 `teams`。一局含 `id`、`A/B`、`seed`、种子序号 `k`、`first_kick`、`order`、`seats`、`score`、`goals`、`faults`、`cpu`、`kicks`、`digest`、`time`。参考队以 `ref-` 开头，不占名次。

路径以 `soccer/` 为根，历史编译目录改为 `<build>` 占位；该占位不表示仓库文件。`replay` 指历史回放位置，只有 [抽样清单](../replays/selection.json) 中的文件随仓库提供。统计、种子、座位、源码哈希、历史可执行文件哈希不变。`.gz` 可用 Python 标准库 `gzip.open` 读取。

复算胜平负、积分、进失球：在 `soccer/` 运行 `python tournament/score.py final` 或 `individual`；加 `--bt` 重算参考实力值。检查归档一致性：`python check_archive.py`。脚本不修改历史成绩。
