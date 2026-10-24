# 峡谷对决

编写 `submission/bot.cpp`：一个程序统一控制己方 5 个英雄（上单战士、打野刺客、中单法师、射手、辅助），
在峡谷里推塔、拆水晶。完整规则和输入输出格式见 `RULES.md`。

解压时开始计时。你有 60 分钟，只看最终 bot 的强度，提前交卷不加分；可以适当超时，但到 90 分钟会强制交卷。

## 文件

| 文件 | 用途 |
|---|---|
| `RULES.md` | 规则与程序接口 |
| `submission/bot.cpp` | 你的程序。现在是一个模板：已经写好完整的输入解析，5 个英雄原地不动 |
| `baseline.exe` | 中等强度的练习对手（一支打法常规的队伍），没有提供源码，请不要反编译 |
| `arena.py`、`engine.py`、`core.py` | 本地对战用的裁判，与正式比赛相同 |
| `viewer.html` | 回放网页模板（所有图形都是 SVG 画的） |
| `clock.py` | 查看已用时间 |
| `run.cmd`、`run.sh` | 找到 Python 并运行上面的脚本 |

## 常用命令

在本目录运行。下面以 PowerShell 和命令提示符为例，写作 `.\run.cmd`；在 bash 里（包括 Windows 上的 Git Bash 和 Linux）把它换成 `sh run.sh`，后面的参数不变。

```text
.\run.cmd clock.py                                              已用时间
.\run.cmd arena.py check                                        检查环境
.\run.cmd arena.py play submission/bot.cpp baseline --seed 7    和基准打 5v5
.\run.cmd arena.py play submission/bot.cpp --difficulty 困难    单人挑战 NPC 队（困难）
.\run.cmd arena.py batch submission/bot.cpp baseline --games 8  连打多局看平均
.\run.cmd arena.py show replays/latest.json 300 --around 2      看第 298–302 回合
```

- `.cpp` 程序会先自动编译，结果按源码缓存在 `build/`，源码不变就不重复编译。编译出错时会显示编译器的报错。
- `play` 给 1 个程序就是单人挑战（对方是引擎 NPC 队，难度用 `--difficulty` 选）；给 2 个程序就是 5v5，
  第二个程序写 `baseline` 表示练习基准。回放存成 `replays/名字.json` 和 `replays/名字.html`，网页可以逐回合播放。
- `batch` 连打多局并汇总平均得分、场均击杀、推塔、故障和最慢用时，不保存回放。
  2 个程序时每张地图换边各打一场，局数最好取偶数。
- `--seed` 决定地图。地图和 NPC 完全由种子和难度决定，同一个种子重打结果完全一致。
- 程序可以重复出场，比如自己和自己对打。

## 提交

最终代码放在 `workspace/submission/bot.cpp`，结束时用几句话说明你的策略。
