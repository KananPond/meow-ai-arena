"""用你的 bot 挑战第 6 集全部 AI：和 14 个模型的下半场版本、出题方参照、基准各打一组正式模式的对局，
再按终场赛的实力值把你放到同一把尺上。

  python challenge.py                      用 workspace/submission/bot.cpp（选手拿到的起步模板）
  python challenge.py 我的bot.cpp          用你自己的程序（每个对手 20 局，共 320 局）
  python challenge.py 我的bot.cpp --full   每个对手 80 局，共 1280 局，结果更稳

你的 bot 和比赛选手一样只写 init 和 decide 两个函数，规则和接口见 workspace/README.md、workspace/RULES.md。
需要 Python 3.10+ 和 g++（Windows 可用 w64devkit，见仓库根目录 README）。
"""
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
import tempfile
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
HERE = Path(__file__).resolve().parent
WS = HERE / "workspace"
# 名字：终场赛成绩表里的短名 → 程序
OPPONENTS = {
    "Astra": "bots/half2/gpt-6-astra.cpp", "SolUF": "bots/half2/gpt-6.1-sol-ultrafast.cpp",
    "Gemini": "bots/half2/gemini-4-argon.cpp", "Sol": "bots/half2/gpt-6.1-sol.cpp", "Opus": "bots/half2/opus-5.5.cpp",
    "Muse": "bots/half2/muse-wonder-full.cpp", "MiMo": "bots/half2/mimo-v2.6-pro.cpp", "Grok": "bots/half2/grok-4.7.cpp",
    "DeepSeek": "bots/half2/deepseek-v4.1-flash.cpp", "Doubao": "bots/half2/doubao-seed-evolving.cpp",
    "Kimi": "bots/half2/kimi-k3.cpp", "Haiku": "bots/half2/haiku-5.5.cpp", "Qwen": "bots/half2/qwen-3.8-max.cpp",
    "GLM": "bots/half2/glm-5.3.cpp", "sparring": "tournament/sparring.cpp", "baseline": "workspace/baseline.cpp",
}
SUMMARY = re.compile(r"汇总：.*?(\d+)-(\d+)-(\d+)（胜-平-负）")


def fit(rows):
    """对手的实力值固定，只解你一个人的实力值（Bradley–Terry，强 10 倍 = +400，加一局虚拟平局防止发散）。"""
    def loglik(r):
        total = 0.0
        for opp, w, d, l in rows:
            p = 1 / (1 + 10 ** ((opp - r) / 400))
            n, s = w + d + l + 1, w + 0.5 * d + 0.5
            total += s * math.log(p) + (n - s) * math.log(1 - p)
        return total
    lo, hi = -1500.0, 2500.0
    for _ in range(200):                       # 似然是单峰的，三分法就够
        a, b = lo + (hi - lo) / 3, hi - (hi - lo) / 3
        if loglik(a) < loglik(b):
            lo = a
        else:
            hi = b
    return (lo + hi) / 2


def ensure_compiler():
    """选手包的 core.py 只看环境变量 CXX 和 PATH。没找到时，按仓库根目录 README 的约定再找一下 w64devkit。"""
    if os.environ.get("CXX") or shutil.which("g++") or shutil.which("clang++"):
        return
    for guess in (HERE.parent / "tools" / "w64devkit" / "bin" / "g++.exe", Path("C:/w64devkit/bin/g++.exe")):
        if guess.is_file():
            os.environ["CXX"] = str(guess)
            return
    raise SystemExit("没找到 C++ 编译器（g++）。Windows 推荐 w64devkit，放法见仓库根目录 README。")


def budget_info():
    """从官方裁判读取实际预算，不复制或修改游戏规则。"""
    sys.path.insert(0, str(WS))
    import engine
    try:
        return engine.info()
    finally:
        engine.shutdown()


def render_report(doc):
    lines = ["# 坦克大战挑战成绩", "",
             f"你的实力值：**{doc['rating']:+.1f}**；放进 14 位 AI 里相当于第 **{doc['rank']} / 15** 名。", "",
             "固定终场赛对手实力值，只拟合投稿的 Bradley–Terry 实力值（基准 = 0，强 10 倍 = +400）。",
             "这是新地图上的少量挑战对局，不是重跑终场赛，也不是官方选手互相重打的小联赛。", "",
             f"正式模式；共 {doc['games']} 局；地图种子从 {doc['seed']} 起，每张图换边。",
             f"预算：{doc['budget']['first_ms']} / {doc['budget']['turn_ms']} 毫秒（{doc['budget']['budget_kind']}，首回合 / 之后）。", "",
             "| 对手 | 终场赛实力值（固定） | 你的胜-平-负 | 得分率 |", "|---|---:|---|---:|"]
    for row in doc["opponents"]:
        w, d, l = (row[k] for k in ("wins", "draws", "losses"))
        lines.append(f"| {row['display']}（{row['name']}） | {row['rating']:+.0f} | {w}-{d}-{l} | {(w + 0.5 * d) / (w + d + l):.1%} |")
    lines += ["", "局数少时结果会有起伏，想要更稳就加 --full。正式赛（14400 局）的成绩见 tank/results/final/成绩.md。", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bot", nargs="?", help="bot.cpp；默认使用选手包模板")
    ap.add_argument("--full", action="store_true", help="每个对手 80 局（默认 20 局）")
    ap.add_argument("--out", help="结果目录；默认新建 tank/challenge-runs/ 下的目录")
    ap.add_argument("--require-cpu", action="store_true", help="社区评测：CPU 计时不可用时拒绝出成绩")
    args = ap.parse_args()
    ensure_compiler()
    mine = Path(args.bot).resolve() if args.bot else WS / "submission" / "bot.cpp"
    if not mine.is_file():
        raise SystemExit(f"找不到你的程序：{mine}")
    games = 80 if args.full else 20
    workers = max(1, min(8, (os.cpu_count() or 2) // 2))
    budget = budget_info()
    if args.require_cpu and (budget["budget_kind"] != "cpu" or
                             (budget["first_ms"], budget["turn_ms"]) != (2000, 50)):
        raise SystemExit("社区坦克评测必须使用官方的 2000 / 50 CPU 毫秒预算；请勿设置 TANK_BUDGET=wall。")
    if args.out:
        run_dir = Path(args.out).resolve()
        run_dir.mkdir(parents=True, exist_ok=True)
        if any((run_dir / name).exists() for name in ("result.json", "report.md")):
            raise SystemExit(f"结果目录已有成绩，请换一个新目录：{run_dir}")
    else:
        runs = HERE / "challenge-runs"
        runs.mkdir(exist_ok=True)
        run_dir = Path(tempfile.mkdtemp(prefix=time.strftime("%Y%m%d-%H%M%S-"), dir=runs))
    source_sha = hashlib.sha256(mine.read_bytes()).hexdigest()
    final = json.loads((HERE / "results" / "final" / "成绩.json").read_text(encoding="utf-8"))["players"]
    print(f"你的程序：{mine}\n对手：14 个 AI 的下半场版本 + 出题方参照 + 基准，每个 {games} 局（正式模式，并行 {workers}）\n"
          f"（第一次运行要编译 17 个程序，稍等一会儿）\n", flush=True)
    rows, table = [], []
    for name, rel in OPPONENTS.items():
        cmd = [sys.executable, "-X", "utf8", "arena.py", "batch", str(mine), str(HERE / rel),
               "--games", str(games), "--seed", "1001", "--workers", str(workers)]
        start = time.time()
        out = subprocess.run(cmd, cwd=WS, capture_output=True, text=True, encoding="utf-8", errors="replace")
        last = (out.stdout.strip().splitlines() or [""])[-1]
        m = SUMMARY.search(last)
        if out.returncode or not m:
            print(out.stdout[-2000:], out.stderr[-2000:], sep="\n")
            raise SystemExit(f"和 {name} 的对局出错了，请看上面的输出。")
        w, d, l = map(int, m.groups())
        if w + d + l != games:
            raise SystemExit(f"和 {name} 只完成了 {w + d + l} / {games} 局，不计算不完整的成绩。")
        rating = final[name]["rating"]
        rows.append((rating, w, d, l))
        table.append({"name": name, "display": final[name]["display"] or final[name]["label"] or name,
                      "rating": rating, "ranked": final[name]["ranked"], "wins": w, "draws": d, "losses": l})
        print(f"  对 {name:<9} {w:>3}-{d}-{l:<3}（胜-平-负）  {time.time() - start:5.1f} 秒  {last.split('故障：')[-1]}", flush=True)
    if hashlib.sha256(mine.read_bytes()).hexdigest() != source_sha:
        raise SystemExit("评测途中 bot 源码发生了变化，不计算成绩。")
    mine_rating = fit(rows)
    place = 1 + sum(1 for v in final.values() if v["ranked"] and v["rating"] > mine_rating)
    doc = {"schema": 1, "complete": True, "rating_method": "fixed-final", "mode": "formal", "budget": budget,
           "source_sha256": source_sha, "games_per_opponent": games, "games": games * len(table), "seed": 1001,
           "rating": mine_rating, "rank": place, "opponents": table}
    report = render_report(doc)
    (run_dir / "result.json").write_text(json.dumps(doc, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    (run_dir / "report.md").write_text(report, encoding="utf-8")
    print("\n" + report)
    print(f"结果文件：{run_dir / 'result.json'}\n成绩表：{run_dir / 'report.md'}")


if __name__ == "__main__":
    main()
