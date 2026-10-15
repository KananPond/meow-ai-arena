"""用你的 bot 挑战第 6 集全部 AI：和 14 个模型的下半场版本、出题方参照、基准各打一组正式模式的对局，
再按终场赛的实力值把你放到同一把尺上。

  python challenge.py                      用 workspace/submission/bot.cpp（选手拿到的起步模板）
  python challenge.py 我的bot.cpp          用你自己的程序（每个对手 20 局，共 320 局）
  python challenge.py 我的bot.cpp --full   每个对手 80 局，共 1280 局，结果更稳

你的 bot 和比赛选手一样只写 init 和 decide 两个函数，规则和接口见 workspace/README.md、workspace/RULES.md。
需要 Python 3.10+ 和 g++（Windows 可用 w64devkit，见仓库根目录 README）。
"""
import json
import math
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

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


def main():
    ensure_compiler()
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    full = "--full" in sys.argv
    mine = Path(args[0]).resolve() if args else WS / "submission" / "bot.cpp"
    if not mine.is_file():
        raise SystemExit(f"找不到你的程序：{mine}")
    games = 80 if full else 20
    workers = max(1, min(8, (os.cpu_count() or 2) // 2))
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
        rating = final[name]["rating"]
        rows.append((rating, w, d, l))
        table.append((name, final[name]["display"] or final[name]["label"] or name, rating, w, d, l))
        print(f"  对 {name:<9} {w:>3}-{d}-{l:<3}（胜-平-负）  {time.time() - start:5.1f} 秒  {last.split('故障：')[-1]}", flush=True)
    mine_rating = fit(rows)
    ranked = sorted([(v["rating"], k) for k, v in final.items() if v["ranked"]], reverse=True)
    place = 1 + sum(1 for r, _ in ranked if r > mine_rating)
    print(f"\n按终场赛的尺子（基准 = 0，强 10 倍 = +400），你的实力值约为 {mine_rating:+.0f}，"
          f"放进 14 位 AI 里相当于第 {place} 名。")
    print("\n| 对手 | 终场赛实力值 | 你的胜-平-负 | 得分率 |\n|---|---:|---|---:|")
    for name, shown, rating, w, d, l in table:
        print(f"| {shown}（{name}） | {rating:+.0f} | {w}-{d}-{l} | {(w + 0.5 * d) / (w + d + l):.1%} |")
    print(f"\n局数少时结果会有起伏，想要更稳就加 --full。地图种子从 1001 起，和正式赛不是同一批图。"
          f"\n正式赛（14400 局）的成绩见 results/final/成绩.md。")


if __name__ == "__main__":
    main()
