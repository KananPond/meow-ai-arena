[简体中文](README.md)

# Replays: random sample, no cherry-picking

30 games. Open `.html` in a browser offline: viewer and replay are bundled. Space play/pause, ← → step (Shift: 10 turns), `[` `]` previous/next event (death, contested cell, swap), `V` territory, `R` recording mode, `H` all shortcuts. Append `?turn=40` for turn 40, `?check=1` to rerun the rule engine and compare every replay field.

## Sampling

- **01–27 (10-18 final):** `random.Random(20261018)` sampled games whose replays were saved by preset rules: 1v1 map 0 only, both seats for every pair; 4-player first 2 rounds; all 12-player games. Another 14 overrun-triggered 1v1 saves were outcome-selected and excluded. In alphabetical order, each of the 12 listed programs (11 ranked across boards + sparring_hard) got one 1v1 sample, then one 4-player sample; finally one 12-player each from official, strong and both boards pooled. Sort candidates by game ID, shuffle, take first not previously drawn. No filtering by result, length or visuals.
- **28–30 (10-20 additions, all Mercury):** Mercury had not competed at initial sampling. Replay 30 was drawn during video production using `random.Random(20261020)` from 240 new official-board 12-player games. Replays 28–29 used `random.Random(20261021)` at release, same rules: 1v1 from Mercury's 24 map-0 games; 4-player from 8 saved official-board games.
- Each game is credited only to the program it was drawn for, never drawn twice. 1v1 is shared across boards.
- Single-game variability is large: do not judge contestants from one or two games. Overall results: `../results/`. Own-trail deaths include the last move with no escape after enclosure. greedy/greedy_fill are unscored fillers.

## List

| No. | Drawn for | Type | Board | Source | Turns | Players (seat order) | First place | File |
|---:|---|---|---|---|---:|---|---|---|
| 01 | AstraUF | 1v1 | Shared | 10-18 final | 42 | AstraUF, Dots | AstraUF | `01-1v1-AstraUF-vs-Dots.html` |
| 02 | Dots | 1v1 | Shared | 10-18 final | 49 | Dots, sparring_hard | sparring_hard | `02-1v1-Dots-vs-sparring_hard.html` |
| 03 | Doubao | 1v1 | Shared | 10-18 final | 46 | Mistral, Doubao | Mistral | `03-1v1-Mistral-vs-Doubao.html` |
| 04 | DSNew | 1v1 | Shared | 10-18 final | 37 | Qwen27B, DSNew | DSNew | `04-1v1-Qwen27B-vs-DSNew.html` |
| 05 | Hunyuan | 1v1 | Shared | 10-18 final | 100 | Pangu, Hunyuan | Hunyuan | `05-1v1-Pangu-vs-Hunyuan.html` |
| 06 | LongCat | 1v1 | Shared | 10-18 final | 159 | Mistral, LongCat | LongCat | `06-1v1-Mistral-vs-LongCat.html` |
| 07 | Mistral | 1v1 | Shared | 10-18 final | 124 | DSNew, Mistral | Mistral | `07-1v1-DSNew-vs-Mistral.html` |
| 08 | Pangu | 1v1 | Shared | 10-18 final | 8 | Pangu, Step | Step | `08-1v1-Pangu-vs-Step.html` |
| 09 | Qwen27B | 1v1 | Shared | 10-18 final | 87 | sparring_hard, Qwen27B | sparring_hard | `09-1v1-sparring_hard-vs-Qwen27B.html` |
| 10 | sparring_hard | 1v1 | Shared | 10-18 final | 135 | sparring_hard, LongCat | sparring_hard | `10-1v1-sparring_hard-vs-LongCat.html` |
| 11 | Step | 1v1 | Shared | 10-18 final | 73 | Mistral, Step | Mistral | `11-1v1-Mistral-vs-Step.html` |
| 12 | Wenxin | 1v1 | Shared | 10-18 final | 93 | Wenxin, Hunyuan | Wenxin | `12-1v1-Wenxin-vs-Hunyuan.html` |
| 13 | AstraUF | 4-player | Strong-opponent board | 10-18 final | 94 | AstraUF, Hunyuan, Pangu, Dots | AstraUF | `13-4p-strong-AstraUF-Hunyuan-Pangu-Dots.html` |
| 14 | Dots | 4-player | Strong-opponent board | 10-18 final | 78 | Doubao, greedy, Dots, Wenxin | Doubao | `14-4p-strong-Doubao-greedy-Dots-Wenxin.html` |
| 15 | Doubao | 4-player | Strong-opponent board | 10-18 final | 98 | DSNew, sparring_hard, Doubao, LongCat | sparring_hard | `15-4p-strong-DSNew-sparring_hard-Doubao-LongCat.html` |
| 16 | DSNew | 4-player | Strong-opponent board | 10-18 final | 64 | Step, Hunyuan, DSNew, LongCat | Step | `16-4p-strong-Step-Hunyuan-DSNew-LongCat.html` |
| 17 | Hunyuan | 4-player | Strong-opponent board | 10-18 final | 84 | Hunyuan, Pangu, Dots, AstraUF | Pangu | `17-4p-strong-Hunyuan-Pangu-Dots-AstraUF.html` |
| 18 | LongCat | 4-player | Strong-opponent board | 10-18 final | 110 | LongCat, DSNew, sparring_hard, Doubao | DSNew | `18-4p-strong-LongCat-DSNew-sparring_hard-Doubao.html` |
| 19 | Mistral | 4-player | Official board | 10-18 final | 109 | Pangu, Dots, Doubao, Mistral | Mistral | `19-4p-official-Pangu-Dots-Doubao-Mistral.html` |
| 20 | Pangu | 4-player | Official board | 10-18 final | 174 | Step, Pangu, Mistral, LongCat | LongCat | `20-4p-official-Step-Pangu-Mistral-LongCat.html` |
| 21 | Qwen27B | 4-player | Strong-opponent board | 10-18 final | 114 | Pangu, AstraUF, Qwen27B, sparring_hard | AstraUF | `21-4p-strong-Pangu-AstraUF-Qwen27B-sparring_hard.html` |
| 22 | sparring_hard | 4-player | Strong-opponent board | 10-18 final | 121 | sparring_hard, Doubao, LongCat, DSNew | sparring_hard | `22-4p-strong-sparring_hard-Doubao-LongCat-DSNew.html` |
| 23 | Step | 4-player | Official board | 10-18 final | 90 | LongCat, Step, Pangu, Mistral | Step | `23-4p-official-LongCat-Step-Pangu-Mistral.html` |
| 24 | Wenxin | 4-player | Strong-opponent board | 10-18 final | 76 | Wenxin, greedy, Qwen27B, Mistral | Mistral | `24-4p-strong-Wenxin-greedy-Qwen27B-Mistral.html` |
| 25 | Official 12-player | 12-player | Official board | 10-18 final | 111 | LongCat, Dots, Doubao, Hunyuan, Wenxin, greedy_fill, greedy, Mistral, greedy_fill, Qwen27B, Pangu, Step | Mistral | `25-12p-official-m05-rot01.html` |
| 26 | Strong 12-player | 12-player | Strong-opponent board | 10-18 final | 158 | LongCat, greedy, Step, AstraUF, Hunyuan, Wenxin, Pangu, sparring_hard, Dots, Doubao, DSNew, Mistral | DSNew | `26-12p-strong-m17-rot10.html` |
| 27 | Pooled 12-player | 12-player | Official board | 10-18 final | 132 | Pangu, Mistral, greedy, Hunyuan, Doubao, greedy_fill, LongCat, Wenxin, Step, Qwen27B, greedy_fill, Dots | Mistral | `27-12p-official-m10-rot03.html` |
| 28 | Mercury | 1v1 | Shared | 10-20 additions | 86 | Mercury, Dots | Mercury | `28-1v1-Mercury-vs-Dots.html` |
| 29 | Mercury | 4-player | Official board | 10-20 additions | 75 | Mistral, Qwen27B, Mercury, Dots | Qwen27B | `29-4p-official-Mistral-Qwen27B-Mercury-Dots.html` |
| 30 | Mercury | 12-player | Official board | 10-20 additions | 128 | Wenxin, Pangu, Qwen27B, greedy, LongCat, Dots, Mistral, Step, Hunyuan, greedy_fill, Doubao, Mercury | Qwen27B | `30-12p-official-m09-rot03.html` |
