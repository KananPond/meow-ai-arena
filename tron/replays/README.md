# 回放：随机抽样，不挑局

30 局，用浏览器直接打开 `.html` 就能播（查看器和回放打包在一个网页里，不需要联网）。空格播放 / 暂停，← → 单步（Shift 一次 10 回合），`[` `]` 上 / 下一个事件（出局、抢格、互换），`V` 显示地盘，`R` 录屏模式，按 `H` 看全部快捷键。网址后面加 `?turn=40` 停在第 40 回合，加 `?check=1` 让查看器按规则引擎重算全局并和回放逐项核对。

## 怎么抽的

- **01～27**（10-18 终场）：用 `random.Random(20261018)` 从终场"按固定规则存了回放"的局里抽。存回放的规则是赛前定的：1v1 只存第 0 张图（所有对子换边各一局），4 人局只存前 2 轮，12 人局全部存。另有 14 局 1v1 是因为出现超时额外存的（按结果挑出来的），不进抽样范围。顺序：先给 12 位上榜者（两张榜所有占名次的 11 位 + sparring_hard，名字按字母排序）各抽 1 局 1v1，再按同样顺序各抽 1 局 4 人局，最后 12 人局正式榜、强对手榜、两榜合并各抽 1 局。每次把候选局按局号排好、打乱、取第一个还没被抽过的。没有任何按结果、回合数或画面的筛选。
- **28～30**（10-20 补打，都有 Mercury）：10-18 抽样时 Mercury 还没参赛。30 号 12 人局是视频制作时用 `random.Random(20261020)` 从正式榜补打的 240 局 12 人局里抽的；28、29 号是整理开源时用 `random.Random(20261021)` 按同样规则抽的（1v1 从第 0 张图上 Mercury 的 24 局里抽，4 人局从正式榜存了回放的 8 局里抽）。
- 一局只记在抽它的那位名下，同一局不会被抽两次。1v1 两张榜共用，所以榜名写"共用"。
- 单局结果随机性大，不要用一两局给选手下结论；整体成绩见 `../results/`。死因"撞自己的轨迹"也包括被围死后无路可走的最后一步。greedy、greedy_fill 只坐座位、不计分。

## 清单

| 序号 | 抽给谁 | 局型 | 榜 | 来源 | 回合 | 参赛者（按座位号） | 本局第 1 名 | 文件 |
|---:|---|---|---|---|---:|---|---|---|
| 01 | AstraUF | 1v1 | 共用 | 10-18 终场 | 42 | AstraUF、Dots | AstraUF | `01-1v1-AstraUF-vs-Dots.html` |
| 02 | Dots | 1v1 | 共用 | 10-18 终场 | 49 | Dots、sparring_hard | sparring_hard | `02-1v1-Dots-vs-sparring_hard.html` |
| 03 | Doubao | 1v1 | 共用 | 10-18 终场 | 46 | Mistral、Doubao | Mistral | `03-1v1-Mistral-vs-Doubao.html` |
| 04 | DSNew | 1v1 | 共用 | 10-18 终场 | 37 | Qwen27B、DSNew | DSNew | `04-1v1-Qwen27B-vs-DSNew.html` |
| 05 | Hunyuan | 1v1 | 共用 | 10-18 终场 | 100 | Pangu、Hunyuan | Hunyuan | `05-1v1-Pangu-vs-Hunyuan.html` |
| 06 | LongCat | 1v1 | 共用 | 10-18 终场 | 159 | Mistral、LongCat | LongCat | `06-1v1-Mistral-vs-LongCat.html` |
| 07 | Mistral | 1v1 | 共用 | 10-18 终场 | 124 | DSNew、Mistral | Mistral | `07-1v1-DSNew-vs-Mistral.html` |
| 08 | Pangu | 1v1 | 共用 | 10-18 终场 | 8 | Pangu、Step | Step | `08-1v1-Pangu-vs-Step.html` |
| 09 | Qwen27B | 1v1 | 共用 | 10-18 终场 | 87 | sparring_hard、Qwen27B | sparring_hard | `09-1v1-sparring_hard-vs-Qwen27B.html` |
| 10 | sparring_hard | 1v1 | 共用 | 10-18 终场 | 135 | sparring_hard、LongCat | sparring_hard | `10-1v1-sparring_hard-vs-LongCat.html` |
| 11 | Step | 1v1 | 共用 | 10-18 终场 | 73 | Mistral、Step | Mistral | `11-1v1-Mistral-vs-Step.html` |
| 12 | Wenxin | 1v1 | 共用 | 10-18 终场 | 93 | Wenxin、Hunyuan | Wenxin | `12-1v1-Wenxin-vs-Hunyuan.html` |
| 13 | AstraUF | 4 人局 | 强对手榜 | 10-18 终场 | 94 | AstraUF、Hunyuan、Pangu、Dots | AstraUF | `13-4p-strong-AstraUF-Hunyuan-Pangu-Dots.html` |
| 14 | Dots | 4 人局 | 强对手榜 | 10-18 终场 | 78 | Doubao、greedy、Dots、Wenxin | Doubao | `14-4p-strong-Doubao-greedy-Dots-Wenxin.html` |
| 15 | Doubao | 4 人局 | 强对手榜 | 10-18 终场 | 98 | DSNew、sparring_hard、Doubao、LongCat | sparring_hard | `15-4p-strong-DSNew-sparring_hard-Doubao-LongCat.html` |
| 16 | DSNew | 4 人局 | 强对手榜 | 10-18 终场 | 64 | Step、Hunyuan、DSNew、LongCat | Step | `16-4p-strong-Step-Hunyuan-DSNew-LongCat.html` |
| 17 | Hunyuan | 4 人局 | 强对手榜 | 10-18 终场 | 84 | Hunyuan、Pangu、Dots、AstraUF | Pangu | `17-4p-strong-Hunyuan-Pangu-Dots-AstraUF.html` |
| 18 | LongCat | 4 人局 | 强对手榜 | 10-18 终场 | 110 | LongCat、DSNew、sparring_hard、Doubao | DSNew | `18-4p-strong-LongCat-DSNew-sparring_hard-Doubao.html` |
| 19 | Mistral | 4 人局 | 正式榜 | 10-18 终场 | 109 | Pangu、Dots、Doubao、Mistral | Mistral | `19-4p-official-Pangu-Dots-Doubao-Mistral.html` |
| 20 | Pangu | 4 人局 | 正式榜 | 10-18 终场 | 174 | Step、Pangu、Mistral、LongCat | LongCat | `20-4p-official-Step-Pangu-Mistral-LongCat.html` |
| 21 | Qwen27B | 4 人局 | 强对手榜 | 10-18 终场 | 114 | Pangu、AstraUF、Qwen27B、sparring_hard | AstraUF | `21-4p-strong-Pangu-AstraUF-Qwen27B-sparring_hard.html` |
| 22 | sparring_hard | 4 人局 | 强对手榜 | 10-18 终场 | 121 | sparring_hard、Doubao、LongCat、DSNew | sparring_hard | `22-4p-strong-sparring_hard-Doubao-LongCat-DSNew.html` |
| 23 | Step | 4 人局 | 正式榜 | 10-18 终场 | 90 | LongCat、Step、Pangu、Mistral | Step | `23-4p-official-LongCat-Step-Pangu-Mistral.html` |
| 24 | Wenxin | 4 人局 | 强对手榜 | 10-18 终场 | 76 | Wenxin、greedy、Qwen27B、Mistral | Mistral | `24-4p-strong-Wenxin-greedy-Qwen27B-Mistral.html` |
| 25 | 正式榜的 12 人局 | 12 人局 | 正式榜 | 10-18 终场 | 111 | LongCat、Dots、Doubao、Hunyuan、Wenxin、greedy_fill、greedy、Mistral、greedy_fill、Qwen27B、Pangu、Step | Mistral | `25-12p-official-m05-rot01.html` |
| 26 | 强对手榜的 12 人局 | 12 人局 | 强对手榜 | 10-18 终场 | 158 | LongCat、greedy、Step、AstraUF、Hunyuan、Wenxin、Pangu、sparring_hard、Dots、Doubao、DSNew、Mistral | DSNew | `26-12p-strong-m17-rot10.html` |
| 27 | 两榜合并的 12 人局 | 12 人局 | 正式榜 | 10-18 终场 | 132 | Pangu、Mistral、greedy、Hunyuan、Doubao、greedy_fill、LongCat、Wenxin、Step、Qwen27B、greedy_fill、Dots | Mistral | `27-12p-official-m10-rot03.html` |
| 28 | Mercury | 1v1 | 共用 | 10-20 补打 | 86 | Mercury、Dots | Mercury | `28-1v1-Mercury-vs-Dots.html` |
| 29 | Mercury | 4 人局 | 正式榜 | 10-20 补打 | 75 | Mistral、Qwen27B、Mercury、Dots | Qwen27B | `29-4p-official-Mistral-Qwen27B-Mercury-Dots.html` |
| 30 | Mercury | 12 人局 | 正式榜 | 10-20 补打 | 128 | Wenxin、Pangu、Qwen27B、greedy、LongCat、Dots、Mistral、Step、Hunyuan、greedy_fill、Doubao、Mercury | Qwen27B | `30-12p-official-m09-rot03.html` |
