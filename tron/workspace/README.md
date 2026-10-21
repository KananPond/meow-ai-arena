# 光轮对战（Tron）

写一个 bot：**只改 `submission/bot.cpp`**。规则见 `RULES.md`（不到 100 行，请先读完）。

**时间**：解压时开始计时，推荐 45 分钟内完成，最长 60 分钟强制收卷。随时运行 `.\run.cmd clock.py` 看已用时间。最终只看 bot 的强度。交卷时 `submission/bot.cpp` 的内容就是你的提交；另用几句话说明你的策略。

## 怎么写

```cpp
#include "tron.hpp"
int decide(const tron::State& s) {     // 每回合调用一次
    // s.me 是你，s.N 是人数，s.cell[y][x] 是棋盘，s.x[k] s.y[k] s.alive[k] 是各人状态
    return tron::U;                    // 返回 tron::U / R / D / L
}
```

现在的 `submission/bot.cpp` 是能合法行动的最简单版本（能直走就直走）。`bots/greedy.cpp` 是计分的零点（得 0 分），`bots/random.cpp` 是随机走，都可以参考。`bots/` 里还有两个陪练，只有编译好的程序，不参与计分：`sparring`（中等强度）和 `sparring_hard`（强）。

## 命令

在本目录运行（PowerShell / 命令提示符；bash 里把 `.\run.cmd` 换成 `sh run.sh`）。**不用自己编译**：遇到 `.cpp` 会自动编译并缓存，编译错误会直接显示。

```text
.\run.cmd arena.py check                              检查：能编译，2/3/4/12 人局各打一局，两种模式都跑通
.\run.cmd arena.py run                                估分：你的 bot 打 2/3/4/12 人局，按正式公式算分（约半分钟）
.\run.cmd arena.py run --n 1v1 --opp sparring         只打 1v1，对手只用陪练
.\run.cmd arena.py run --opp sparring_hard            对手换成强陪练
.\run.cmd arena.py run --opp greedy,history/旧版.cpp   对手可以是任何 .cpp，用逗号分隔，轮流坐进其余座位
.\run.cmd arena.py play submission/bot.cpp sparring --seed 5     打一局（正式模式），显示结果、调试输出、终局棋盘，存回放
.\run.cmd arena.py play submission/bot.cpp greedy greedy sparring 4 人局：列几个参赛者就是几人局（2、3、4、12）
.\run.cmd arena.py show replays/game-xxxx.json 40      看回放第 40 回合结算后的棋盘
```

`run` 的参数：`--n`（`1v1`、`3-4`、`12`、`all`，或 `2,3,4,12`）、`--opp`（默认 `greedy,sparring`）、`--games`（每种人数的局数）、`--seed`（换一批地图）、`--formal`（正式模式）、`--workers`（并行数）。参赛者可以写 `greedy`、`random`、`sparring`、`sparring_hard` 或文件路径。

## 怎么读 `run` 的输出

每种人数一行：局数、你拿第一的比例、你的平均名次分、**greedy 替身**的平均名次分、两者之差和 95% 区间。"greedy 替身"是把你换成 greedy、对手地图座位都不变再打一遍——它衡量的是你比 greedy 强多少；正式计分用 Bradley–Terry 实力值（RULES.md 第 3 节），尺度不同，但方向一致。最后一行是 ×100 后的各局型分数和加权总分（1v1 50%、3～4 人 30%、12 人 20%）。差值的区间跨过 0 时，说明还没看出区别，加 `--games` 再看。本地的对手和正式比赛不同，分数只作比较版本之用。

## 快速模式与正式模式

| | 快速模式（`run` 默认） | 正式模式（`play` 默认，`run --formal`） |
|---|---|---|
| 怎么跑 | 所有 bot 装进同一个程序、多线程 | 每个 bot 一个独立进程，和正式比赛完全一样 |
| 思考预算 | 只统计，不判罚 | 超了判罚（本回合沿上一回合方向走） |
| 出错 | 任何 bot 崩溃，整批停下并报告是哪一局 | 照常判罚，调试输出存进回放 |

不看时间的 bot（固定搜索量，或者不搜索）两种模式打出的对局逐回合相同。按 `budget_left()` 停的 bot 能想多深取决于机器忙不忙，结果会有波动。**建议给搜索留一个"固定搜索量"的开关**：比较两个版本时打开、用快速模式；交卷前关掉，再用正式模式（`play` 或 `run --formal`）确认没有超时、没有崩溃。

## 其他

- 每次运行 `check`、`run`、`play`，参与的源码都会按内容存一份到 `history/`（文件名带时间和哈希），`history/log.jsonl` 记下命令和结果。想找回旧版本去那里找，请不要删除。
- **看回放**：用浏览器打开 `viewer.html`，把 `replays/` 里的回放拖进去（空格播放、← → 单步，按 H 看全部快捷键）。
- 文件：`tron.hpp` 规则引擎（裁判用同一份）；`tron_main.cpp` 主循环（不用改）；`arena.py`、`judge/` 本地对战与裁判；`bots/` 基准与陪练；`clock.py`、`run.cmd`、`run.sh` 时钟与启动器；`build/`、`replays/`、`history/` 运行后自动生成。
- 正式比赛的编译命令（一般用不到）：`g++ -std=c++20 -O2 -static -Wl,--stack,67108864 -I. submission/bot.cpp tron_main.cpp -o bot.exe`
