# 示例投稿

这里的三个文件夹只用来演示投稿的格式，不参加社区榜，也不是谁的真实投稿。

| 文件夹 | 说明 |
|---|---|
| `ctf/chen-006__stand-still/` | bot 类投稿的样子：`bot.cpp` 加 `meta.json`。代码是第 3 集选手包里的模板，三个角色原地不动 |
| `tank/chen-006__starter/` | 坦克大战投稿的样子：`bot.cpp` 加 `meta.json`。第 6 集官方起步模板原样复制，只定义 `init` 和 `decide`，使用官方 `tank.hpp` 和 `tank_main.cpp`，不自带 `main` |
| `warehouse/chen-006__kimi-k3-round1/` | 仓储调度投稿的样子：`src/` 加 `meta.json`。代码是 Kimi K3 第 1 轮交的原样 |

可以拿它们试一下自测脚本：

```bash
python community/check.py community/_example/ctf/chen-006__stand-still
python community/check.py community/_example/tank/chen-006__starter --skip-eval
python community/check.py community/_example/tank/chen-006__starter
python community/check.py community/_example/warehouse/chen-006__kimi-k3-round1
```

坦克大战默认正式评测 320 局，`--full` 为 1280 局；`--skip-eval` 只检查结构和编译。它固定官方终场赛对手的实力值，只拟合投稿者的分数。

真正投稿时，文件夹要放在 `community/<集>/` 下，不是 `community/_example/<集>/` 下。规则见 [`../README.md`](../README.md)。
