[English](README_en.md)

# 各模型的封存代码

`half1/` 是 12 份上半场主程序；`half2/` 是 12 份下半场主程序与 10 份个人搭档程序。都是封存原样，换行符也不改。team2 的下半场来自 10-29 重赛。SHA-256 见 [来源清单](../source_manifest.json)，上场来源见 [成绩索引](../results/README.md)。

| 文件前缀 | 模型 | 搭档位 |
|---|---|---|
| `team1_a` | Grok 4.7 | 独立 partner 文件 |
| `team1_b` | 腾讯混元 Hy4 | 独立 partner 文件 |
| `team2_a` | Kimi K3.1 Pro | partner 与 player 相同 |
| `team2_b` | 阶跃 Step 5 | 独立 partner 文件 |
| `team3_a` | Gemini 4 Argon | 独立 partner 文件 |
| `team3_b` | 豆包 Doubao-Seed-Evolving | 没交，使用自己的 player |
| `team4_a` | GPT-6 Astra | 独立 partner 文件 |
| `team4_b` | GPT-6.1 Terra | 独立 partner 文件 |
| `team5_a` | Claude Opus 5.5 | 没交，使用自己的 player |
| `team5_b` | GLM 5.3 | partner 与 player 相同 |
| `team6_a` | DeepSeek V4.1 Flash 1017 | 独立 partner 文件 |
| `team6_b` | Claude Haiku 5.5 | 独立 partner 文件 |

主程序名为 `teamN_x.cpp`，搭档为 `teamN_x.partner.cpp`。个人赛始终用本人两份程序，不是直接搭队友。开发席位 A/B 与每局 0/1 号不同，座位由赛程轮换。完整映射见 [roster.json](roster.json)。

DeepSeek 搭档的局部大段照搬判定见 [CONDITIONS.md](../CONDITIONS.md)，源码和成绩都保留原样。
