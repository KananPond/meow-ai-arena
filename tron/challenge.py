"""用你的 bot 挑战第 7 集光轮对战的全部 AI：和 10 位正式选手、2 位参考选手、强档陪练线一起打 1v1、4 人局、12 人局
（正式模式，和比赛时一样每个 bot 一个进程、超预算判罚），再按终场强对手榜的实力值把你放到同一把尺上。

  python challenge.py                      用 workspace/submission/bot.cpp（选手拿到的起步模板，能直走就直走）
  python challenge.py 我的bot.cpp          用你自己的程序（1v1 52 局、4 人局 24 局、12 人局 6 局，几分钟）
  python challenge.py 我的bot.cpp --full   局数是上面的 4 倍，结果更稳

你的 bot 和比赛选手一样只写 decide 一个函数，规则和接口见 workspace/README.md、workspace/RULES.md。
需要 Python 3.10+ 和 g++（Windows 可用 w64devkit，见仓库根目录 README）。裁判只支持 Windows。
"""
import argparse
import hashlib
import json
import math
import os
import random
import re
import shutil
import sys
import tempfile
import time
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
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



def require_windows():
    if os.name != "nt":
        raise SystemExit("光轮正式裁判只支持 Windows；社区评测使用标准 Windows runner，不使用快速模式或放宽预算。")


def budget_info():
    """记录可信引擎的正式 CPU 预算，不改动引擎或裁判。"""
    header = (WS / "tron.hpp").read_text(encoding="utf-8")
    values = {}
    for field, constant in (("first_ms", "BUDGET_FIRST_MS"), ("turn_ms", "BUDGET_TURN_MS")):
        match = re.search(r"inline\s+constexpr\s+int\s+" + constant + r"\s*=\s*(\d+)\s*;", header)
        if not match:
            raise SystemExit("读不懂官方光轮 CPU 预算，不生成成绩。")
        values[field] = int(match.group(1))
    return {"budget_kind": "cpu", **values}


def load_arena():
    sys.path.insert(0, str(WS))
    sys.dont_write_bytecode = True
    import arena                                # 可信选手包：编译、调用原生裁判
    return arena


def validate_records(games, recs):
    """不把截断、缺座位或无效的结果当作完整评测。"""
    if not isinstance(recs, list) or len(recs) != len(games):
        raise ValueError("对局没有全部完成")
    index = {"me": 0, **{name: i + 1 for i, name in enumerate(OPPONENTS)}}
    for i, ((mode, n, seed, seats), rec) in enumerate(zip(games, recs)):
        if not isinstance(rec, dict) or type(rec.get("id")) is not int or rec["id"] != i:
            raise ValueError("对局编号缺失或不一致")
        if (type(rec.get("n")) is not int or rec["n"] != n or
                type(rec.get("seed")) is not int or rec["seed"] != seed or
                not isinstance(rec.get("bots"), list) or
                any(type(v) is not int for v in rec["bots"]) or
                rec["bots"] != [index[name] for name in seats]):
            raise ValueError("对局人数、地图种子或座位与赛程不一致")
        turns = rec.get("turns")
        if type(turns) is not int or not 1 <= turns <= {2: 441, 4: 625, 12: 1681}[n]:
            raise ValueError("对局回合数无效")
        for key in ("death_turn", "timeouts", "invalid", "crash_turn", "first_ms", "max_ms"):
            row = rec.get(key)
            if not isinstance(row, list) or len(row) != n:
                raise ValueError("对局缺少每个座位的 " + key)
            if key in ("first_ms", "max_ms"):
                valid = all(isinstance(v, (int, float)) and not isinstance(v, bool)
                            and math.isfinite(v) and v >= 0 for v in row)
            else:
                lower = -1 if key in ("death_turn", "crash_turn") else 0
                valid = all(type(v) is int and v >= lower for v in row)
            if not valid:
                raise ValueError("对局含无效的 " + key)
        if (sum(v == -1 for v in rec["death_turn"]) > 1 or
                any(v >= turns for v in rec["death_turn"] + rec["crash_turn"]) or
                any(v > turns for v in rec["timeouts"] + rec["invalid"])):
            raise ValueError("对局未终局或统计超出已完成回合")


def make_result(games, recs, table, source_sha, full, budget):
    validate_records(games, recs)
    names = list(OPPONENTS)
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
    opponents = [{"name": n, "ranked": n != "sparring_hard", "rating": table[n]["total"],
                  "ratings": {m: table[n]["modes"][m]["score"] or 0.0 for m in MODES},
                  "tally": {m: tally[m][n] for m in MODES}} for n in names]
    return {"schema": 1, "complete": True, "rating_method": "fixed-final-strong", "mode": "formal",
            "budget": budget, "reference": {"path": "tron/results/results.json", "board": "强对手榜", "anchor": "Doubao"},
            "source_sha256": source_sha, "seed": SEED, "full": full, "games": len(games),
            "games_by_mode": {m: sum(g[0] == m for g in games) for m in MODES},
            "weights": dict(MODES), "ratings": ratings, "rating": total, "rank": place,
            "opponents": opponents, "stats": mine_stats}


def render_report(doc):
    ratings, stats = doc["ratings"], doc["stats"]
    hard = next(row for row in doc["opponents"] if row["name"] == "sparring_hard")
    lines = ["# 光轮对战挑战成绩", "",
             f"固定终场强对手榜参照（豆包 = 0，强 10 倍 = +400），只拟合你的实力值：",
             f"1v1 **{ratings['1v1']:+.1f}**、3～4 人 **{ratings['3-4']:+.1f}**、12 人 **{ratings['12']:+.1f}**；"
             f"加权总分 **{doc['rating']:+.1f}**（1v1 50%、3～4 人 30%、12 人 20%）。", "",
             f"放进强对手榜的 12 位里相当于第 **{doc['rank']} / 13** 名。"
             f"强档陪练线为 {hard['rating']:+.0f}，参加对战和拟合，只作参考线、不占名次。", "",
             f"正式模式，共 {doc['games']} 局（1v1 {doc['games_by_mode']['1v1']}、4 人局 {doc['games_by_mode']['3-4']}、"
             f"12 人局 {doc['games_by_mode']['12']}），地图种子从 {doc['seed']} 起。",
             f"官方 CPU 预算：首回合 {doc['budget']['first_ms']} 毫秒，之后每回合 {doc['budget']['turn_ms']} 毫秒。",
             f"你的 bot：首回合 CPU 最多 {stats['first']:.0f} 毫秒，之后每回合最多 {stats['max']:.1f} 毫秒；"
             f"超时 {stats['timeouts']} 回合，不合法 {stats['invalid']} 次，崩溃 {stats['crash']} 局。", "",
             "| 对手 | 终场总分（固定） | 1v1 胜-平-负 | 4 人局 胜-平-负 | 12 人局 胜-平-负 |",
             "|---|---:|---|---|---|"]
    for row in doc["opponents"]:
        cells = ["-".join(map(str, row["tally"][m])) if sum(row["tally"][m]) else "—" for m in MODES]
        lines.append(f"| {row['name']} | {row['rating']:+.0f} | " + " | ".join(cells) + " |")
    lines += ["", "多人局的“胜-平-负”是你和这个对手同桌时谁活得更久。对手分数是固定的历史终场赛参照，"
              "不是本次重新测出的官方成绩。局数少，机器和地图也不同，这个估计不能当作正式赛名次；更稳可加 --full。",
              "正式成绩见 tron/results/RESULTS_正式榜.md、tron/results/RESULTS_强对手榜.md。", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bot", nargs="?", help="bot.cpp；默认使用选手包模板")
    ap.add_argument("--full", action="store_true", help="局数是默认的 4 倍（328 局）")
    ap.add_argument("--out", help="结果目录；默认新建 tron/challenge-runs/ 下的目录")
    ap.add_argument("--require-cpu", action="store_true", help="社区评测：要求官方 2000 / 50 CPU 毫秒预算")
    args = ap.parse_args()
    require_windows()
    ensure_compiler()
    full = args.full
    mine = Path(args.bot).resolve() if args.bot else WS / "submission" / "bot.cpp"
    if not mine.is_file():
        raise SystemExit(f"找不到你的程序：{mine}")
    budget = budget_info()
    if args.require_cpu and (budget["budget_kind"] != "cpu" or (budget["first_ms"], budget["turn_ms"]) != (2000, 50)):
        raise SystemExit("社区光轮评测必须使用官方的 2000 / 50 CPU 毫秒预算。")
    if args.out:
        run_dir = Path(args.out).resolve()
        run_dir.mkdir(parents=True, exist_ok=True)
        if any(os.path.lexists(run_dir / name) for name in ("result.json", "report.md")):
            raise SystemExit(f"结果目录已有成绩，请换一个新目录：{run_dir}")
    else:
        runs = HERE / "challenge-runs"
        runs.mkdir(exist_ok=True)
        run_dir = Path(tempfile.mkdtemp(prefix=time.strftime("%Y%m%d-%H%M%S-"), dir=runs))
    source_sha = hashlib.sha256(mine.read_bytes()).hexdigest()
    if not (WS / "bots" / "sparring_hard.exe").is_file():
        print("第一次运行：先从源码编译陪练（sparring/ → workspace/bots/）……", flush=True)
        import make_player_pack
        make_player_pack.build_sparring(quiet=True)
    arena = load_arena()
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
    if hashlib.sha256(mine.read_bytes()).hexdigest() != source_sha:
        raise SystemExit("评测途中 bot 源码发生了变化，不生成成绩。")
    try:
        doc = make_result(games, recs, table, source_sha, full, budget)
    except (ValueError, TypeError, KeyError, IndexError, OverflowError) as e:
        raise SystemExit(f"对局结果不完整或无效，不生成成绩：{e}")
    report = render_report(doc)
    (run_dir / "report.md").write_text(report, encoding="utf-8")
    (run_dir / "result.json").write_text(json.dumps(doc, ensure_ascii=False, indent=1, allow_nan=False) + "\n", encoding="utf-8")
    print(report)
    print(f"结果文件：{run_dir / 'result.json'}\n成绩表：{run_dir / 'report.md'}")


if __name__ == "__main__":
    main()
