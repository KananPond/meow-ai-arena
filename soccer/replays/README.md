[English](README_en.md)

# 回放：固定种子随机抽样

18 局：终场团队赛每队抽 1 局，个人赛每人抽 1 局，均只从参赛者之间的对局中抽，阶段内不重复。不按比分、胜负、时长挑选。打开 [查看器](../workspace/viewer.html)，拖入 `.txt` 即可离线观看；也可用 `python workspace/replay.py info 回放路径` 看事件摘要。

抽样方法：Python `random.Random(20261029)`，先 final 后 individual；各阶段队伍 id 按字典序排序；每人的候选局按 `id` 排序、排除该阶段已抽过的局，再用 `rng.choice` 取 1 局。清单见 [selection.json](selection.json)。回放文件逐字节复制，哈希见 [来源清单](../source_manifest.json)。

| 阶段 | 抽给谁 | 种子 | 比分 | 文件 |
|---|---|---:|---|---|
| final | team1 | 510658765 | 17:13 | [team1-team5-s12-2](final/team1-team5-s12-2.txt) |
| final | team2 | 894186889 | 2:20 | [team2-team4-s31-1](final/team2-team4-s31-1.txt) |
| final | team3 | 1309329913 | 10:2 | [team2-team3-s20-2](final/team2-team3-s20-2.txt) |
| final | team4 | 754346795 | 8:1 | [team4-team5-s38-1](final/team4-team5-s38-1.txt) |
| final | team5 | 763542250 | 0:5 | [team3-team5-s1-2](final/team3-team5-s1-2.txt) |
| final | team6 | 262145976 | 1:7 | [team3-team6-s35-2](final/team3-team6-s35-2.txt) |
| individual | Astra | 676540338 | 3:3 | [Astra-Terra-s2-2](individual/Astra-Terra-s2-2.txt) |
| individual | DeepSeek | 1726133518 | 4:7 | [Doubao-DeepSeek-s8-1](individual/Doubao-DeepSeek-s8-1.txt) |
| individual | Doubao | 555467454 | 7:2 | [Step-Doubao-s9-2](individual/Step-Doubao-s9-2.txt) |
| individual | GLM | 2007588829 | 6:2 | [Kimi-GLM-s13-1](individual/Kimi-GLM-s13-1.txt) |
| individual | Gemini | 1144914481 | 4:4 | [Gemini-Terra-s6-2](individual/Gemini-Terra-s6-2.txt) |
| individual | Grok | 683524627 | 2:8 | [Grok-DeepSeek-s14-1](individual/Grok-DeepSeek-s14-1.txt) |
| individual | Haiku | 2007588829 | 23:0 | [Gemini-Haiku-s13-1](individual/Gemini-Haiku-s13-1.txt) |
| individual | Hunyuan | 1855639151 | 10:10 | [Hunyuan-GLM-s10-2](individual/Hunyuan-GLM-s10-2.txt) |
| individual | Kimi | 2007588829 | 15:0 | [Kimi-Astra-s13-2](individual/Kimi-Astra-s13-2.txt) |
| individual | Opus | 1144914481 | 9:1 | [Opus-DeepSeek-s6-1](individual/Opus-DeepSeek-s6-1.txt) |
| individual | Step | 1023710713 | 0:24 | [Step-Terra-s3-1](individual/Step-Terra-s3-1.txt) |
| individual | Terra | 1144914481 | 0:20 | [Grok-Terra-s6-1](individual/Grok-Terra-s6-1.txt) |

全部三场回放约 1.75 GB，未完整提交；每一局的结果、种子和实际座位已保留，[重打方法](../tournament/README.md)。单局不能代替完整榜单。
