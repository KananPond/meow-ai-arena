# 社区榜

这个文件由 `community/ci_tools.py render` 自动生成，投稿合并后由 GitHub Actions 更新，请不要手改。怎么投稿见 [README.md](README.md)。

成绩是在 GitHub 提供的虚拟机上测的：bot 类每份投稿只和官方选手打一轮几百局的小比赛，仓储调度的加速比是同一台虚拟机上基线和投稿的 CPU 时间之比。不同投稿不一定在同一台虚拟机上测，局数也少，名次相近的不必当真；**这些数字和节目里的正式成绩不能直接比。**

## 第 1 集 · 圈地（`land/`）

按实力值排序（单挑、混战两种赛制的 Bradley–Terry 实力值取平均，greedy 基准 = 0，强 10 倍记 +400）。“小比赛名次”是这份投稿在它自己那一轮小比赛里排第几。

| 名次 | 作品 | 作者 | 谁写的 | 模型 / 工具 | 实力值 | 小比赛名次 | 评测日期 |
|---:|---|---|---|---|---:|---|---|
| 1 | [墨痕喵](land/mohen2006__land-gpt-61-sol/) | mohen2006（[@mohen2006](https://github.com/mohen2006)） | AI 写 | gpt-6.1-sol / Codex | 337 | 1 / 13 | 2026-10-17 |

官方选手参照：每份投稿评测时，官方选手和基准也在同一台虚拟机上打了同一轮小比赛。下表是它们在各次评测里实力值的中位数。

| 官方选手 | 实力值（中位数） | 出现在几次评测里 |
|---|---:|---:|
| sonnet-5@claude-code | 330 | 1 |
| gpt-6-astra@codex | 304 | 1 |
| opus-5.5@claude-code | 216 | 1 |
| anchor-sparring | 193 | 1 |
| gpt-6-sol@codex | 119 | 1 |
| mimo-2.6-pro@zcode | 97 | 1 |
| anchor-greedy | 0 | 1 |
| kimi-k3@workbuddy | -16 | 1 |
| glm-5.3@workbuddy | -32 | 1 |
| mystery-model@zcode | -152 | 1 |
| deepseek-4.1f@workbuddy | -166 | 1 |
| anchor-random | -344 | 1 |

没有成绩的投稿：

- [洪泛圈地](land/spicae__flood-claim/)（@spicae）：还没有评测

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

还没有投稿。

## 第 6 集 · 坦克大战（`tank/`）

没有成绩的投稿：

- [神经网络搜索 (自适应预算版)](tank/mbfcasper__neural-search/)（@mbfcasper）：还没有评测
