"""用你的 bot 挑战第 7 集光轮对战的全部 AI：和 10 位正式选手、2 位参考选手、强档陪练线一起打 1v1、4 人局、12 人局
（正式模式，和比赛时一样每个 bot 一个进程、超预算判罚），再按终场强对手榜的实力值把你放到同一把尺上。

  python challenge.py                      用 workspace/submission/bot.cpp（选手拿到的起步模板，能直走就直走）
  python challenge.py 我的bot.cpp          用你自己的程序（1v1 52 局、4 人局 24 局、12 人局 6 局，几分钟）
  python challenge.py 我的bot.cpp --full   局数是上面的 4 倍，结果更稳

你的 bot 和比赛选手一样只写 decide 一个函数，规则和接口见 workspace/README.md、workspace/RULES.md。
需要 Python 3.10+ 和 g++（Windows 可用 w64devkit，见仓库根目录 README）。裁判只支持 Windows。
"""
import json
import math
import os
import random
import shutil
import sys
import time
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
HERE = Path(__file__).resolve().parent
WS = HERE / "workspace"
# 强对手榜里的短名 → 程序（sparring_hard 是强档陪练线，由 make_player_pack.py 从源码编出来）
OPPONENTS = {
    "Mistral": "bots/mistral-large-4.cpp", "LongCat": "bots/longcat-2.0.cpp", "Doubao": "bots/doubao-seed-evolving.cpp",
    "Step": "bots/step-5-preview.cpp", "Pangu": "bots/openpangu-2.0-pro.cpp", "Hunyuan": "bots/hunyuan-hy4-preview.cpp",
    "Qwen27B": "bots/qwen3.8-27b.cpp", "Mercury": "bots/mercury-3.cpp", "Wenxin": "bots/ernie-5.1.cpp",
    "Dots": "bots/dots3-note-preview.cpp", "AstraUF": "bots/reference/gpt-6-astra-ultra-fast.cpp",
    "DSNew": "bots/reference/deepseek-v4.1-flash-dsh.cpp", "sparring_hard": None,
}
MODES = {"1v1": 0.5, "3-4": 0.3, "12": 0.2}
SEED = 5001                       # 地图种子从这里起，和正式赛不是同一批图


def ensure_compiler():
    """arena.py 先看环境变量 CXX，再找 PATH 里的 g++。都没有时，按仓库根目录 README 的约定再找一下 w64devkit。"""
    if os.environ.get("CXX") or shutil.which("g++"):
        return
    for guess in (HERE.parent / "tools" / "w64devkit" / "bin" / "g++.exe", Path("C:/w64devkit/bin/g++.exe")):
        if guess.is_file():
            os.environ["CXX"] = str(guess)
            return
    raise SystemExit("没找到 C++ 编译器（g++）。Windows 推荐 w64devkit，放法见仓库根目录 README。")


def fit(rows):
    """对手的实力值固定，只解你一个人的实力值（Bradley–Terry，强 10 倍 = +400，每个对手加一局虚拟平局防止发散）。"""
    def loglik(r):
        total = 0.0
        for opp, w, d, l in rows:
            p = 1 / (1 + 10 ** ((opp - r) / 400))
            n, s = w + d + l + 1, w + 0.5 * d + 0.5
            total += s * math.log(p) + (n - s) * math.log(1 - p)
        return total
    lo, hi = -2000.0, 2500.0
    for _ in range(200):                       # 似然是单峰的，三分法就够
        a, b = lo + (hi - lo) / 3, hi - (hi - lo) / 3
        if loglik(a) < loglik(b):
            lo = a
        else:
            hi = b
    return (lo + hi) / 2


def schedule(names, full):
    """返回 [(局型, 人数, 种子, 座位上的名字列表)]。"me" 是你。"""
    rng = random.Random(SEED)
    k = 4 if full else 1
    games = []
    for i in range(2 * k):                     # 1v1：每个对手 2k 张图，每张图换边各一局
        for opp in names:
            games.append(("1v1", 2, SEED + i, ["me", opp]))
            games.append(("1v1", 2, SEED + i, [opp, "me"]))
    for i in range(24 * k):                    # 4 人局：每局随机 3 个对手，你的座位轮换
        seats = rng.sample(names, 3)
        seats.insert(i % 4, "me")
        games.append(("3-4", 4, SEED + 100 + i, seats))
    for i in range(6 * k):                     # 12 人局：13 个对手里随机 11 个，随机排座
        seats = rng.sample(names, 11) + ["me"]
        rng.shuffle(seats)
        games.append(("12", 12, SEED + 200 + i, seats))
    return games


def main():
    ensure_compiler()
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    full = "--full" in sys.argv
    mine = Path(args[0]).resolve() if args else WS / "submission" / "bot.cpp"
    if not mine.is_file():
        raise SystemExit(f"找不到你的程序：{mine}")
    if not (WS / "bots" / "sparring_hard.exe").is_file():
        print("第一次运行：先从源码编译陪练（sparring/ → workspace/bots/）……", flush=True)
        import make_player_pack
        make_player_pack.build_sparring(quiet=True)
    sys.path.insert(0, str(WS))
    sys.dont_write_bytecode = True
    import arena                                # 选手包里的本地工具：编译、调用裁判

    board = json.loads((HERE / "results" / "results.json").read_text(encoding="utf-8"))["boards"]["强对手榜"]["score"]
    table = board["table"]
    names = list(OPPONENTS)
    games = schedule(names, full)
    entries = [arena.Entry(str(mine))] + [arena.Entry(str(HERE / p)) if p else arena.Entry(n) for n, p in OPPONENTS.items()]
    index = {"me": 0, **{n: i + 1 for i, n in enumerate(names)}}
    workers = max(1, min(4, (os.cpu_count() or 2) // 2))
    print(f"你的程序：{mine}\n对手：10 位正式选手 + 2 位参考选手 + 强档陪练线（共 13 个）\n"
          f"共 {len(games)} 局（1v1 {sum(g[0] == '1v1' for g in games)}、4 人局 {sum(g[0] == '3-4' for g in games)}、"
          f"12 人局 {sum(g[0] == '12' for g in games)}），正式模式，并行 {workers}\n"
          f"（第一次运行要编译 15 个程序，稍等一会儿；12 人局每局要几十秒）\n", flush=True)
    start = time.time()
    try:
        recs = arena.run_games(entries, [(n, seed, [index[s] for s in seats]) for _, n, seed, seats in games], True, workers)
    except arena.Fail as e:
        raise SystemExit(f"出错了：{e}")
    print(f"打完了，用时 {time.time() - start:.0f} 秒。\n")

    # 每局里你和每个对手两两比较：活得更久算赢，同一回合死算平（和正式计分一样）
    tally = {m: {n: [0, 0, 0] for n in names} for m in MODES}
    mine_stats = {"timeouts": 0, "invalid": 0, "crash": 0, "first": 0.0, "max": 0.0}
    for (mode, n, seed, seats), r in zip(games, recs):
        key = [10 ** 9 if d < 0 else d for d in r["death_turn"]]
        me = seats.index("me")
        for j, opp in enumerate(seats):
            if j == me:
                continue
            t = tally[mode][opp]
            t[0 if key[me] > key[j] else 1 if key[me] == key[j] else 2] += 1
        mine_stats["timeouts"] += r["timeouts"][me]
        mine_stats["invalid"] += r["invalid"][me]
        mine_stats["crash"] += r["crash_turn"][me] >= 0
        mine_stats["first"] = max(mine_stats["first"], r["first_ms"][me])
        mine_stats["max"] = max(mine_stats["max"], r["max_ms"][me])

    ratings = {}
    for m in MODES:
        rows = [(table[n]["modes"][m]["score"] or 0.0, *tally[m][n]) for n in names if sum(tally[m][n])]
        ratings[m] = fit(rows)
    total = sum(w * ratings[m] for m, w in MODES.items())
    ranked = sorted(((table[n]["total"], n) for n in names if n != "sparring_hard"), reverse=True)
    place = 1 + sum(1 for t, _ in ranked if t > total)
    print(f"按终场强对手榜的尺子（豆包 = 0，强 10 倍 = +400），你的实力值约为：1v1 {ratings['1v1']:+.0f}、"
          f"3～4 人 {ratings['3-4']:+.0f}、12 人 {ratings['12']:+.0f}，加权总分 {total:+.0f}"
          f"（1v1 50%、3～4 人 30%、12 人 20%）。\n放进强对手榜的 12 位里相当于第 {place} 名"
          f"（强档陪练线是 {table['sparring_hard']['total']:+.0f}，只作参考线、不占名次）。")
    print(f"你的 bot：首回合 CPU 最多 {mine_stats['first']:.0f} 毫秒，之后每回合最多 {mine_stats['max']:.1f} 毫秒（预算 50）；"
          f"超时 {mine_stats['timeouts']} 回合，不合法 {mine_stats['invalid']} 次，崩溃 {mine_stats['crash']} 局。")
    print("\n| 对手 | 终场总分 | 1v1 胜-平-负 | 4 人局 胜-平-负 | 12 人局 胜-平-负 |\n|---|---:|---|---|---|")
    for n in names:
        cells = ["-".join(map(str, tally[m][n])) if sum(tally[m][n]) else "—" for m in MODES]
        print(f"| {n} | {table[n]['total']:+.0f} | " + " | ".join(cells) + " |")
    print(f"\n多人局的“胜-平-负”是你和这个对手同桌时谁活得更久。局数少时结果会有起伏，想要更稳就加 --full。"
          f"\n地图种子从 {SEED} 起，和正式赛不是同一批图。正式成绩见 results/RESULTS_正式榜.md、results/RESULTS_强对手榜.md。")


if __name__ == "__main__":
    main()
