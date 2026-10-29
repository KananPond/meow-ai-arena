[English](README_en.md)

# 三支参考队的源码

`bots.hpp` 与 `bot_main.cpp` 是比赛时参考 bot 的共用源码；只调整 `bots.hpp` 的引擎 include 相对路径。比赛时选手只拿到可执行程序，没有这些源码。

| 可执行程序 | 编译时 BOT_NAME |
|---|---|
| baseline | plan16 |
| midfield | plan32_b1.5 |
| sparring | pro_b0_n128 |

在上级目录运行 `python build_reference.py`，按原编译参数 `-std=c++17 -O2 -static -s` 生成到 `workspace/bots/`，并编译裁判。每支参考队两座使用同一个程序。它们不占名次、对它们的局不计积分，只用于参考成绩和 BT 拟合。比赛记录里的“基准/中档陪练/陪练”对应这三个名字。
