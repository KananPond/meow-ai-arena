# 第 7 集 · 屎山优化：各模型交的代码

第 7 集《区王争霸赛》第二场"屎山优化"的提交。任务就是第 5 集第 1 轮的主程任务（选手包是同一个 `../../workspace/`），每个模型单独做一次，没有队友。每个文件夹是收卷时的 `src/`，逐字节原样复制，主办方没有改动。编译命令：`g++ -O2 -std=c++20 *.cpp`（Windows 上加 `-static`），和评测时一样只编译顶层的 `.cpp`。

比赛时主办方内部给这些文件夹起的名字带场次前缀（例如 `…_Step`），`../../results/ep7/` 里的成绩文件用 `ep7_<短名>` 指代它们，对应关系见下表"成绩文件里的名字"一列。

| 文件夹 | 模型 | 成绩文件里的名字 | 名次 | 8 组正确 | 几何平均加速比 | 有没有交接文档 |
|---|---|---|---:|---|---:|---|
| `step-5-preview/` | 阶跃 Step 5 preview | `ep7_Step` | 1（并列） | 8/8 | 139.4× | 有 |
| `hunyuan-hy4-preview/` | 腾讯混元 Hy4 preview | `ep7_Hunyuan` | 1（并列） | 8/8 | 138.2× | 有 |
| `qwen3.8-27b/` | 通义千问 Qwen3.8-27B | `ep7_Qwen27B` | 3（并列） | 8/8 | 8.7× | 没有 |
| `mercury-3/` | Inception Mercury 3（扩散模型） | `ep7_Mercury` | 3（并列） | 8/8 | 8.6× | 没有（到点后才写的那份已删，见 `../../CONDITIONS.md`） |
| （`../round1/doubao-seed-2.1-turbo/`） | 豆包 Seed 2.1 Turbo | 第 5 集原主程 | 5 | 8/8 | 8.4× | 有（第 5 集的代码，沿用旧成绩） |
| `ernie-5.1/` | 文心 ERNIE 5.1 | `ep7_Wenxin` | 6 | 8/8 | 1.3× | 有 |
| `mistral-large-4/` | Mistral Large 4 | `ep7_Mistral` | 7（并列） | 8/8 | 约 1×（代码与基线相同） | 没有（到点后才写的那份已删） |
| `dots3-note-preview/` | 小红书 dots3-note preview | `ep7_Dots` | 7（并列） | 8/8 | 约 1×（代码与基线相同） | 没有 |
| `openpangu-2.0-pro/` | 盘古 openPangu-2.0-Pro | `ep7_Pangu` | 9 | 1/8（其余 7 组超时） | 比基线慢 2 倍以上 | 没有 |
| `longcat-2.0/` | 美团 LongCat-2.0 | `ep7_LongCat` | 10 | 0/8（编译失败） | — | 没有 |
| `sonnet-5.5/` | Claude Sonnet 5.5 | `ep7_Sonnet` | 参照，不排名 | 8/8 | 169.7× | 有 |
| `opus-5.5/` | Claude Opus 5.5 | `ep7_Opus` | 参照，不排名 | 8/8 | 101.0× | 有 |
| `haiku-5.5/` | Claude Haiku 5.5 | `ep7_Haiku` | 参照，不排名 | 8/8 | 98.1× | 有 |

- `mistral-large-4/` 和 `dots3-note-preview/` 的 5 个文件和基线 `../../workspace/baseline/` 逐字节相同（SHA-256 一致），评测时按"和基线相同"处理，没有单独运行。
- `longcat-2.0/` 用上面的命令编译失败：`sim.cpp` 第 70 行用了 `__m128i` 却没有包含对应的头文件。编译器的原始报错在 `../../results/ep7/编译错误.json`。
- 豆包这一场的名次沿用它第 5 集作为主程的成绩，代码见 `../round1/doubao-seed-2.1-turbo/`；它这一场交的新代码按用户决定不测速、不放进仓库。
- Claude 三位用同一个包、同一句提示词、同样的时限做了一次，作为参照列出，不参与名次。
- 名次、反例和用时的完整说明见 `../../README.md` 的"第 7 集 · 屎山优化"一节。
