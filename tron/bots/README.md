[English](README_en.md)

# 各模型的封存代码

都是模型在比赛中写出的原样代码，主办方没有改动。文件的 sha256 和 `../results/清单.json` 里冻结的一致。greedy、random 在 `../workspace/bots/`，两个陪练的源码在 `../sparring/`。

## 正式选手

名次、总分是正式榜的（锚点豆包 = 0）。

| 文件名 | 模型 | 短名 | 正式榜名次 | 总分 | 行数 |
|---|---|---|---|---:|---:|
| `mistral-large-4.cpp` | Mistral Large 4 | Mistral | 第 1 名 | 102 | 431 |
| `longcat-2.0.cpp` | 美团 LongCat-2.0 | LongCat | 第 2～3 名 | 10 | 276 |
| `doubao-seed-evolving.cpp` | 豆包 Doubao-Seed-Evolving | Doubao | 第 2～4 名 | 0（锚点） | 776 |
| `step-5-preview.cpp` | 阶跃 Step 5 preview | Step | 第 3～4 名 | −25 | 565 |
| `openpangu-2.0-pro.cpp` | 盘古 openPangu-2.0-Pro | Pangu | 第 5 名 | −208 | 218 |
| `hunyuan-hy4-preview.cpp` | 腾讯混元 Hy4 preview | Hunyuan | 第 6 名 | −240 | 504 |
| `qwen3.8-27b.cpp` | 通义千问 Qwen3.8-27B | Qwen27B | 第 7～9 名 | −290 | 746 |
| `mercury-3.cpp` | Inception Mercury 3 | Mercury | 第 7～9 名 | −297 | 371 |
| `ernie-5.1.cpp` | 文心 ERNIE 5.1 | Wenxin | 第 7～9 名 | −304 | 131 |
| `dots3-note-preview.cpp` | 小红书 dots3-note preview | Dots | 第 10 名 | −440 | 115 |

## 参考选手（`reference/`，只在强对手榜）

| 文件名 | 模型 | 短名 | 强对手榜名次 | 总分 | 行数 |
|---|---|---|---|---:|---:|
| `reference/gpt-6-astra-ultra-fast.cpp` | GPT-6 Astra（ultra 思考档 + fast 服务档，Codex） | AstraUF | 第 1 名 | 313 | 253 |
| `reference/deepseek-v4.1-flash-dsh.cpp` | DeepSeek V4.1 Flash（dsh 0.2.1-rc.2 标准模式） | DSNew | 第 2～3 名 | 128 | 514 |

短名是成绩表、回放和配置里用的名字。

豆包封存后会话又改了文件，按规则以封存的版本为准，这里放的就是封存版本。

用 `../workspace/arena.py` 可以直接拿它们当对手，例如在 `workspace/` 里运行 `python arena.py play submission/bot.cpp ../bots/mistral-large-4.cpp --seed 5`，或 `python arena.py run --opp ../bots/step-5-preview.cpp,../bots/hunyuan-hy4-preview.cpp`。
