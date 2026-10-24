# 社区榜

这个文件由 `community/ci_tools.py render` 自动生成，投稿合并后由 GitHub Actions 更新，请不要手改。怎么投稿见 [README.md](README.md)。

成绩是在 GitHub 提供的虚拟机上测的：bot 类每份投稿只和官方选手打一轮几百局的小比赛，仓储调度的加速比是同一台虚拟机上基线和投稿的 CPU 时间之比。不同投稿不一定在同一台虚拟机上测，局数也少，名次相近的不必当真；**这些数字和节目里的正式成绩不能直接比。**

## 第 1 集 · 圈地（`land/`）

按实力值排序（单挑、混战两种赛制的 Bradley–Terry 实力值取平均，greedy 基准 = 0，强 10 倍记 +400）。“小比赛名次”是这份投稿在它自己那一轮小比赛里排第几。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 实力值 | 小比赛名次 | 评测日期 |
|---:|---|---|---|---|---:|---|---|
| 1 | [原点圈地](land/owo9891__origin-loop/) | OWO9891（[@owo9891](https://github.com/owo9891)） | AI 写 | deepseek-flash / Proma | 404 | 1 / 13 | 2026-10-24 |
| 2 | [墨痕喵](land/mohen2006__land-gpt-61-sol/) | mohen2006（[@mohen2006](https://github.com/mohen2006)） | AI 写 | gpt-6.1-sol / Codex | 337 | 1 / 13 | 2026-10-17 |
| 3 | [皇家圈地](land/owo9891__royal-loop/) | OWO9891（[@owo9891](https://github.com/owo9891)） | AI 写 | deepseek-flash / DeepSeek Harness | 319 | 2 / 13 | 2026-10-22 |
| 4 | [大方框](land/light-yuncai__big-loop/) | Light-YunCai（[@light-yuncai](https://github.com/light-yuncai)） | AI 写 | deepseek-flash / DeepSeek Harness | -6 | 10 / 13 | 2026-10-21 |
| 5 | [洪泛圈地](land/spicae__flood-claim/) | spicae（[@spicae](https://github.com/spicae)） | AI 写 | deepseek-V4-pro / WorkBuddy | -38 | 10 / 13 | 2026-10-20 |

官方选手参照：每份投稿评测时，官方选手和基准也在同一台虚拟机上打了同一轮小比赛。下表是它们在各次评测里实力值的中位数。

| 官方选手 | 实力值（中位数） | 出现在几次评测里 |
|---|---:|---:|
| sonnet-5@claude-code | 373 | 5 |
| gpt-6-astra@codex | 289 | 5 |
| opus-5.5@claude-code | 275 | 5 |
| anchor-sparring | 269 | 5 |
| gpt-6-sol@codex | 181 | 5 |
| mimo-2.6-pro@zcode | 167 | 5 |
| kimi-k3@workbuddy | 49 | 5 |
| anchor-greedy | 0 | 5 |
| glm-5.3@workbuddy | -4 | 5 |
| deepseek-4.1f@workbuddy | -104 | 5 |
| mystery-model@zcode | -124 | 5 |
| anchor-random | -327 | 5 |

## 第 2 集 · 炸弹人（`bomberman/`）

按实力值排序（Bradley–Terry，基准 = 0，强 10 倍记 +400）。“小比赛名次”是这份投稿在它自己那一轮小比赛里排第几。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 实力值 | 小比赛名次 | 评测日期 |
|---:|---|---|---|---|---:|---|---|
| 1 | [墨痕喵](bomberman/mohen2006__bomberman-gpt-61-sol/) | mohen2006（[@mohen2006](https://github.com/mohen2006)） | AI 写 | gpt-6.1-sol / Codex | 484 | 1 / 11 | 2026-10-20 |
| 2 | [deepseek屎山v35](bomberman/dinson98__deepseek-shishan-v35/) | Dinson（[@dinson98](https://github.com/dinson98)） | 人和 AI 合写 | DeepSeek-V4.1-Flash / WorkBuddy | 312 | 2 / 11 | 2026-10-20 |

官方选手参照：每份投稿评测时，官方选手和基准也在同一台虚拟机上打了同一轮小比赛。下表是它们在各次评测里实力值的中位数。

| 官方选手 | 实力值（中位数） | 出现在几次评测里 |
|---|---:|---:|
| GPT-6 Astra | 413 | 2 |
| Opus 5.5 | 295 | 2 |
| GPT-6 Sol | 282 | 2 |
| Qwen 3.8 Max | 252 | 2 |
| DeepSeek V4.1 Flash | 233 | 2 |
| Kimi K3 | 202 | 2 |
| GLM 5.3 | 190 | 2 |
| 豆包 Seed 2.1 | 124 | 2 |
| MiMo V2.6 Pro | 67 | 2 |
| Base | 0 | 2 |

## 第 3 集 · 夺旗（`ctf/`）

按实力值排序（Bradley–Terry，基准 = 0，强 10 倍记 +400）。“小比赛名次”是这份投稿在它自己那一轮小比赛里排第几。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 实力值 | 小比赛名次 | 评测日期 |
|---:|---|---|---|---|---:|---|---|
| 1 | [弈喵（MeowMind）](ctf/huixiong806__meowmind/) | orangebird（[@huixiong806](https://github.com/huixiong806)） | 人和 AI 合写 | GPT-6.1 Sol / Codex | 298 | 1 / 13 | 2026-10-13 |
| 2 | [Gemini 4 Argon](ctf/sugarzo__gemini-4-argon/) | sugarzo（[@sugarzo](https://github.com/sugarzo)） | AI 写 | Gemini 4 Argon / Cursor | 180 | 1 / 13 | 2026-10-17 |

官方选手参照：每份投稿评测时，官方选手和基准也在同一台虚拟机上打了同一轮小比赛。下表是它们在各次评测里实力值的中位数。

| 官方选手 | 实力值（中位数） | 出现在几次评测里 |
|---|---:|---:|
| GPT-6 Astra | 215 | 2 |
| GPT-6.1 Sol | 181 | 2 |
| Fable 5.1 | 174 | 2 |
| Sonnet 5.5 | 148 | 2 |
| Opus 5.5 | 136 | 2 |
| DeepSeek V4.1 Flash | 104 | 2 |
| MiMo V2.6 Pro | 13 | 2 |
| 基准 | 0 | 2 |
| Qwen 3.8 Max | -7 | 2 |
| Kimi K3 | -27 | 2 |
| GLM 5.3 | -62 | 2 |
| 豆包 Seed 2.1 Turbo | -186 | 2 |

## 第 5 集 · 仓储调度（`warehouse/`）

按几何平均加速比排序，只列全部负载输出都正确的投稿。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 几何平均加速比 | 输出正确 | 负载 | 评测日期 |
|---:|---|---|---|---|---:|---:|---|---|
| 1 | [稳流 SteadyFlow](warehouse/genshinkleenahida__steadyflow/) | genshinkleenahida（[@genshinkleenahida](https://github.com/genshinkleenahida)） | AI 写 | GPT-6 / Codex | 2971.0 倍 | 8 / 8 | 8 组隐藏负载 | 2026-10-21 |
| 2 | [循迹仓流](warehouse/genshinkleenahida__traceflow/) | genshinkleenahida（[@genshinkleenahida](https://github.com/genshinkleenahida)） | AI 写 | GPT-6 Astra Pro / ChatGPT（GitHub 插件） | 1789.2 倍 | 8 / 8 | 8 组隐藏负载 | 2026-10-21 |
| 3 | [缓存派单 + 位图寻路](warehouse/dafeiyuzhi__warehouse-cache/) | 志（[@dafeiyuzhi](https://github.com/dafeiyuzhi)） | AI 写 | DeepSeek（Harness 会话，具体型号未记录） / DeepSeek Harness | 235.2 倍 | 8 / 8 | 8 组隐藏负载 | 2026-10-22 |

官方选手的第 1 轮成绩见 [`warehouse/results/第1轮_速度成绩.md`](../warehouse/results/第1轮_速度成绩.md)，那是在比赛电脑上测的，评测脚本不会在虚拟机上重测它们，所以这里不列参照。

## 第 6 集 · 坦克大战（`tank/`）

按投稿实力值排序：正式模式挑战 14 位 AI、出题方参照和基准，固定终场赛对手评分，只拟合投稿的 Bradley–Terry 实力值（基准 = 0，强 10 倍 = +400）。

“终场赛参照名次”只把投稿分数放进 14 位 AI 的固定评分中比较，不是重跑正式赛。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 实力值 | 终场赛参照名次 | 评测日期 |
|---:|---|---|---|---|---:|---|---|
| 1 | [Overmatch（超越火力）](tank/genshinkleenahida__overmatch/) | genshinkleenahida（[@genshinkleenahida](https://github.com/genshinkleenahida)） | AI 写 | GPT-6.1 Sol / Codex | 833 | 1 / 15 | 2026-10-22 |
| 2 | [value-based PUCT](tank/slzhta__value-based-puct/) | slzhta（[@slzhta](https://github.com/slzhta)） | 人和 AI 合写 | GPT-6 Astra / Codex | 765 | 1 / 15 | 2026-10-21 |
| 3 | [神经网络搜索 (自适应预算版)](tank/mbfcasper__neural-search/) | mbfcasper（[@mbfcasper](https://github.com/mbfcasper)） | AI 写 | GPT-6.1 Sol + Gemini 4 Argon + Gemini 3.8 Flash + Claude Opus 5.5 / Codex / Antigravity / Claude Code | 670 | 1 / 15 | 2026-10-20 |

固定参照来自 [`tank/results/final/成绩.md`](../tank/results/final/成绩.md)。社区对局使用新地图，在不同机器上运行，局数较少；参照分数不是本次在虚拟机上重新测出的官方成绩。

## 第 7 集 · 光轮对战（`tron/`）

还没有投稿。
