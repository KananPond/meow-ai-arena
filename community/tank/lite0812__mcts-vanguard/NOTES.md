# MCTS Vanguard

作者：Lite0812  
开发方式：AI 编写；使用 GPT-6.1 Sol，开发工具为 Codex。

本程序结合分车 PUCT 搜索与增量策略／价值网络，在每回合有限的 CPU 时间内为三辆坦克选择行动。非终局的动作先验和局面评分全部来自网络，终局直接使用官方引擎给出的胜负。

## 搜索与协同

使用官方 `tank::step` 同时推进双方动作。搜索深度上限为 12 回合；非终局的新分支至少推进 2 回合后估值，后续模拟逐步扩展已有分支。节点按各车局部状态组织，局部转置表复用动作先验、访问次数和价值统计。双方六辆车共同参与推演，结果按阵营回传，最终选择己方各车根节点访问次数最多的合法动作。

根节点先以网络先验产生双方基准动作，逐一替换单车动作，用官方引擎推进一回合。所得局面经同一神经价值函数评估，按 `P′(a) ∝ P(a) × exp(4 × V(s′))` 复核先验，再混入 2% 均匀探索。PUCT 将平均回传价值和动作先验结合，分配后续模拟。

## 增量网络与 CNN 策略分支

主体网络结构为 `3504 → 56 → 64`，由 244 个激活槽编码基地、坦克、子弹、地形和路径等公开状态。嵌入累加器通过减去变化的旧特征、加入新特征更新，再计算后续网络层；这是实际执行的 NNUE 增量推理。

策略头输出 25 类动作分数。根节点额外使用一个局部 CNN 策略残差：输入为以当前车辆为中心的 `27 × 9 × 9` 观测，经过两层 8 通道、3×3 卷积，得到 `8 × 5 × 5` 特征，再输出 25 类动作残差，与主体策略分数相加。CNN 只用于根节点，深层仍使用主体网络策略；合法动作归一化温度为 0.7，再混入 2% 均匀探索。

价值头之外还有一个 `56 → 8 → 1` 的神经残差分支，从第一层激活预测修正量，加入价值 logit 后取 tanh。搜索采用双方视角的对称值 `V(s) = 0.5 × (v₀(s) − v₁(s))`。该分支仅参与估值，不修改策略头。

权重全部内嵌在 `bot.cpp` 中，采用量化整数与浮点混合推理、对齐和填充布局，由标准 C++ 实现；运行时不加载外部模型文件。

## 离线训练

NNUE 自对弈、MCTS 自对弈和两者交叉对弈共享数据，两款网络参数独立。本轮使用 1,536 局新自对弈，高预算策略样本采用 8,192 次离线模拟的根节点访问分布，价值监督来自完整对局的实际终局胜负。另有 1,664 局原生正式对战用于神经价值残差训练，全部胜、平、负样本保留并核对终态。

训练使用 PyTorch，包含左右镜像增强、量化感知训练和自对弈预测保留约束。MCTS 的 CNN 输出头还使用真实完整对局胜方的动作进行离线策略蒸馏。实际比赛只执行固定权重的 C++ 推理和搜索。

## 运行预算

仅依赖官方 `tank.hpp` 和 C++ 标准库，只定义 `init`、`decide`，由官方主循环提供 `main`。程序单线程运行，模型和搜索缓存仅保存在当前进程内存中。

正式运行 `DUAL_FIXED=0`。首回合搜索目标约 1,400 CPU 毫秒，常规回合约 44 CPU 毫秒，并按剩余预算保留约 5 CPU 毫秒返回余量。墙上时钟另设较宽的保险截止。正式预算保持首回合 2,000、之后每回合 50 CPU 毫秒，实际搜索量随机器与负载变化。


## 本版官方自测

官方 `community/check.py` 的结构、编译与 320 局原生正式自测通过：315 胜、0 平、5 负。16 个对手各 20 局，地图种子 1001–1010，每图换边，首回合 2000、之后每回合 50 CPU 毫秒；未发现超时、非法指令或进程故障。

| 官方对手／参照 | 胜 | 平 | 负 | 局数 | 实际胜率 |
|---|---:|---:|---:|---:|---:|
| Astra | 18 | 0 | 2 | 20 | 90.00% |
| SolUF | 20 | 0 | 0 | 20 | 100.00% |
| Gemini | 19 | 0 | 1 | 20 | 95.00% |
| Sol | 20 | 0 | 0 | 20 | 100.00% |
| Opus | 18 | 0 | 2 | 20 | 90.00% |
| Muse | 20 | 0 | 0 | 20 | 100.00% |
| MiMo | 20 | 0 | 0 | 20 | 100.00% |
| Grok | 20 | 0 | 0 | 20 | 100.00% |
| DeepSeek | 20 | 0 | 0 | 20 | 100.00% |
| Doubao | 20 | 0 | 0 | 20 | 100.00% |
| Kimi | 20 | 0 | 0 | 20 | 100.00% |
| Haiku | 20 | 0 | 0 | 20 | 100.00% |
| Qwen | 20 | 0 | 0 | 20 | 100.00% |
| GLM | 20 | 0 | 0 | 20 | 100.00% |
| sparring | 20 | 0 | 0 | 20 | 100.00% |
| baseline | 20 | 0 | 0 | 20 | 100.00% |

## 额外开发对战

下表来自本机另一批地图，各对手 64 局；社区种子 1525000000–1525000031，官方种子 1525001000–1525001015，每张地图换边。使用原生正式 2000／50 CPU 毫秒预算、4 并发，全部无运行故障。Overmatch 固定为 R13（SHA-256 前缀 `628a67a5`）。

| 对手 | 胜 | 平 | 负 | 局数 | 实际胜率 |
|---|---:|---:|---:|---:|---:|
| Overmatch R13 | 57 | 0 | 7 | 64 | 89.06% |
| slzhta__value-based-puct | 49 | 0 | 15 | 64 | 76.56% |
| mbfcasper__neural-search | 58 | 0 | 6 | 64 | 90.62% |
| gpt-6-astra | 30 | 0 | 2 | 32 | 93.75% |
| gpt-6.1-sol-ultrafast | 31 | 0 | 1 | 32 | 96.88% |

本版对同一 R13 源码的累计记录为 194 胜、0 平、30 负，共 224 局、112 张图，实际胜率 86.61%。累计记录包含上表以及其他开发筛选局，不是预注册的独立确认。各数据集分别列示，不与下方联合赛重复相加。

## 本地联合对战与 Elo

### 测试配置

- 参赛程序：14 位官方选手的下半场版本、出题方参照、基准、3 款社区 bot，以及 NNUE Vanguard、MCTS Vanguard，共 21 款。
- 完整两两对战：210 组，每组 20 局，共 4,200 局；每款程序均参加 400 局。
- 地图种子：1001–1010，风格按官方 auto 模式轮换，每张地图换边各打一局。
- 原生正式模式，首回合 2000 CPU 毫秒，之后每回合 50 CPU 毫秒，8 个并行位。
- 复用同机、同源码、同地图和预算的既有联合赛 3,060 局；本版官方自测提供 640 局，再补齐 500 局。受 NNUE、MCTS 和 Overmatch 源码变化影响的 1,140 局均已重新执行。
- 环境：Windows-10-10.0.19045-SP0；Python 3.13.1；g++.exe (GCC) 15.2.0；16 个逻辑核。

### Elo 计算方法

使用仓库 `tank/tournament/bt.py` 的 Bradley–Terry 模型，一次性共同拟合全部 21 款程序。基准程序设为 0 分；胜记 1、平记 0.5、负记 0。预期得分率为：

`P(A 对 B) = 1 / (1 + 10^((Elo_B - Elo_A) / 400))`

每对选手加入一局虚拟平局作正则化，避免全胜或全负造成无穷分数；虚拟局不计入战绩及 4,200 局总数。分数为本次样本的点估计，随地图、机器与运行负载变化。

### 联合 Elo 与总战绩

| 序号 | ID | 程序 | 类别 | Elo | 胜 | 平 | 负 | 局数 | 胜率 | 得分率 |
|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | MCTS | MCTS Vanguard | 本次作品 | 879.7 | 380 | 0 | 20 | 400 | 95.00% | 95.00% |
| 2 | NNUE | NNUE Vanguard | 本次作品 | 798.8 | 365 | 1 | 34 | 400 | 91.25% | 91.38% |
| 3 | ValuePUCT | value-based PUCT | 社区 | 711.9 | 347 | 0 | 53 | 400 | 86.75% | 86.75% |
| 4 | Overmatch | Overmatch（超越火力） | 社区 | 698.9 | 344 | 0 | 56 | 400 | 86.00% | 86.00% |
| 5 | NeuralSearch | 神经网络搜索 (自适应预算版) | 社区 | 602.5 | 320 | 0 | 80 | 400 | 80.00% | 80.00% |
| 6 | Astra | GPT-6 Astra | 官方 | 463.7 | 280 | 1 | 119 | 400 | 70.00% | 70.12% |
| 7 | sparring | 出题方参照 | 参照 | 388.9 | 257 | 0 | 143 | 400 | 64.25% | 64.25% |
| 8 | SolUF | GPT-6.1 Sol Ultrafast（Codex CLI） | 官方 | 379.7 | 254 | 0 | 146 | 400 | 63.50% | 63.50% |
| 9 | Gemini | Gemini 4 Argon Contributor（OpenCode Go） | 官方 | 302.0 | 228 | 0 | 172 | 400 | 57.00% | 57.00% |
| 10 | Sol | GPT-6.1 Sol | 官方 | 281.6 | 221 | 0 | 179 | 400 | 55.25% | 55.25% |
| 11 | Opus | Claude Opus 5.5 | 官方 | 258.7 | 213 | 0 | 187 | 400 | 53.25% | 53.25% |
| 12 | Muse | Muse Wonder Full | 官方 | 76.6 | 147 | 1 | 252 | 400 | 36.75% | 36.88% |
| 13 | MiMo | 小米 MiMo V2.6 Pro | 官方 | 44.8 | 136 | 0 | 264 | 400 | 34.00% | 34.00% |
| 14 | DeepSeek | DeepSeek V4.1 Flash | 官方 | 37.8 | 132 | 3 | 265 | 400 | 33.00% | 33.38% |
| 15 | Grok | Grok 4.7 | 官方 | 8.5 | 123 | 0 | 277 | 400 | 30.75% | 30.75% |
| 16 | baseline | 基准 | 参照 | 0.0 | 120 | 0 | 280 | 400 | 30.00% | 30.00% |
| 17 | Doubao | Doubao-Seed-Evolving | 官方 | -31.4 | 108 | 2 | 290 | 400 | 27.00% | 27.25% |
| 18 | Kimi | Kimi K3 | 官方 | -149.5 | 68 | 5 | 327 | 400 | 17.00% | 17.62% |
| 19 | Haiku | Claude Haiku 5.5 | 官方 | -151.1 | 69 | 2 | 329 | 400 | 17.25% | 17.50% |
| 20 | Qwen | 通义千问 Qwen 3.8 Max | 官方 | -239.7 | 45 | 1 | 354 | 400 | 11.25% | 11.38% |
| 21 | GLM | 智谱 GLM 5.3 | 官方 | -283.7 | 34 | 2 | 364 | 400 | 8.50% | 8.75% |

### 逐对战绩

每行胜、平、负均从 A 的角度统计；所有对阵均为 20 局。ID 与上表一致。

| A | B | A 胜 | 平 | A 负 | A 胜率 | A 得分率 |
|---|---|---:|---:|---:|---:|---:|
| MCTS | NNUE | 12 | 0 | 8 | 60.00% | 60.00% |
| MCTS | ValuePUCT | 15 | 0 | 5 | 75.00% | 75.00% |
| MCTS | Overmatch | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | NeuralSearch | 18 | 0 | 2 | 90.00% | 90.00% |
| MCTS | sparring | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | SolUF | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | Sol | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | Opus | 18 | 0 | 2 | 90.00% | 90.00% |
| MCTS | Muse | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | MiMo | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| MCTS | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| NNUE | ValuePUCT | 19 | 0 | 1 | 95.00% | 95.00% |
| NNUE | Overmatch | 14 | 0 | 6 | 70.00% | 70.00% |
| NNUE | NeuralSearch | 17 | 0 | 3 | 85.00% | 85.00% |
| NNUE | sparring | 18 | 0 | 2 | 90.00% | 90.00% |
| NNUE | SolUF | 18 | 0 | 2 | 90.00% | 90.00% |
| NNUE | Sol | 19 | 0 | 1 | 95.00% | 95.00% |
| NNUE | Opus | 18 | 0 | 2 | 90.00% | 90.00% |
| NNUE | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| NNUE | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| ValuePUCT | sparring | 20 | 0 | 0 | 100.00% | 100.00% |
| ValuePUCT | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| Overmatch | ValuePUCT | 8 | 0 | 12 | 40.00% | 40.00% |
| Overmatch | sparring | 20 | 0 | 0 | 100.00% | 100.00% |
| Overmatch | SolUF | 20 | 0 | 0 | 100.00% | 100.00% |
| Overmatch | Sol | 20 | 0 | 0 | 100.00% | 100.00% |
| Overmatch | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| Overmatch | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| NeuralSearch | ValuePUCT | 4 | 0 | 16 | 20.00% | 20.00% |
| NeuralSearch | Overmatch | 6 | 0 | 14 | 30.00% | 30.00% |
| NeuralSearch | sparring | 18 | 0 | 2 | 90.00% | 90.00% |
| NeuralSearch | SolUF | 19 | 0 | 1 | 95.00% | 95.00% |
| NeuralSearch | Sol | 19 | 0 | 1 | 95.00% | 95.00% |
| NeuralSearch | Opus | 19 | 0 | 1 | 95.00% | 95.00% |
| NeuralSearch | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| NeuralSearch | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | MCTS | 2 | 0 | 18 | 10.00% | 10.00% |
| Astra | NNUE | 4 | 1 | 15 | 20.00% | 22.50% |
| Astra | ValuePUCT | 3 | 0 | 17 | 15.00% | 15.00% |
| Astra | Overmatch | 1 | 0 | 19 | 5.00% | 5.00% |
| Astra | NeuralSearch | 6 | 0 | 14 | 30.00% | 30.00% |
| Astra | sparring | 13 | 0 | 7 | 65.00% | 65.00% |
| Astra | SolUF | 14 | 0 | 6 | 70.00% | 70.00% |
| Astra | Gemini | 16 | 0 | 4 | 80.00% | 80.00% |
| Astra | Sol | 18 | 0 | 2 | 90.00% | 90.00% |
| Astra | Opus | 10 | 0 | 10 | 50.00% | 50.00% |
| Astra | Muse | 19 | 0 | 1 | 95.00% | 95.00% |
| Astra | MiMo | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | DeepSeek | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | Grok | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | baseline | 18 | 0 | 2 | 90.00% | 90.00% |
| Astra | Doubao | 18 | 0 | 2 | 90.00% | 90.00% |
| Astra | Kimi | 18 | 0 | 2 | 90.00% | 90.00% |
| Astra | Haiku | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| Astra | GLM | 20 | 0 | 0 | 100.00% | 100.00% |
| SolUF | ValuePUCT | 2 | 0 | 18 | 10.00% | 10.00% |
| SolUF | sparring | 8 | 0 | 12 | 40.00% | 40.00% |
| SolUF | baseline | 18 | 0 | 2 | 90.00% | 90.00% |
| Gemini | MCTS | 1 | 0 | 19 | 5.00% | 5.00% |
| Gemini | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| Gemini | ValuePUCT | 2 | 0 | 18 | 10.00% | 10.00% |
| Gemini | Overmatch | 3 | 0 | 17 | 15.00% | 15.00% |
| Gemini | NeuralSearch | 4 | 0 | 16 | 20.00% | 20.00% |
| Gemini | sparring | 7 | 0 | 13 | 35.00% | 35.00% |
| Gemini | SolUF | 3 | 0 | 17 | 15.00% | 15.00% |
| Gemini | Sol | 13 | 0 | 7 | 65.00% | 65.00% |
| Gemini | Opus | 12 | 0 | 8 | 60.00% | 60.00% |
| Gemini | Muse | 16 | 0 | 4 | 80.00% | 80.00% |
| Gemini | MiMo | 16 | 0 | 4 | 80.00% | 80.00% |
| Gemini | Grok | 18 | 0 | 2 | 90.00% | 90.00% |
| Gemini | baseline | 18 | 0 | 2 | 90.00% | 90.00% |
| Gemini | Kimi | 17 | 0 | 3 | 85.00% | 85.00% |
| Gemini | Haiku | 19 | 0 | 1 | 95.00% | 95.00% |
| Gemini | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| Sol | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Sol | sparring | 5 | 0 | 15 | 25.00% | 25.00% |
| Sol | SolUF | 4 | 0 | 16 | 20.00% | 20.00% |
| Sol | baseline | 19 | 0 | 1 | 95.00% | 95.00% |
| Opus | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Opus | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Opus | sparring | 6 | 0 | 14 | 30.00% | 30.00% |
| Opus | SolUF | 7 | 0 | 13 | 35.00% | 35.00% |
| Opus | Sol | 5 | 0 | 15 | 25.00% | 25.00% |
| Opus | baseline | 20 | 0 | 0 | 100.00% | 100.00% |
| Opus | Qwen | 19 | 0 | 1 | 95.00% | 95.00% |
| Muse | NNUE | 1 | 0 | 19 | 5.00% | 5.00% |
| Muse | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Muse | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Muse | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| Muse | sparring | 4 | 0 | 16 | 20.00% | 20.00% |
| Muse | SolUF | 1 | 0 | 19 | 5.00% | 5.00% |
| Muse | Sol | 6 | 0 | 14 | 30.00% | 30.00% |
| Muse | Opus | 0 | 0 | 20 | 0.00% | 0.00% |
| Muse | baseline | 10 | 0 | 10 | 50.00% | 50.00% |
| Muse | Qwen | 16 | 1 | 3 | 80.00% | 82.50% |
| MiMo | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| MiMo | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| MiMo | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| MiMo | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| MiMo | sparring | 1 | 0 | 19 | 5.00% | 5.00% |
| MiMo | SolUF | 2 | 0 | 18 | 10.00% | 10.00% |
| MiMo | Sol | 2 | 0 | 18 | 10.00% | 10.00% |
| MiMo | Opus | 10 | 0 | 10 | 50.00% | 50.00% |
| MiMo | Muse | 6 | 0 | 14 | 30.00% | 30.00% |
| MiMo | baseline | 10 | 0 | 10 | 50.00% | 50.00% |
| MiMo | Qwen | 17 | 0 | 3 | 85.00% | 85.00% |
| DeepSeek | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| DeepSeek | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| DeepSeek | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| DeepSeek | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| DeepSeek | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| DeepSeek | sparring | 1 | 0 | 19 | 5.00% | 5.00% |
| DeepSeek | SolUF | 4 | 0 | 16 | 20.00% | 20.00% |
| DeepSeek | Gemini | 3 | 0 | 17 | 15.00% | 15.00% |
| DeepSeek | Sol | 4 | 0 | 16 | 20.00% | 20.00% |
| DeepSeek | Opus | 3 | 0 | 17 | 15.00% | 15.00% |
| DeepSeek | Muse | 10 | 0 | 10 | 50.00% | 50.00% |
| DeepSeek | MiMo | 12 | 0 | 8 | 60.00% | 60.00% |
| DeepSeek | Grok | 14 | 0 | 6 | 70.00% | 70.00% |
| DeepSeek | baseline | 8 | 0 | 12 | 40.00% | 40.00% |
| DeepSeek | Doubao | 9 | 1 | 10 | 45.00% | 47.50% |
| DeepSeek | Kimi | 16 | 1 | 3 | 80.00% | 82.50% |
| DeepSeek | Haiku | 16 | 0 | 4 | 80.00% | 80.00% |
| DeepSeek | Qwen | 15 | 0 | 5 | 75.00% | 75.00% |
| DeepSeek | GLM | 17 | 1 | 2 | 85.00% | 87.50% |
| Grok | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | sparring | 1 | 0 | 19 | 5.00% | 5.00% |
| Grok | SolUF | 0 | 0 | 20 | 0.00% | 0.00% |
| Grok | Sol | 1 | 0 | 19 | 5.00% | 5.00% |
| Grok | Opus | 4 | 0 | 16 | 20.00% | 20.00% |
| Grok | Muse | 3 | 0 | 17 | 15.00% | 15.00% |
| Grok | MiMo | 9 | 0 | 11 | 45.00% | 45.00% |
| Grok | baseline | 11 | 0 | 9 | 55.00% | 55.00% |
| Grok | Kimi | 16 | 0 | 4 | 80.00% | 80.00% |
| Grok | Haiku | 15 | 0 | 5 | 75.00% | 75.00% |
| Grok | Qwen | 20 | 0 | 0 | 100.00% | 100.00% |
| baseline | sparring | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | sparring | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | SolUF | 2 | 0 | 18 | 10.00% | 10.00% |
| Doubao | Gemini | 2 | 0 | 18 | 10.00% | 10.00% |
| Doubao | Sol | 0 | 0 | 20 | 0.00% | 0.00% |
| Doubao | Opus | 2 | 0 | 18 | 10.00% | 10.00% |
| Doubao | Muse | 10 | 0 | 10 | 50.00% | 50.00% |
| Doubao | MiMo | 6 | 0 | 14 | 30.00% | 30.00% |
| Doubao | Grok | 4 | 0 | 16 | 20.00% | 20.00% |
| Doubao | baseline | 14 | 0 | 6 | 70.00% | 70.00% |
| Doubao | Kimi | 13 | 1 | 6 | 65.00% | 67.50% |
| Doubao | Haiku | 12 | 0 | 8 | 60.00% | 60.00% |
| Doubao | Qwen | 13 | 0 | 7 | 65.00% | 65.00% |
| Doubao | GLM | 18 | 0 | 2 | 90.00% | 90.00% |
| Kimi | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | sparring | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | SolUF | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | Sol | 0 | 0 | 20 | 0.00% | 0.00% |
| Kimi | Opus | 5 | 0 | 15 | 25.00% | 25.00% |
| Kimi | Muse | 4 | 0 | 16 | 20.00% | 20.00% |
| Kimi | MiMo | 5 | 0 | 15 | 25.00% | 25.00% |
| Kimi | baseline | 3 | 0 | 17 | 15.00% | 15.00% |
| Kimi | Qwen | 12 | 0 | 8 | 60.00% | 60.00% |
| Haiku | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | sparring | 1 | 0 | 19 | 5.00% | 5.00% |
| Haiku | SolUF | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | Sol | 0 | 0 | 20 | 0.00% | 0.00% |
| Haiku | Opus | 3 | 0 | 17 | 15.00% | 15.00% |
| Haiku | Muse | 1 | 0 | 19 | 5.00% | 5.00% |
| Haiku | MiMo | 2 | 0 | 18 | 10.00% | 10.00% |
| Haiku | baseline | 7 | 0 | 13 | 35.00% | 35.00% |
| Haiku | Kimi | 8 | 2 | 10 | 40.00% | 45.00% |
| Haiku | Qwen | 18 | 0 | 2 | 90.00% | 90.00% |
| Qwen | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| Qwen | sparring | 0 | 0 | 20 | 0.00% | 0.00% |
| Qwen | SolUF | 0 | 0 | 20 | 0.00% | 0.00% |
| Qwen | Sol | 0 | 0 | 20 | 0.00% | 0.00% |
| Qwen | baseline | 4 | 0 | 16 | 20.00% | 20.00% |
| GLM | MCTS | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | NNUE | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | ValuePUCT | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Overmatch | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | NeuralSearch | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | sparring | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | SolUF | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Gemini | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Sol | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Opus | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Muse | 2 | 0 | 18 | 10.00% | 10.00% |
| GLM | MiMo | 2 | 0 | 18 | 10.00% | 10.00% |
| GLM | Grok | 1 | 0 | 19 | 5.00% | 5.00% |
| GLM | baseline | 0 | 0 | 20 | 0.00% | 0.00% |
| GLM | Kimi | 8 | 1 | 11 | 40.00% | 42.50% |
| GLM | Haiku | 9 | 0 | 11 | 45.00% | 45.00% |
| GLM | Qwen | 8 | 0 | 12 | 40.00% | 40.00% |

### 运行故障

| ID | 超时回合 | 非法动作回合 | 崩溃或故障退出局数 |
|---|---:|---:|---:|
| MCTS | 0 | 0 | 0 |
| NNUE | 0 | 0 | 0 |
| ValuePUCT | 0 | 0 | 0 |
| Overmatch | 0 | 0 | 0 |
| NeuralSearch | 0 | 0 | 0 |
| Astra | 0 | 0 | 0 |
| sparring | 0 | 0 | 0 |
| SolUF | 0 | 0 | 0 |
| Gemini | 0 | 0 | 0 |
| Sol | 0 | 0 | 0 |
| Opus | 0 | 0 | 0 |
| Muse | 0 | 0 | 0 |
| MiMo | 0 | 0 | 0 |
| DeepSeek | 0 | 0 | 0 |
| Grok | 0 | 0 | 0 |
| baseline | 0 | 0 | 0 |
| Doubao | 0 | 0 | 0 |
| Kimi | 0 | 0 | 0 |
| Haiku | 0 | 0 | 0 |
| Qwen | 0 | 0 | 0 |
| GLM | 0 | 0 | 0 |

### 参赛源码版本

源码在开赛前按 SHA-256 冻结；下列路径均相对于仓库根目录。

| ID | 源码 | SHA-256 |
|---|---|---|
| MCTS | `community/tank/lite0812__mcts-vanguard/bot.cpp` | `5ff13e2891898b9a542ded1cb00cfe2d1c030bc3fed4c3d2334d736c35f2eb67` |
| NNUE | `community/tank/lite0812__nnue-vanguard/bot.cpp` | `12f06e9d1446b20af119baeb628d7790159ffdc5aa356db63f30b25b95ccc5c3` |
| ValuePUCT | `community/tank/slzhta__value-based-puct/bot.cpp` | `f8e961e3bde05828b1cdefae0ac09abe0b0f30e6517a1edbbb45f0897c5a5d69` |
| Overmatch | `community/tank/genshinkleenahida__overmatch/bot.cpp` | `628a67a5845de0d8a99df35273a9b9bc63d524e77d8b30bf8524846dd27ea7e3` |
| NeuralSearch | `community/tank/mbfcasper__neural-search/bot.cpp` | `3dd95c2ee1a5e4de2a3fbc6b352459a7b3d94b97794cbaba6b40ee647ca3cd5d` |
| Astra | `tank/bots/half2/gpt-6-astra.cpp` | `f00b2d5635fa27ecb4397686e5fa836ebff629f5a65a5285a5d7d19110eb35e7` |
| sparring | `tank/tournament/sparring.cpp` | `1a8b3c75181de5bdeae77b669da893743ef3a772e67880a1a9313dcdb1abfff0` |
| SolUF | `tank/bots/half2/gpt-6.1-sol-ultrafast.cpp` | `63319e0a2b1a31015630c062125202626c40b3345054a31f60341d920ee78abd` |
| Gemini | `tank/bots/half2/gemini-4-argon.cpp` | `41c1341d0a8ac9357d704639940c3c0b0a9dfb248352ecb531a9905889288947` |
| Sol | `tank/bots/half2/gpt-6.1-sol.cpp` | `e34c0f45539a5f6b98c0e89848216a24dae09dbf078e0657751e6c1e099a8b75` |
| Opus | `tank/bots/half2/opus-5.5.cpp` | `c026348ce96efb8c12bc8fa7a5f782291b74a3ce4730745dcaf47ce213ec51df` |
| Muse | `tank/bots/half2/muse-wonder-full.cpp` | `4a21366e94d1a00e4b88df5cc912aef88094bfffa93f061705fbfd0e797b84e3` |
| MiMo | `tank/bots/half2/mimo-v2.6-pro.cpp` | `dbc4e9969c5229fc3995215e9932539a11c109698c5b791cc08bb36137364b18` |
| DeepSeek | `tank/bots/half2/deepseek-v4.1-flash.cpp` | `1ebbb227be6875d57d0f280965f312294705e97e8335215db533edd65903cfb1` |
| Grok | `tank/bots/half2/grok-4.7.cpp` | `723cd036eb314020b0a450ccc3564e77231bd93da21a63527f2e9ab9a1880145` |
| baseline | `tank/workspace/baseline.cpp` | `1e0803a541e875342aa2e4a598dd584e44b7382e0ca1d11679ab79b5b9ef04e9` |
| Doubao | `tank/bots/half2/doubao-seed-evolving.cpp` | `aeb0e7202ad5a5ad7b4d2b11a74173ae61edbb53f622317e6ab2d258b5d5c403` |
| Kimi | `tank/bots/half2/kimi-k3.cpp` | `b21ebeb9bc7626a6102779fedb1010e4bff86f65dee1c5cd7431ae58772148af` |
| Haiku | `tank/bots/half2/haiku-5.5.cpp` | `54cd8653190543f3646b7fd4ba2f33a9a798295b7f90889cafbb4c7bd00e250a` |
| Qwen | `tank/bots/half2/qwen-3.8-max.cpp` | `efdb8e35c0bffa8206993a21011369ddc2a8e6ee86445b4521b90f84db1c8fc0` |
| GLM | `tank/bots/half2/glm-5.3.cpp` | `681423c23fdb8324b42258bf77a57aae9733d47cf664ea26c2924e4507a22162` |

测试完成时间：2026-10-27T01:47:23+08:00。

这里的本机联合 Elo 对全部 21 款程序共同拟合；社区投稿自测则固定终场赛对手分数，只拟合投稿者。二者口径不同，均不能当成节目正式赛名次。

## 已知局限

局部转置可能合并全局状态不同的局面，分车统计不能穷举三车联合行动。远距离回防、友伤和长期资源交换仍依赖网络泛化。对强社区 bot 的稳定 90% 胜率尚未达到，也不能宣称对全部官方选手不败。
