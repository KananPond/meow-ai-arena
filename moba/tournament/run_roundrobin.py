"""示例赛：5 个示例 bot 两两对阵的循环赛，覆盖三种难度，产出成绩表和精选回放。

  python run_roundrobin.py [--games-per-pair N] [--workers W] [--replays]

每个对阵在每档难度打 2 局（换边），可再加局数（--games-per-pair 大于 2 时按种子扩展，
座位轮换）。成绩按胜负场次排（平局算半分），写入 ../results/示例赛-成绩.md；
--replays 会把普通难度的每一局都在正式对局里顺便存成回放到 ../replays/，
文件名带种子，换边的两局不会互相覆盖。
"""
import argparse
import json
import sys
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "workspace"))
import core  # noqa: E402
from engine import VERSION, Game, DIFF_LABELS  # noqa: E402

BOTS = {
    "速推": "../bots/rush.cpp",
    "发育": "../bots/farm.cpp",
    "游走": "../bots/gank.cpp",
    "抱团": "../bots/siege.cpp",
    "均衡": "../bots/balance.cpp",
}


def _one(job):
    """跑一局。给了 stem 就顺便把这一局的回放写到磁盘（回放和成绩来自同一次对局）。"""
    programs, names, seed, difficulty, stem, rep_dir, viewer = job
    result, replay = core.run_match(Game, programs, names, seed, record=stem is not None,
                                    difficulty=difficulty)
    if stem is not None:
        core.save_replay(replay, Path(rep_dir) / stem, Path(viewer))
    return names, result


def esc_h(t):
    return str(t).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--games-per-pair", type=int, default=2, help="每个对阵每档难度打的局数（≥2，含换边）")
    ap.add_argument("--workers", type=int, default=0)
    ap.add_argument("--replays", action="store_true", help="把普通难度的每场都存成回放（文件名带种子）")
    ap.add_argument("--seed", type=int, default=101)
    a = ap.parse_args()

    root = HERE.parent
    bots = {n: str((HERE / p).resolve()) for n, p in BOTS.items()}
    missing = [n for n, p in bots.items() if not Path(p).is_file()]
    if missing:
        raise SystemExit(f"示例 bot 缺失：{missing}（先让它们交卷）")
    names = sorted(bots)
    programs = {n: core.command_for(Path(p), root / "workspace" / "build") for n, p in bots.items()}

    rep_dir = root / "replays"
    viewer = root / "workspace" / "viewer.html"
    if a.replays:
        rep_dir.mkdir(exist_ok=True)

    jobs, plan = [], []
    k = a.seed
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            for diff in (0, 1, 2):
                per = max(2, a.games_per_pair)
                for g in range(per):
                    home = g % 2 == 0
                    seat = [names[i], names[j]] if home else [names[j], names[i]]
                    # 回放文件名带种子和座位顺序：同一对阵换边打的两局是两份不同的文件
                    stem = f"示例-{seat[0]}-vs-{seat[1]}-seed{k}" if a.replays and diff == 1 else None
                    jobs.append(([programs[seat[0]], programs[seat[1]]], seat, k, diff, stem,
                                 str(rep_dir), str(viewer)))
                    plan.append({"a": seat[0], "b": seat[1], "difficulty": diff, "seed": k,
                                 "home_is_a": home, "replay": stem})
                    k += 1
    workers = a.workers or max(1, min(8, (__import__("os").cpu_count() or 4) // 2))
    print(f"{VERSION} 示例赛：{len(names)} 个 bot 循环赛，{len(jobs)} 局，并行 {workers}")
    t0 = time.time()
    results = []
    with ProcessPoolExecutor(workers) as pool:
        for pl, (seat, res) in zip(plan, pool.map(_one, jobs)):
            results.append({**pl, "winner": res["winner"], "reason": res["reason"], "turns": res["turns"],
                            "score": res["score"], "stats": res["stats"]})
            w = "平" if res["winner"] < 0 else seat[res["winner"]]
            print(f"  {pl['a']} vs {pl['b']}  {DIFF_LABELS[pl['difficulty']]}  种子 {pl['seed']}  →  {w}"
                  f"（{res['turns']} 回合）")
    print(f"打完用时 {time.time() - t0:.0f}s")

    # 汇总
    wins = {n: 0.0 for n in names}
    breakdown = {n: {d: 0.0 for d in range(3)} for n in names}
    matrix = {n: {m: 0.0 for m in names} for n in names}
    for r in results:
        if r["winner"] < 0:
            for n in (r["a"], r["b"]):
                wins[n] += 0.5
                breakdown[n][r["difficulty"]] += 0.5
                matrix[n][r["b"] if n == r["a"] else r["a"]] += 0.5
        else:
            w = r["a"] if r["winner"] == 0 else r["b"]
            l = r["b"] if r["winner"] == 0 else r["a"]
            wins[w] += 1
            breakdown[w][r["difficulty"]] += 1
            matrix[w][l] += 1

    out = root / "results"
    out.mkdir(exist_ok=True)
    games_path = out / "示例赛-对局.json"
    games_path.write_text(json.dumps(results, ensure_ascii=False, indent=1), encoding="utf-8")

    lines = [f"# 峡谷对决 · 示例赛成绩（{VERSION}）", "",
             f"{len(names)} 个示例 bot 循环赛，每个对阵在每档难度换边各打一场，共 {len(results)} 局。",
             "胜 1 分、平 0.5 分。这是**主办方代写的示例 bot 之间的表演赛**，用来演示赛制，不计入正式成绩。", "",
             "## 总榜", "", "| 名次 | bot | 胜-平-负 | 积分 | 简单 | 普通 | 困难 |",
             "|---:|---|---|---:|---:|---:|---:|"]
    order = sorted(names, key=lambda n: -wins[n])
    games_per_diff = max(2, a.games_per_pair) * (len(names) - 1)
    for rank, n in enumerate(order, 1):
        d = breakdown[n]
        others = [m for m in names if m != n]
        win = sum(1 for r in results if r["winner"] >= 0 and (r["a"] if r["winner"] == 0 else r["b"]) == n)
        draw = sum(1 for r in results if r["winner"] < 0 and n in (r["a"], r["b"]))
        lose = sum(1 for r in results if r["winner"] >= 0 and (r["b"] if r["winner"] == 0 else r["a"]) == n)
        lines.append(f"| {rank} | {n} | {win}-{draw}-{lose} | {wins[n]:g} | "
                     f"{d[0]:g}/{games_per_diff} | {d[1]:g}/{games_per_diff} | {d[2]:g}/{games_per_diff} |")
    lines += ["", "## 两两交锋（行对列的胜场）", "", "| | " + " | ".join(names) + " |",
              "|---|" + "---:|" * len(names)]
    for n in names:
        rows = []
        for m in names:
            rows.append("—" if m == n else f"{matrix[n][m]:g}")
        lines.append(f"| {n} | " + " | ".join(rows) + " |")
    (out / "示例赛-成绩.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"成绩表：{out / '示例赛-成绩.md'}")

    if a.replays:
        # 回放已经在正式对局里一起落盘了，这里只按成绩生成索引页
        items = []
        for r in results:
            if not r.get("replay"):
                continue
            w = "平局" if r["winner"] < 0 else (r["a"] if r["winner"] == 0 else r["b"]) + " 胜"
            items.append((r["replay"], f"{r['a']} vs {r['b']}（{w}，{r['turns']} 回合，种子 {r['seed']}）"))
        links = "\n".join(f'<option value="{s}.html">{esc_h(t)}</option>' for s, t in items)
        (rep_dir / "index.html").write_text(
            """<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><title>峡谷对决 · 示例赛回放</title>
<style>body{font-family:"Segoe UI","Microsoft YaHei",sans-serif;background:#0d1219;color:#e6ecf5;display:grid;place-items:center;min-height:90vh;margin:0}
main{background:#161e2a;border:1px solid #2a3547;border-radius:12px;padding:28px 34px;max-width:560px}
h1{font-size:20px;margin:0 0 6px}p{color:#93a1b5;font-size:13px;margin:0 0 18px}
select,button{background:#243149;color:#e6ecf5;border:1px solid #3c4d68;border-radius:8px;padding:9px 12px;font:inherit}
select{width:100%}button{margin-top:12px;cursor:pointer;width:100%}</style></head><body><main>
<h1>峡谷对决 · 示例赛回放</h1>
<p>5 个示例 bot 循环赛（普通难度）的每一局。文件名带种子，换边的两局分开存放。回放里所有图形都是 SVG 现场画的。</p>
<select id="pick">__LINKS__</select>
<button onclick="location.href=document.getElementById('pick').value">打开这份回放</button>
</main></body></html>""".replace("__LINKS__", links),
            encoding="utf-8",
        )
        print(f"回放：{len(items)} 份普通难度回放（文件名带种子，互不覆盖）")
        print(f"索引页：{rep_dir / 'index.html'}")


if __name__ == "__main__":
    main()
