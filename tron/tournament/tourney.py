"""正式赛：赛程、统一编译、跑对局、计分、成绩表、回放、视频统计、估时。

对局本身由选手包里的裁判程序（workspace/judge/judge.cpp，正式模式）完成，规则只在 tron.hpp 里；
这里只负责排赛程、按并发和绑核把对局交给裁判、收结果、算分。编译、名次分都直接用选手包里的 arena.py，
和选手本地完全相同。

赛程（赛事配置的 schedule，默认值见 hostlib.SCHEDULE_DEFAULTS）：
  1v1       全体参赛者（选手 + greedy）单循环，每对在同一组 maps 张图上各换边打一局
  4p / 3p   每轮把参赛者随机分成 4 人桌 / 3 人桌，每桌一张新图、座位轮换 n 局；人数除不尽时轮空的人按轮空次数最少优先
  12p       每张图随机排座后全轮换 12 局；参赛者不是 12 位时循环补满 / 轮流坐（只在演练里出现）
  sparring  参考表：每位参赛者 1v1 对陪练，用 1v1 的前 maps 张图换边；不计分
"""
from __future__ import annotations

import json
import math
import os
import random
import secrets
import shutil
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

import hostlib as H

TYPES = ("1v1", "3p", "4p", "12p", "sparring")
SIZE = {"1v1": 2, "3p": 3, "4p": 4, "12p": 12, "sparring": 2}
MODE_OF = {"1v1": "1v1", "3p": "3-4", "4p": "3-4", "12p": "12"}       # 局型
MODES = ("1v1", "3-4", "12")
MODE_TEXT = {"1v1": "1v1", "3-4": "3～4 人", "12": "12 人"}
CAUSE_ZH = {"alive": "活到最后", "out": "出界", "obstacle": "撞障碍", "self": "撞自己", "trail": "撞别人轨迹",
            "headon": "抢格失败"}
DIRS = {"U": (0, -1), "R": (1, 0), "D": (0, 1), "L": (-1, 0)}
SCHEDULE_FILE, RESULTS_FILE, MANIFEST_FILE = "赛程.json", "results.jsonl", "清单.json"


# ====================================================================
# 处理器：选哪些核、绑核、作业对象
# ====================================================================

def cpu_topology():
    """[{"core": 物理核序号, "class": 效率等级, "cpus": [逻辑处理器]}]。抄自第 6 集 平台/core.py。"""
    if os.name != "nt":
        return []
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.GetLogicalProcessorInformationEx.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(wintypes.DWORD)]
    size = wintypes.DWORD(0)
    k.GetLogicalProcessorInformationEx(0, None, ctypes.byref(size))
    buf = (ctypes.c_ubyte * size.value)()
    if not k.GetLogicalProcessorInformationEx(0, buf, ctypes.byref(size)):
        return []
    raw, out, at = bytes(buf), [], 0
    while at < size.value:
        length = int.from_bytes(raw[at + 4:at + 8], "little")
        eff = raw[at + 9]
        groups = int.from_bytes(raw[at + 30:at + 32], "little")
        cpus = []
        for g in range(groups):
            base = at + 32 + 16 * g
            mask = int.from_bytes(raw[base:base + 8], "little")
            group = int.from_bytes(raw[base + 8:base + 10], "little")
            cpus += [64 * group + b for b in range(64) if mask >> b & 1]
        out.append({"core": len(out), "class": eff, "cpus": cpus})
        at += length
    return out


def pick_cpus(cfg, key="cpus"):
    """bot 用的逻辑处理器。auto：性能最高那类核，每个物理核取一个（另一个超线程空着），最多 max_cpus 个。"""
    want = cfg.get(key)
    if key != "cpus" and not want:
        return pick_cpus(cfg)
    if isinstance(want, list):
        return sorted(int(c) for c in want)
    topo = cpu_topology()
    if not topo:
        return list(range(min(cfg["max_cpus"], os.cpu_count() or 1)))
    top = max(c["class"] for c in topo)
    return [c["cpus"][0] for c in topo if c["class"] == top][: cfg["max_cpus"]]


def workers_for(n, cpus):
    """同时打几局：一局 n 个 bot，同时运行的 bot 不超过处理器数；n 比处理器多时只打一局（超卖，见说明）。"""
    return max(1, len(cpus) // n)


_job = None


def _kill_on_close_job():
    """本进程的作业对象：本进程不论怎么结束，裁判进程都跟着结束（裁判自己的作业对象再管 bot）。"""
    global _job
    if _job is not None or os.name != "nt":
        return _job
    import ctypes
    from ctypes import wintypes

    class BASIC(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64), ("PerJobUserTimeLimit", ctypes.c_int64),
                    ("LimitFlags", wintypes.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t), ("PriorityClass", wintypes.DWORD), ("SchedulingClass", wintypes.DWORD)]

    class IO(ctypes.Structure):
        _fields_ = [(n, ctypes.c_uint64) for n in ("a", "b", "c", "d", "e", "f")]

    class EXT(ctypes.Structure):
        _fields_ = [("Basic", BASIC), ("Io", IO), ("ProcessMemoryLimit", ctypes.c_size_t),
                    ("JobMemoryLimit", ctypes.c_size_t), ("PeakProcessMemoryUsed", ctypes.c_size_t),
                    ("PeakJobMemoryUsed", ctypes.c_size_t)]

    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateJobObjectW.restype = wintypes.HANDLE
    k.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    job = k.CreateJobObjectW(None, None)
    info = EXT()
    info.Basic.LimitFlags = 0x2000        # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
    k.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info))
    _job = (k, job)
    return _job


def adopt(proc, cpus):
    """把裁判进程放进本进程的作业对象，并绑到 cpus 上（它之后起的 bot 进程继承这个绑核）。"""
    if os.name != "nt":
        return
    import ctypes
    from ctypes import wintypes
    k, job = _kill_on_close_job()
    k.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    k.AssignProcessToJobObject(job, int(proc._handle))
    if cpus and max(cpus) < 64:
        mask = 0
        for c in cpus:
            mask |= 1 << c
        k.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
        if not k.SetProcessAffinityMask(int(proc._handle), mask):
            raise H.Stop(f"绑核失败（处理器 {cpus}，错误码 {ctypes.get_last_error()}）")


def power_scheme():
    try:
        r = subprocess.run(["powercfg", "/getactivescheme"], capture_output=True, text=True, timeout=10,
                           encoding="mbcs" if os.name == "nt" else "utf-8", errors="replace")
        return r.stdout.strip()
    except Exception as exc:     # noqa: BLE001
        return f"读不到（{exc}）"


# ====================================================================
# 赛程
# ====================================================================

def _deal(entries, n, rng, sat):
    """一轮分桌：P 人分成 P // n 张 n 人桌；除不尽时轮空次数最少的人优先轮空（同样少的随机）。P < n 时一桌、循环补满。"""
    P = len(entries)
    if P < n:
        perm = rng.sample(entries, P)
        return [[perm[i % P] for i in range(n)]], []
    k = P // n
    out_n = P - k * n
    order = sorted(entries, key=lambda e: (sat[e], rng.random()))
    resting = order[:out_n] if out_n else []
    playing = [e for e in entries if e not in resting]
    rng.shuffle(playing)
    for e in resting:
        sat[e] += 1
    return [playing[i * n:(i + 1) * n] for i in range(k)], resting


def make_schedule(entries, sched, sparring):
    """生成赛程。entries：计分的参赛者（选手 + greedy）；sparring：陪练的名字（只进参考表）。

    返回 {"maps": {...}, "games": [...], "rest": {...}}。每局：gid、type、n、seed、seats（各座位的名字）、
    map（图的序号）、round、table、rot、block（自助法重抽样的单位）。"""
    rng = random.Random(f"tables-{sched['table_seed']}")
    if sched.get("map_seed") is None:
        seed_rng = secrets.SystemRandom()
    else:
        seed_rng = random.Random(f"maps-{sched['map_seed']}")
    used = set()

    def new_seed():
        while True:
            s = seed_rng.randrange(1_000_000, 2 ** 31)
            if s not in used:
                used.add(s)
                return s

    games, maps, rest = [], {}, {}

    def add(kind, seed, seats, **meta):
        games.append({"gid": len(games), "type": kind, "n": len(seats), "seed": seed, "seats": list(seats), **meta})

    # 1v1：同一组图，每对换边
    m1 = int(sched["1v1"]["maps"])
    maps["1v1"] = [new_seed() for _ in range(m1)]
    for m, seed in enumerate(maps["1v1"]):
        for i, a in enumerate(entries):
            for b in entries[i + 1:]:
                for side, seats in enumerate(([a, b], [b, a])):
                    add("1v1", seed, seats, map=m, rot=side, block=f"1v1-m{m}-{a}-{b}")
    # 4 人桌、3 人桌
    for kind, n in (("4p", 4), ("3p", 3)):
        maps[kind] = []
        sat = {e: 0 for e in entries}
        rest[kind] = []
        for r in range(int(sched[kind]["rounds"])):
            tables, resting = _deal(entries, n, rng, sat)
            rest[kind].append(resting)
            for t, table in enumerate(tables):
                seed = new_seed()
                maps[kind].append(seed)
                for rot in range(n):
                    add(kind, seed, [table[(k + rot) % n] for k in range(n)], map=len(maps[kind]) - 1, round=r,
                        table=t, rot=rot, block=f"{kind}-r{r}-t{t}")
    # 12 人局：每张图随机排座、全轮换
    maps["12p"] = []
    count = {e: 0 for e in entries}
    for m in range(int(sched["12p"]["maps"])):
        seed = new_seed()
        maps["12p"].append(seed)
        if len(entries) >= 12:
            pick = sorted(entries, key=lambda e: (count[e], rng.random()))[:12]
            rng.shuffle(pick)
        else:
            perm = rng.sample(entries, len(entries))
            pick = [perm[i % len(perm)] for i in range(12)]
        for e in pick:
            count[e] += 1
        for rot in range(12):
            seats = [pick[(k + rot) % 12] for k in range(12)]
            add("12p", seed, seats, map=m, rot=rot, block=f"12p-g{len(games)}")
    # 对陪练（参考表）
    ms = int(sched["sparring"]["maps"])
    maps["sparring"] = (maps["1v1"] + [new_seed() for _ in range(max(0, ms - m1))])[:ms]
    for m, seed in enumerate(maps["sparring"]):
        for e in entries:
            for side, seats in enumerate(([e, sparring], [sparring, e])):
                add("sparring", seed, seats, map=m, rot=side, block=None)
    return {"maps": maps, "games": games, "rest": rest}


def seats_12(entries, rng, filler):
    """一张 12 人图的各局座位（列表的列表）。
    - 正好 12 位：随机排座，全轮换 12 局；
    - 不足 12 位：用 filler（基准的副本）补满 12 座，随机排座，全轮换 12 局；filler 参与的两两比较不计分；
    - 多于 12 位（k 位）：随机排成一圈，第 j 局从圈上第 j 位起顺次坐 12 个座位，其余 k − 12 位下场。
      每张图 k 局，每人上场 12 局、每个座位恰好坐一次、下场 k − 12 局。"""
    k = len(entries)
    if k <= 12:
        pick = rng.sample(list(entries) + [filler] * (12 - k), 12)
        return [[pick[(q + rot) % 12] for q in range(12)] for rot in range(12)]
    pick = rng.sample(list(entries), k)
    return [[pick[(j + q) % k] for q in range(12)] for j in range(k)]


def _focus_tables(entries, focus, n, rounds, rng):
    """补打用的 n 人桌：每“轮”只开一桌，focus 里（在本榜的）几位每桌都坐，其余座位从榜内其他人里按上桌次数最少优先抽
    （一样少的随机）。返回 [桌（名字列表，已随机排座）]。"""
    fin = [e for e in focus if e in entries]
    if not fin:
        return []
    if len(fin) >= n:
        raise ValueError(f"补打的 {n} 人桌坐不下 focus 的 {len(fin)} 位")
    others = [e for e in entries if e not in fin]
    cnt = {e: 0 for e in others}
    tables = []
    for _ in range(int(rounds)):
        pick = sorted(others, key=lambda e: (cnt[e], rng.random()))[:n - len(fin)]
        for e in pick:
            cnt[e] += 1
        table = fin + pick
        rng.shuffle(table)
        tables.append(table)
    return tables


def make_board_schedule(pool, boards, sched, sparring, filler, avoid_seeds=(), pool_1v1=None, seeds_1v1=None):
    """多张榜的赛程。pool：所有榜的参赛者合起来（基准在最后）；boards：hostlib.parse_boards 的结果。

    - 1v1：pool 单循环，所有对子同一组 sched["1v1"]["maps"] 张图、换边；各榜只取榜内成员之间的局（共用）；
    - 3 人 / 4 人 / 12 人局：每张榜各打一套（局里带 "board"），分桌用 _deal，12 人局用 seats_12；
      每张榜用自己的分桌随机数（table_seed + 榜名），加一张榜不影响别的榜的分桌；
    - 对陪练：陪练已在 pool 里（某张榜把它当参考线）时，共用 1v1 里已经有每人对陪练的换边对局，不另打；否则照旧另打参考表；
    - avoid_seeds：不用的地图种子（并入的预览对局用过的），保证不会重打同一局；
    - pool_1v1：1v1 单循环的名单（默认 = pool）。基准不计分时把它拿掉（和它的 1v1 本来就不算），多人局照样有它补位；
    - 补打（sched["focus"] = [名字…]，例如后加的一位选手；其余局从之前的终场并入）：1v1 只打含 focus 的对子；
      3 人 / 4 人局每轮只开一桌、每桌都有 focus（_focus_tables）；12 人局照常全员（每张图本来就几乎人人上场）；
    - seeds_1v1：共用 1v1 用这组地图种子（例如沿用之前终场的同一组图），不现抽。"""
    pool_1v1 = list(pool if pool_1v1 is None else pool_1v1)
    focus = list(sched.get("focus") or [])
    if sched.get("map_seed") is None:
        seed_rng = secrets.SystemRandom()
    else:
        seed_rng = random.Random(f"maps-{sched['map_seed']}")
    used = set(int(s) for s in avoid_seeds)

    def new_seed():
        while True:
            s = seed_rng.randrange(1_000_000, 2 ** 31)
            if s not in used:
                used.add(s)
                return s

    games, maps, rest = [], {}, {}

    def add(kind, seed, seats, **meta):
        games.append({"gid": len(games), "type": kind, "n": len(seats), "seed": seed, "seats": list(seats), **meta})

    m1 = int(sched["1v1"]["maps"])
    if seeds_1v1 is not None:
        if len(seeds_1v1) < m1:
            raise ValueError(f"沿用的 1v1 地图只有 {len(seeds_1v1)} 张，不够 {m1} 张")
        maps["1v1"] = [int(s) for s in seeds_1v1[:m1]]
        used.update(maps["1v1"])
    else:
        maps["1v1"] = [new_seed() for _ in range(m1)]
    for m, seed in enumerate(maps["1v1"]):
        for i, a in enumerate(pool_1v1):
            for b in pool_1v1[i + 1:]:
                if focus and a not in focus and b not in focus:
                    continue
                for side, seats in enumerate(([a, b], [b, a])):
                    add("1v1", seed, seats, map=m, rot=side, block=f"1v1-m{m}-{a}-{b}")
    for bd in boards:
        name, entries, bs = bd["name"], bd["entries"], bd["schedule"]
        rng = random.Random(f"tables-{sched['table_seed']}-{name}" + ("-focus" if focus else ""))
        maps[name], rest[name] = {}, {}
        for kind, n in (("4p", 4), ("3p", 3)):
            maps[name][kind], rest[name][kind] = [], []
            sat = {e: 0 for e in entries}
            if focus:
                for r, table in enumerate(_focus_tables(entries, focus, n, bs[kind]["rounds"], rng)):
                    seed = new_seed()
                    maps[name][kind].append(seed)
                    for rot in range(n):
                        add(kind, seed, [table[(k + rot) % n] for k in range(n)], board=name,
                            map=len(maps[name][kind]) - 1, round=r, table=0, rot=rot, block=f"{name}-{kind}-r{r}-t0")
                continue
            for r in range(int(bs[kind]["rounds"])):
                tables, resting = _deal(entries, n, rng, sat)
                rest[name][kind].append(resting)
                for t, table in enumerate(tables):
                    seed = new_seed()
                    maps[name][kind].append(seed)
                    for rot in range(n):
                        add(kind, seed, [table[(k + rot) % n] for k in range(n)], board=name,
                            map=len(maps[name][kind]) - 1, round=r, table=t, rot=rot, block=f"{name}-{kind}-r{r}-t{t}")
        maps[name]["12p"] = []
        for m in range(int(bs["12p"]["maps"])):
            seed = new_seed()
            maps[name]["12p"].append(seed)
            for rot, seats in enumerate(seats_12(entries, rng, filler)):
                add("12p", seed, seats, board=name, map=m, rot=rot, block=f"{name}-12p-g{len(games)}")
    if sparring not in pool:
        ms = int(sched["sparring"]["maps"])
        maps["sparring"] = (maps["1v1"] + [new_seed() for _ in range(max(0, ms - m1))])[:ms]
        for m, seed in enumerate(maps["sparring"]):
            for e in pool:
                if focus and e not in focus:
                    continue
                for side, seats in enumerate(([e, sparring], [sparring, e])):
                    add("sparring", seed, seats, map=m, rot=side, block=None)
    return {"maps": maps, "games": games, "rest": rest}


def import_key(r):
    """并入对局来自哪一份结果："<来源>/<文件名去掉扩展名>"，例如 preview1/multi_formal。"""
    return f"{r.get('source')}/{str(r.get('origin', '')).split('#')[0]}"


def board_rows(rows, board, filler=None):
    """一张榜计分用的对局：共用 1v1 里榜内成员之间的局 + 本榜自己打的多人局 + 并入的对局里（除补位外）同桌全在榜内的局。

    board["import_multi"]：并入的多人局只取这几份结果（import_key 的写法）；None = 不限，只看同桌是否全在榜内。
    并入的 1v1 不受它限制，只看两人是否都在榜内。"""
    ent = set(board["entries"])
    out = []
    for r in rows:
        src = r.get("source", "final")
        if r["type"] == "sparring":
            continue
        if src == "final":
            if r["type"] == "1v1":
                ok = set(r["seats"]) <= ent
            else:
                ok = r.get("board") == board["name"]
        else:
            ok = {s for s in r["seats"] if s != filler} <= ent
            allow = board.get("import_multi")
            if ok and r["type"] != "1v1" and allow is not None:      # 多人局只取本榜口径对应的那几份预览结果
                ok = import_key(r) in allow
            if ok and r["type"] != "1v1" and r.get("board") is not None:   # 并入之前终场的多人局：只进当时同名的榜
                ok = r["board"] == board["name"]
        if ok:
            out.append(r)
    return out


def coverage(rows, entries, filler=None):
    """每人每个局型：上场局数、计分的两两比较次数（看覆盖度）。{类型: {名字: [局数, 比较数]}}。"""
    ent = set(entries)
    out = {}
    for r in rows:
        c = out.setdefault(r["type"], {e: [0, 0] for e in entries})
        for k, s in enumerate(r["seats"]):
            if s in ent:
                c[s][0] += 1
                c[s][1] += sum(1 for j, o in enumerate(r["seats"]) if j != k and o != s and o in ent)
    return out


KIT_FILES = ("tron.hpp", "tron_main.cpp", "judge/judge.cpp", "arena.py")


def import_rows(cfg, imp, expected, pool, kit_now, sparring_dll_sha):
    """读一批已有对局（例如快速预览），核对后转成终场的结果行。返回 (行, 报告)。

    expected：{名字: 现在上场的源码 sha256}（选手 = 封存的 bot.cpp，基准 = greedy.cpp）。核对：
    - 当时的选手包（tron.hpp、tron_main.cpp、judge.cpp、arena.py）和现在逐字节相同，否则整批不要；
    - 每个名字当时编译的源码（bots/<名字>.cpp）的 sha256 和现在上场的相同；陪练比 kit 里的 .dll（快速模式用的）
      和正式陪练 .exe 旁边的同名 .dll；对不上的名字，含它的局全部丢掉；不在 pool 里的名字（作废的等）同样丢掉；
    - 有人超时的局丢掉：快速模式超时只统计、不判罚，正式模式会把那一回合换成沿原方向走，两边不等价；
    - 同一局（局型、地图种子、座位排列）只留第一次出现的那份。
    rename：当时的名字 → 现在的名字；"@filler" 表示补位（基准的副本），换成 cfg["filler_name"]，按补位规则计分。
    分块（自助法单位）照终场的规则：1v1 = 同一对在同一张图上的换边两局，3/4 人局 = 一张桌子的座位轮换，12 人局 = 单局。"""
    import glob
    src = imp["source"]
    filler = cfg["filler_name"]
    rep = {"source": src, "files": [], "kit": "一致", "names": {}, "dropped": {}, "kept": 0, "note": imp.get("note")}
    fin = imp.get("final")             # 并入一次打完的终场（正式模式）：按它的 清单.json / 程序.json 核对
    rep["formal"] = bool(fin)
    kit_then, progs_then = {}, {}
    if fin:
        fin = Path(fin)
        for need in (RESULTS_FILE, SCHEDULE_FILE, MANIFEST_FILE, "程序.json"):
            if not (fin / need).is_file():
                raise H.Stop(f"并入的终场 {fin} 里没有 {need}")
        plan_then = H.read_json(fin / SCHEDULE_FILE)
        if schedule_digest(plan_then["games"]) != plan_then["digest"]:
            raise H.Stop(f"并入的终场的赛程被改过（指纹对不上）：{fin / SCHEDULE_FILE}")
        done = {r["gid"] for r in H.read_lines(fin / RESULTS_FILE)}
        if len(done) != len(plan_then["games"]):
            raise H.Stop(f"并入的终场 {fin} 没打完：{len(done)} / {len(plan_then['games'])} 局")
        frozen = H.read_json(fin / MANIFEST_FILE)["frozen"]
        kit_then = {k: frozen["kit"].get(k) for k in KIT_FILES}
        comp_now = compiler_info(cfg)
        bad_comp = [k for k in ("sha256", "flags", "version") if frozen["compiler"].get(k) != comp_now.get(k)]
        progs_then = H.read_json(fin / "程序.json")
        spar_now = H.sha_file(cfg["sparring"]) if cfg.get("sparring") and Path(cfg["sparring"]).is_file() else None
    else:
        bad_comp = []
        for k in KIT_FILES:
            p = imp["kit"] / k
            kit_then[k] = H.sha_file(p) if p.is_file() else None
    bad_kit = [k for k in KIT_FILES if kit_then[k] != kit_now.get(k)] + [f"编译器的 {k}" for k in bad_comp]
    if bad_kit:
        rep["kit"] = "不一致：" + "、".join(bad_kit)
    rename = {k: (filler if v == "@filler" else v) for k, v in imp["rename"].items()}
    files = sorted({f for pat in imp["results"] for f in glob.glob(pat)})
    out, seen = [], set()

    def drop(why):
        rep["dropped"][why] = rep["dropped"].get(why, 0) + 1

    name_ok = {}

    def check(orig, now):
        if now in name_ok:
            return name_ok[now]
        if fin:                        # 之前的终场：程序.json 里当时编译的源码（陪练是 .exe 本身）和现在的比
            p = progs_then.get(orig) or {}
            if now == cfg["sparring_name"]:
                got, want = p.get("used_sha"), spar_now
            else:
                base = cfg["baseline_name"] if now == filler else now
                got, want = p.get("source_sha"), expected.get(base)
            ok = got is not None and got == want and p.get("status") == "正常"
        elif now == cfg["sparring_name"]:
            p = imp["kit"] / "bots" / f"{orig}.dll"
            got = H.sha_file(p) if p.is_file() else None
            ok, want = got is not None and got == sparring_dll_sha, sparring_dll_sha
        else:
            base = cfg["baseline_name"] if now == filler else now      # 补位是基准的副本，当时用的也是基准的源码
            p = imp["bots"] / f"{base}.cpp"
            got = H.sha_file(p) if p.is_file() else None
            want = expected.get(base)
            ok = got is not None and got == want
        name_ok[now] = ok
        rep["names"][now] = {"then": (got or "没有文件")[:16], "now": (want or "—")[:16], "same": ok}
        return ok

    for f in files:
        rows = H.read_lines(f)
        stem = Path(f).stem
        rep["files"].append({"file": f, "rows": len(rows)})
        for r in rows:
            if bad_kit:
                drop("选手包不一致")
                continue
            seats = [rename.get(s, s) for s in r["seats"]]
            members = [s for s in seats if s != filler]
            if any(s not in pool for s in members):
                drop("有人不在任何榜里（作废或没上名单）")
                continue
            if not all(check(o, s) for o, s in zip(r["seats"], seats)):
                drop("源码和现在上场的不一致")
                continue
            if not fin and any(r["timeouts"]):          # 之前的终场是正式模式，超时已经判罚过，照收
                drop("有超时（快速模式不判罚，和正式模式不等价）")
                continue
            if fin and r.get("source", "final") != "final":
                drop("那次终场自己并入的对局（这里直接从原来源并入）")
                continue
            key = (r["type"], int(r["seed"]), tuple(seats))
            if key in seen:
                drop("同一局重复（只留第一份）")
                continue
            seen.add(key)
            t = r["type"]
            if fin:                                    # 沿用那次终场的分块，加来源前缀
                block = f"{src}-{r['block']}"
            elif t == "1v1":
                a, b = sorted(seats)
                block = f"imp-1v1-s{r['seed']}-{a}-{b}"
            elif t in ("3p", "4p"):
                block = f"{src}-{stem}-{t}-r{r['round']}-t{r['table']}"
            else:
                block = f"{src}-{stem}-{t}-g{r['gid']}"
            row = {k: v for k, v in r.items() if k not in ("seats", "block", "gid")}
            row.update(gid=f"{src}:{stem}:{r['gid']}", seats=seats, block=block, source=src, origin=f"{stem}#{r['gid']}")
            if fin and r.get("replay"):                # 回放留在原来的终场目录里
                row["replay"] = str(fin / r["replay"])
            out.append(row)
    rep["kept"] = len(out)
    rep["by_type"] = count_by_type(out)
    rep["seeds"] = sorted({int(r["seed"]) for f in files for r in H.read_lines(f)})
    return out, rep


def schedule_digest(games):
    return H.sha_bytes(json.dumps([[g["gid"], g["type"], g["seed"], g["seats"]] for g in games]).encode())


def count_by_type(games):
    out = {t: 0 for t in TYPES}
    for g in games:
        out[g["type"]] += 1
    return out


# ====================================================================
# 编译
# ====================================================================

def compile_entries(cfg, out, entries_src):
    """统一编译。entries_src：{名字: (源码路径或 None, 说明)}。编译不过、没有文件的由 substitute（模板）顶替。

    返回 {名字: 记录}；记录里 exe 是可执行文件，status 是 正常 / 顶替，why 是顶替的原因。"""
    A = H.arena(cfg)
    logs = out / "编译记录"
    logs.mkdir(parents=True, exist_ok=True)
    progs = {}
    sub_exe = None
    for name, (src, origin) in entries_src.items():
        rec = {"name": name, "origin": origin, "source": str(src) if src else None, "status": "正常", "why": None}
        began = time.time()
        if src is None or not Path(src).is_file():
            rec.update(status="顶替", why="没有文件")
        else:
            rec["source_sha"] = H.sha_file(src)
            try:
                exe = A.build_bot(Path(src), "exe")
                rec.update(exe=str(exe), used_sha=rec["source_sha"], used="提交的版本")
            except A.Fail as exc:
                (logs / f"{name}.txt").write_text(str(exc), encoding="utf-8")
                rec.update(status="顶替", why="编译失败", log=str(logs / f"{name}.txt"))
        if rec["status"] == "顶替":
            if sub_exe is None:
                sub_exe = A.build_bot(cfg["substitute"], "exe")
            rec.update(exe=str(sub_exe), used_sha=H.sha_file(cfg["substitute"]), used="模板（能走就直走）")
        rec["compile_seconds"] = round(time.time() - began, 1)
        rec["exe_sha"] = H.sha_file(rec["exe"])
        progs[name] = rec
        print(f"  {name:<14}{rec['status']}" + (f"（{rec['why']}，由模板顶替）" if rec["why"] else "")
              + f"  源码 {str(rec.get('source_sha', '—'))[:12]}  {rec['compile_seconds']} 秒", flush=True)
    return progs


def sparring_program(cfg):
    """陪练只有编译好的程序：复制一份带哈希的到 work_dir/build。"""
    src = cfg["sparring"]
    if not src or not src.is_file():
        raise H.Stop(f"找不到陪练程序：{src}")
    digest = H.sha_file(src)
    dst = cfg["work_dir"] / "build" / f"sparring-{digest[:16]}.exe"
    if not dst.is_file():
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    return {"name": cfg["sparring_name"], "origin": "陪练", "source": str(src), "status": "正常", "why": None,
            "exe": str(dst), "used_sha": digest, "exe_sha": digest, "used": "陪练程序"}


def kit_hashes(cfg):
    kit = H.sync_kit(cfg)
    return {name: H.sha_file(kit / name) for name in ("tron.hpp", "tron_main.cpp", "judge/judge.cpp", "arena.py")}


def compiler_info(cfg):
    A = H.arena(cfg)
    cxx = A.find_compiler()
    try:
        ver = subprocess.run([cxx, "--version"], capture_output=True, text=True, timeout=30).stdout.splitlines()[0]
    except Exception:       # noqa: BLE001
        ver = "?"
    exe = shutil.which(cxx) or cxx
    return {"path": str(exe), "version": ver, "sha256": H.sha_file(exe) if Path(exe).is_file() else None,
            "flags": " ".join(A.FLAGS + A.EXE_FLAGS)}


# ====================================================================
# 跑对局
# ====================================================================

_maps = {}


def map_of(cfg, n, seed):
    key = (n, seed)
    if key not in _maps:
        A = H.arena(cfg)
        _maps[key] = A.map_of(n, seed)       # (W, H, rows, spawns)
    return _maps[key]


def want_replay(cfg, g):
    rp = cfg["replays"]
    t = g["type"]
    if t == "1v1":
        return g["map"] < rp["1v1_maps"]
    if t == "4p":
        return g["round"] < rp["4p_rounds"]
    if t == "3p":
        return g["round"] < rp["3p_rounds"]
    if t == "12p":
        return bool(rp["12p"])
    return g["map"] < rp["sparring_maps"]


def has_fault(rec):
    return any(rec["timeouts"]) or any(rec["invalid"]) or any(c >= 0 for c in rec["crash_turn"])


def replay_name(g):
    t = g["type"]
    if t in ("1v1", "sparring"):
        return f"{t}-m{g['map']:02d}-{g['seats'][0]}-vs-{g['seats'][1]}.json"
    if t in ("3p", "4p"):
        return f"{t}-r{g['round']:02d}-t{g['table']}-rot{g['rot']}.json"
    return f"12p-m{g['map']:02d}-rot{g['rot']:02d}.json"


def write_replay(cfg, out, g, rec, progs, stderr=None):
    """回放文件，格式和选手本地 arena.py play 存的完全相同（tron-replay-1，说明.md 第 7 节），没有加字段。"""
    A = H.arena(cfg)
    W, Hh, rows, sp = map_of(cfg, g["n"], g["seed"])
    rp = {"format": "tron-replay-1", "engine": "tron-1.0", "mode": "formal", "n": g["n"], "seed": g["seed"],
          "W": W, "H": Hh, "rows": rows, "spawns": sp, "players": list(g["seats"]),
          "sha": [None if progs[s]["origin"] == "陪练" else progs[s]["used_sha"][:12] for s in g["seats"]],
          "turns": rec["turns"], "moves": rec["moves"], "rank": rec["rank"], "score": A.scores(rec),
          "death_turn": rec["death_turn"], "cause": rec["cause"], "final": rec["final"],
          "first_ms": rec["first_ms"], "avg_ms": rec["avg_ms"], "max_ms": rec["max_ms"],
          "timeouts": rec["timeouts"], "invalid": rec["invalid"], "crash_turn": rec["crash_turn"]}
    if stderr is not None:
        rp["stderr"] = stderr
    path = out.joinpath("回放", *([g["board"]] if g.get("board") else []), g["type"], replay_name(g))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(rp, ensure_ascii=False), encoding="utf-8")
    return path


def describe_exit(code):
    """裁判进程退出码的白话解释（排查"无报错退出"用）。code 为 Popen.returncode（Windows 上是无符号 32 位）。"""
    c = code & 0xFFFFFFFF if isinstance(code, int) else code
    known = {
        0xFFFFFFFF: "-1：被外部强制结束。PowerShell 的 Stop-Process / .NET Process.Kill() 结束进程用的就是这个退出码；"
                    "taskkill /F 和 Python 的 Popen.kill() 是 1。查一查有没有谁按名字或命令行（'judge-'）批量结束进程",
        1: "1：被外部强制结束（taskkill /F、Popen.kill/terminate 的退出码），或程序自己返回 1",
        2: "2：命令行参数不对",
        3: "3：裁判自己的 fatal / 未处理异常过滤器退出；应该在标准输出里有一行 'X ...'，看上面的报告",
        0xC000013A: "0xC000013A：收到 Ctrl+C / 控制台被关",
        0xC0000005: "0xC0000005：访问违例（崩溃）",
        0xC00000FD: "0xC00000FD：栈溢出（崩溃）",
        0xC0000409: "0xC0000409：栈保护失败 / fail-fast（崩溃）",
        0xC0000142: "0xC0000142：动态库初始化失败（常见于桌面堆耗尽 / 会话被注销）",
    }
    if c in known:
        return known[c]
    if c is not None and c >= 0xC0000000:
        return f"0x{c:08X}：Windows 异常代码（崩溃）；去事件查看器 应用程序日志 找 Application Error"
    return f"{code}：不是已知的退出码"


def run_games(cfg, out, games, progs, cpus, label="", replays=True, results_file=None, quiet=False):
    """把一批同人数的对局交给一个裁判进程（正式模式），边打边把结果追加到 results_file。返回 [结果行]。"""
    if not games:
        return []
    A = H.arena(cfg)
    judge = A.build_judge()
    n = games[0]["n"]
    assert all(g["n"] == n for g in games)
    workers = workers_for(n, cpus)
    names = sorted({s for g in games for s in g["seats"]})
    index = {nm: i for i, nm in enumerate(names)}
    lines = [f"BOT {i} {Path(progs[nm]['exe']).resolve()}" for nm, i in index.items()]
    lines += [f"GAME {g['gid']} {n} {g['seed']} {' '.join(str(index[s]) for s in g['seats'])}" for g in games]
    errdir = cfg["work_dir"] / "stderr" / f"run-{os.getpid()}-{int(time.time() * 1000)}"
    errdir.mkdir(parents=True, exist_ok=True)
    tmp = cfg["work_dir"] / "build" / "dll"
    cmd = [str(judge), "run", "formal", "--workers", str(workers), "--tmp", str(tmp), "--stderr-dir", str(errdir)]
    judge_err = open(out / "裁判stderr.txt", "a", encoding="utf-8")
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=judge_err, text=True,
                            encoding="utf-8", errors="replace")
    adopt(proc, cpus)
    proc.stdin.write("\n".join(lines) + "\n")
    proc.stdin.close()
    by_gid = {g["gid"]: g for g in games}
    rows, began, last_print = [], time.time(), time.time()
    seen_gid = set()
    fatal = None
    if not quiet:
        over = f"，{n} 个 bot 挤 {len(cpus)} 个处理器（超卖 {n / len(cpus):.2f} 倍）" if n > len(cpus) else ""
        print(f"  {label}：{len(games)} 局，{n} 人，同时 {workers} 局，绑处理器 {cpus}{over}", flush=True)
    for line in proc.stdout:
        line = line.strip()
        if line.startswith("X "):
            fatal = line[2:]
            break
        if not line.startswith("{"):
            continue
        try:
            rec = json.loads(line)
        except json.JSONDecodeError:         # 裁判被结束时最后一行可能只写了一半：当作没有这局
            continue
        if rec["id"] in seen_gid or rec["id"] not in by_gid:
            continue                         # 同一局只收一次，不认识的局号不收（不会重复计局）
        seen_gid.add(rec["id"])
        g = by_gid[rec["id"]]
        row = {k: g[k] for k in ("gid", "type", "n", "seed", "seats", "map", "rot", "block", "board") if k in g}
        row["source"] = "final"
        for k in ("round", "table"):
            if k in g:
                row[k] = g[k]
        row["score"] = A.scores(rec)
        for k in ("turns", "rank", "death_turn", "cause", "final", "first_ms", "avg_ms", "max_ms", "timeouts",
                  "invalid", "crash_turn", "moves"):
            row[k] = rec[k]
        row["time"] = H.stamp()
        stderr = None
        if replays and (want_replay(cfg, g) or (cfg["replays"]["faults"] and has_fault(rec))):
            stderr = {}
            for k in range(n):
                f = errdir / f"g{g['gid']}_s{k}.txt"
                stderr[str(k)] = f.read_text(encoding="utf-8", errors="replace")[-20000:] if f.is_file() else ""
            row["replay"] = str(write_replay(cfg, out, g, rec, progs, stderr).relative_to(out))
        for k in range(n):
            try:
                (errdir / f"g{g['gid']}_s{k}.txt").unlink()
            except OSError:
                pass
        if results_file:
            H.append_line(results_file, row)
        rows.append(row)
        if not quiet and (time.time() - last_print > 20 or len(rows) == len(games)):
            last_print = time.time()
            spent = time.time() - began
            eta = spent / len(rows) * (len(games) - len(rows))
            print(f"    {len(rows)}/{len(games)} 局，已用 {spent:.0f} 秒，估计还要 {eta:.0f} 秒", flush=True)
    proc.wait()
    if fatal or len(rows) != len(games):
        # 裁判自己的 stderr 常常是空的（被外部结束时一个字也不会写），这里补一行，事后好查
        try:
            judge_err.write(f"[{H.stamp()}] 主办方记录：裁判退出码 {proc.returncode}，{label} 本批 {len(games)} 局，"
                            f"收到结果 {len(rows)} 局。{describe_exit(proc.returncode)}\n")
            if fatal:
                judge_err.write(f"[{H.stamp()}] 裁判报告：{fatal}\n")
        except (OSError, ValueError):
            pass
    judge_err.close()
    shutil.rmtree(errdir, ignore_errors=True)
    if fatal:
        raise H.Stop(f"裁判程序报告无法继续：{fatal}")
    if len(rows) != len(games):
        raise H.Stop(f"裁判程序提前退出（代码 {proc.returncode}：{describe_exit(proc.returncode)}），"
                     f"{len(games) - len(rows)} 局没有结果（已收到的 {len(rows)} 局已写入结果文件）。原样重跑会接着打，不会重复计局。")
    return rows


def other_judges():
    """机器上还有没有别的裁判进程在跑（别人占着处理器会让正式模式的超时判罚偏严；
    也方便确认没有上次留下的孤儿裁判）。返回 [(进程号, 映像名)]。只看不动。"""
    if os.name != "nt":
        return []
    try:
        r = subprocess.run(["tasklist", "/FI", "IMAGENAME eq judge-*", "/FO", "CSV", "/NH"], capture_output=True,
                           text=True, timeout=20, encoding="mbcs", errors="replace")
    except Exception:      # noqa: BLE001
        return []
    out = []
    for line in r.stdout.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) >= 2 and parts[0].lower().startswith("judge-") and parts[1].isdigit():
            out.append((int(parts[1]), parts[0]))
    return out


def run_schedule(cfg, out, plan, progs, cpus, cpus12):
    """按局型分批跑完赛程里还没有结果的局。可以随时中断，重跑时接着打。"""
    results = out / RESULTS_FILE
    done = {r["gid"] for r in H.read_lines(results)}
    timing = {}
    others = other_judges()
    if others and any(g["gid"] not in done for g in plan["games"]):
        print(f"  注意：机器上还有 {len(others)} 个别的裁判进程在跑（进程号 {[p for p, _ in others]}）。"
              "它们占处理器会影响计时；也不要有人按名字结束它们。", flush=True)
    for kind in ("12p", "4p", "3p", "1v1", "sparring"):
        todo = [g for g in plan["games"] if g["type"] == kind and g["gid"] not in done]
        if not todo:
            continue
        began = time.time()
        run_games(cfg, out, todo, progs, cpus12 if kind == "12p" else cpus, label=kind, results_file=results)
        timing[kind] = {"games": len(todo), "seconds": round(time.time() - began, 1)}
    return timing


# ====================================================================
# 计分
# ====================================================================

def mode_means(rows, entries):
    """{局型: {名字: (名次分之和, 座位数)}}。"""
    acc = {m: {e: [0.0, 0] for e in entries} for m in MODES}
    for r in rows:
        m = MODE_OF.get(r["type"])
        if m is None:
            continue
        for s, sc in zip(r["seats"], r["score"]):
            if s not in acc[m]:
                continue
            acc[m][s][0] += sc
            acc[m][s][1] += 1
    return acc


def pair_rows(rows):
    """多人局拆成两两比较 → {局型: [bt.py 要的行]}。

    每局把所有座位两两比较：名次高的记 1、低的记 0，名次相同（同一回合死）记 0.5（和第 6 集的平局一样）。
    同一个名字坐了两个座位（参赛者不满 12 位时的 12 人局）自己和自己不比。"map" 是 block（重抽样的单位）。"""
    out = {m: [] for m in MODES}
    for r in rows:
        m = MODE_OF.get(r["type"])
        if m is None:
            continue
        key = r.get("rank")
        better = (lambda i, j: key[i] < key[j]) if key is not None else (lambda i, j: r["score"][i] > r["score"][j])
        seats = r["seats"]
        for i in range(len(seats)):
            for j in range(i + 1, len(seats)):
                if seats[i] == seats[j]:
                    continue
                s = 1.0 if better(i, j) else 0.0 if better(j, i) else 0.5
                out[m].append({"map": (r["type"], r["block"]), "a": seats[i], "b": seats[j], "score": s})
    return {m: v for m, v in out.items() if v}


def score(rows, entries, baseline, weights, boot=1000, seed=0, lines=(), anchor=None):
    """RULES.md 第 3 节的总分（Bradley–Terry），加上自助法区间和相邻名次之差。口径照第 6 集的 bt.py。

    每个局型（1v1、3～4 人合并、12 人）各拟合一个 BT：每局两两比较（pair_rows），每对加一局虚拟平局，
    greedy = 0，强 10 倍 = +400。总分 = 各局型实力值按 weights 加权。
    区间：按局型分层、以 block 为单位整块重抽样（1v1 是同一对在同一张图上换边的两局，3/4 人局是一张桌子的座位轮换，
    12 人局是单独一局；3 人桌、4 人桌各自分层），三个局型在同一次重抽样里一起抽、各自重新拟合、再加权，取 2.5% / 97.5% 分位。
    并列：相邻两名总分之差的 95% 区间下界 ≤ 0（差在每次重抽样里直接算），可以连成一串。

    只用 entries 之间的两两比较（补位、榜外的人参与的比较不计）。lines：参考线（例如强档陪练），正常参与拟合、
    有自己的区间和与每人之差，但不占名次：order、名次区间、“和谁分不开”都只在其余选手之间算。

    anchor：定为 0 分的锚点（默认就是 baseline）。锚点不是 baseline 时（例如 Doubao），锚点照常占名次、分数恒为 0、
    没有区间，其他人的区间、名次区间都是相对它的；baseline（greedy）照常参与拟合，作为“基准线”参考线，有分数和区间、不占名次。"""
    import bt
    anchor = anchor or baseline
    keep = set(entries)
    by_mode = {m: bt.tally([p for p in v if p["a"] in keep and p["b"] in keep]) for m, v in pair_rows(rows).items()}
    by_mode = {m: t for m, t in by_mode.items() if t}
    modes = [m for m in MODES if m in by_mode]
    if not modes:
        raise H.Stop("没有计分的对局，算不了分。")

    def fit_all(picks, start=None, tol=1e-9):
        rel = {m: bt.fit(bt.merge(by_mode[m], picks[m]), entries, anchor, start=(start or {}).get(m), tol=tol) for m in modes}
        tot = {}
        for e in entries:
            vals = [rel[m][e] for m in modes]
            tot[e] = None if None in vals else sum(weights[m] * rel[m][e] for m in modes)
        return rel, tot

    try:
        point_rel, point_tot = fit_all({m: list(by_mode[m]) for m in modes})
    except ValueError as exc:
        raise H.Stop(f"评分算不了：{exc}")
    start = {m: {e: v / bt.SCALE for e, v in point_rel[m].items() if v is not None} for m in modes}
    players = [e for e in entries if e != baseline or baseline != anchor]     # 基准就是锚点时不进表（恒为 0）

    def key(e):
        return (-(point_tot[e] if point_tot[e] is not None else -1e9), e)
    full = sorted(players, key=key)                                            # 含参考线（和不是锚点的基准）
    order = [e for e in full if e not in lines and e != baseline]              # 占名次的
    pairs = [(a, b) for i, a in enumerate(full) for b in full[i + 1:]]         # a 在点估计里排在 b 前面
    strata = {}
    for m in modes:
        for blk in by_mode[m]:
            strata.setdefault((m, blk[0]), []).append(blk)
    rng = random.Random(seed)
    samples = {}
    done = 0
    for _ in range(boot):
        picks = {m: [] for m in modes}
        for (m, _t), group in strata.items():
            picks[m] += rng.choices(group, k=len(group))
        try:
            rel, tot = fit_all(picks, start, tol=1e-7)
        except ValueError:
            continue            # 抽到的局里锚点没出场或对战关系断开（只在局极少时会发生）
        done += 1
        for e in entries:
            if tot[e] is not None:
                samples.setdefault(("total", e), []).append(tot[e])
            for m in modes:
                if rel[m][e] is not None:
                    samples.setdefault((m, e), []).append(rel[m][e])
        for a, b in pairs:
            if tot[a] is not None and tot[b] is not None:
                samples.setdefault(("diff", a, b), []).append(tot[a] - tot[b])
        live = [e for e in order if tot[e] is not None]
        for e in live:          # 这次重抽样里的名次：总分严格比他高的人数 + 1（只在选手之间排，greedy 不占名次）
            samples.setdefault(("rank", e), []).append(1 + sum(tot[x] > tot[e] for x in live))

    def ci(key, v):             # 同 bt.rate 里的 interval：抽样不足 20 次算不出；区间至少包住点估计
        s = sorted(samples.get(key, []))
        if v is None or len(s) < 20:
            return [None, None]
        low, high = s[int(0.025 * (len(s) - 1))], s[int(math.ceil(0.975 * (len(s) - 1)))]
        return [round(min(low, v), 2), round(max(high, v), 2)]

    def r2(v):
        return None if v is None else round(v, 2)

    table = {}
    for e in entries:
        table[e] = {"total": r2(point_tot[e]), "total_ci": ci(("total", e), point_tot[e]), "modes": {}}
        for m in modes:
            table[e]["modes"][m] = {"score": r2(point_rel[m][e]), "ci": ci((m, e), point_rel[m][e])}
    pairwise = {}
    for a, b in pairs:
        d = point_tot[a] - point_tot[b]
        lo, hi = ci(("diff", a, b), d)
        pairwise[a, b] = {"upper": a, "lower": b, "diff": round(d, 2), "ci": [lo, hi],
                          "separated": lo is not None and (lo > 0.0 or hi < 0.0)}
    adjacent = [pairwise[a, b] for a, b in zip(order, order[1:])]
    rank = {}
    for k, e in enumerate(order):
        lo, hi = ci(("rank", e), k + 1)
        lo, hi = (k + 1, k + 1) if lo is None else (int(lo), int(hi))
        rank[e] = {"rank": k + 1, "lo": lo, "hi": hi,
                   "indistinct": [x for x in order if x != e and not pairwise[(e, x) if (e, x) in pairwise else (x, e)]["separated"]]}
    return {"method": "bt", "entries": entries, "baseline": baseline, "anchor": anchor, "weights": weights, "boot": done, "boot_asked": boot,
            "table": table, "order": order, "full_order": full, "lines": list(lines), "adjacent": adjacent, "pairwise": list(pairwise.values()), "rank": rank,
            "modes_present": modes,
            "pairs": {m: int(sum(g for cells in by_mode[m].values() for g, _ in cells.values())) for m in modes}}


def rank_score(rows, entries, baseline, weights, boot=1000, seed=12345, lines=(), anchor=None):
    """旧计分（名次分减 greedy），成绩表里作附表参考，不定名次。

    point：每个局型求平均名次分，减去 greedy 的平均，×100；按 weights 加权得总分。
    区间：按局型分层重抽样，单位是"一组相关的局"——1v1 是同一对在同一张图上换边的两局，3/4 人局是一张桌子的轮换，
    12 人局是单独一局（只有 4 张图，按图抽太粗）。每次重抽样都重新算 greedy 的平均，所以区间里含着基准自身的噪声。"""
    import numpy as np
    E = len(entries)
    idx = {e: i for i, e in enumerate(entries)}
    strata = {}
    for r in rows:
        if r["type"] not in MODE_OF:
            continue
        st = strata.setdefault(r["type"], {})
        blk = st.setdefault(r["block"], (np.zeros(E), np.zeros(E)))
        for s, sc in zip(r["seats"], r["score"]):
            if s not in idx:            # 补位、榜外的人
                continue
            blk[0][idx[s]] += sc
            blk[1][idx[s]] += 1
    arr = {t: (np.array([b[0] for b in st.values()]), np.array([b[1] for b in st.values()])) for t, st in strata.items()}
    anchor = anchor or baseline
    bi = idx[anchor]                # 每个局型减去锚点的平均

    def totals(sums):          # sums: {type: (S[...,E], C[...,E])} → (每局型的相对分 {mode: [...,E]}, 总分)
        rel, tot = {}, None
        for m in MODES:
            ts = [t for t in arr if MODE_OF[t] == m]
            if not ts:
                continue
            S = sum(sums[t][0] for t in ts)
            C = sum(sums[t][1] for t in ts)
            with np.errstate(invalid="ignore", divide="ignore"):
                mean = S / C
            rel[m] = 100 * (mean - mean[..., bi:bi + 1])
            part = weights[m] * rel[m]
            tot = part if tot is None else tot + part
        return rel, tot

    point_rel, point_tot = totals({t: (a[0].sum(0), a[1].sum(0)) for t, a in arr.items()})
    rng = np.random.default_rng(seed)
    rel_samples = {m: [] for m in point_rel}
    tot_samples = []
    chunk = 100
    for start in range(0, boot, chunk):
        b = min(chunk, boot - start)
        sums = {}
        for t, (S, C) in arr.items():
            pick = rng.integers(0, len(S), size=(b, len(S)))
            sums[t] = (S[pick].sum(1), C[pick].sum(1))
        rel, tot = totals(sums)
        for m in rel:
            rel_samples[m].append(rel[m])
        tot_samples.append(tot)
    tot_s = np.concatenate(tot_samples) if tot_samples else np.zeros((0, E))
    rel_s = {m: np.concatenate(v) for m, v in rel_samples.items()}

    def ci(x):
        x = x[~np.isnan(x)]
        if len(x) == 0:
            return [None, None]
        return [round(float(np.percentile(x, 2.5)), 2), round(float(np.percentile(x, 97.5)), 2)]

    acc = mode_means(rows, entries)
    table = {}
    for e in entries:
        i = idx[e]
        table[e] = {"total": None if point_tot is None else round(float(point_tot[i]), 2),
                    "total_ci": ci(tot_s[:, i]) if len(tot_s) else [None, None], "modes": {}}
        for m in point_rel:
            s, c = acc[m][e]
            table[e]["modes"][m] = {"score": round(float(point_rel[m][i]), 2), "ci": ci(rel_s[m][:, i]),
                                    "mean": round(s / c, 4) if c else None, "seats": c}
    players = [e for e in entries if e != baseline and e not in lines]
    order = sorted(players, key=lambda e: -(table[e]["total"] if table[e]["total"] is not None else -1e9))
    adjacent = []
    for a, b in zip(order, order[1:]):
        d = tot_s[:, idx[a]] - tot_s[:, idx[b]]
        lo, hi = ci(d)
        adjacent.append({"upper": a, "lower": b, "diff": round(table[a]["total"] - table[b]["total"], 2), "ci": [lo, hi],
                         "tie": lo is not None and lo <= 0 <= hi})
    # 名次：相邻之差的区间含 0 就并列，可以连成一串
    groups, cur = [], [order[0]] if order else []
    for adj in adjacent:
        if adj["tie"]:
            cur.append(adj["lower"])
        else:
            groups.append(cur)
            cur = [adj["lower"]]
    if cur:
        groups.append(cur)
    rank, pos = {}, 1
    for grp in groups:
        for k, e in enumerate(grp):
            rank[e] = {"rank": pos + k, "group": [pos, pos + len(grp) - 1] if len(grp) > 1 else None}
        pos += len(grp)
    return {"entries": entries, "baseline": baseline, "weights": weights, "boot": boot, "table": table,
            "order": order, "adjacent": adjacent, "rank": rank, "lines": list(lines), "anchor": anchor,
            "modes_present": list(point_rel)}


def rank_text(rk):
    """BT 名次：自助法 95% 名次区间，"第 a～b 名"；a = b 时写单一名次。"""
    if "lo" in rk:
        return f"第 {rk['lo']} 名" if rk["lo"] == rk["hi"] else f"第 {rk['lo']}～{rk['hi']} 名"
    if rk.get("group"):         # 旧计分（附表、旧的 results.json）
        return f"{rk['rank']}（{rk['group'][0]}–{rk['group'][1]} 并列）"
    return str(rk["rank"])


# ====================================================================
# 统计：故障、对陪练、给视频用的
# ====================================================================

def fault_stats(rows, names):
    out = {e: {"games": 0, "turns": 0, "timeouts": 0, "timeout_games": 0, "invalid": 0, "crash_games": 0,
               "first_max": 0.0, "max_ms": 0.0, "avg_sum": 0.0} for e in names}
    for r in rows:
        for k, s in enumerate(r["seats"]):
            if s not in out:
                continue
            o = out[s]
            o["games"] += 1
            live = r["death_turn"][k] if r["death_turn"][k] >= 0 else r["turns"]
            o["turns"] += live
            o["timeouts"] += r["timeouts"][k]
            o["timeout_games"] += r["timeouts"][k] > 0
            o["invalid"] += r["invalid"][k]
            o["crash_games"] += r["crash_turn"][k] >= 0
            o["first_max"] = max(o["first_max"], r["first_ms"][k])
            o["max_ms"] = max(o["max_ms"], r["max_ms"][k])
            o["avg_sum"] += r["avg_ms"][k] * max(0, live - 1)
    for o in out.values():
        o["avg_ms"] = round(o["avg_sum"] / max(1, o["turns"] - o["games"]), 2)
        del o["avg_sum"]
    return out


def sparring_table(rows, entries, sparring):
    out = {}
    for e in entries:
        mine = [r for r in rows if r["type"] in ("sparring", "1v1") and e in r["seats"] and sparring in r["seats"]]
        w = d = l = 0
        sc = []
        for r in mine:
            k = r["seats"].index(e)
            s = r["score"][k]
            sc.append(s)
            w, d, l = w + (s == 1), d + (s == 0.5), l + (s == 0)
        out[e] = {"games": len(mine), "w": w, "d": d, "l": l,
                  "rate": round(sum(sc) / len(sc), 4) if sc else None,
                  "turns": round(statistics.mean(r["turns"] for r in mine), 1) if mine else None}
    return out


def reconstruct(cfg, r):
    """按 moves 重放头部位置，找出每次死亡撞的是谁的轨迹、抢格输给了谁。返回 [(死者座位, 回合, 死因, 对方座位或 None)]。"""
    _, _, _, spawns = map_of(cfg, r["n"], r["seed"])
    pos = [tuple(p) for p in spawns]
    owner = {p: k for k, p in enumerate(pos)}
    events = []
    for t, mv in enumerate(r["moves"]):
        tgt = {}
        for k, ch in enumerate(mv):
            if ch in DIRS:
                dx, dy = DIRS[ch]
                tgt[k] = (pos[k][0] + dx, pos[k][1] + dy)
        dying = [k for k in tgt if r["death_turn"][k] == t]
        movers = [k for k in tgt if r["death_turn"][k] != t]
        for k in dying:
            cause = r["cause"][k]
            other = None
            if cause == "trail":
                other = owner.get(tgt[k])
            elif cause == "headon":
                other = next((j for j in movers if tgt[j] == tgt[k]), None)
            events.append((k, t, cause, other))
        for k in movers:
            pos[k] = tgt[k]
            owner[pos[k]] = k
    return events


def video_stats(cfg, rows, entries):
    """每位参赛者在三个局型里：平均存活回合、存活比例、第一名次数、死因分布、撞死别人（别人撞上他的轨迹）、抢格胜负。"""
    st = {}

    def slot(e, m):
        return st.setdefault(e, {}).setdefault(m, {"games": 0, "turns_alive": 0, "game_turns": 0, "wins": 0,
                                                    "causes": {}, "kills": 0, "headon_won": 0, "headon_lost": 0,
                                                    "killed_by": {}})
    longest = {}
    for r in rows:
        m = MODE_OF.get(r["type"], "sparring")
        for k, s in enumerate(r["seats"]):
            o = slot(s, m)
            o["games"] += 1
            live = r["death_turn"][k] if r["death_turn"][k] >= 0 else r["turns"]
            o["turns_alive"] += live
            o["game_turns"] += r["turns"]
            o["wins"] += r["rank"][k] == 0 and r["rank"].count(0) == 1
            o["causes"][r["cause"][k]] = o["causes"].get(r["cause"][k], 0) + 1
        for k, t, cause, other in reconstruct(cfg, r):
            if other is None or other == k:
                continue
            killer = r["seats"][other]
            victim = r["seats"][k]
            if cause == "trail":
                slot(killer, m)["kills"] += 1
            elif cause == "headon":
                slot(killer, m)["headon_won"] += 1
                slot(victim, m)["headon_lost"] += 1
            kb = slot(victim, m)["killed_by"]
            kb[killer] = kb.get(killer, 0) + 1
        if r["turns"] > longest.get(m, {"turns": -1})["turns"]:
            longest[m] = {"turns": r["turns"], "gid": r["gid"], "seats": r["seats"], "replay": r.get("replay")}
    for e in st:
        for o in st[e].values():
            o["avg_alive"] = round(o["turns_alive"] / o["games"], 1)
            o["alive_share"] = round(o["turns_alive"] / max(1, o["game_turns"]), 3)
    turns = {}
    for r in rows:
        turns.setdefault(r["type"], []).append(r["turns"])
    lengths = {t: {"games": len(v), "mean": round(statistics.mean(v), 1), "median": statistics.median(v),
                   "max": max(v), "min": min(v)} for t, v in turns.items()}
    return {"per_entry": st, "longest": longest, "lengths": lengths}


# ====================================================================
# 输出
# ====================================================================

def fmt(x, signed=True, nd=1):
    if x is None:
        return "—"
    return f"{x:+.{nd}f}" if signed else f"{x:.{nd}f}"


def ci_text(c):
    return "—" if not c or c[0] is None else f"[{c[0]:+.1f}, {c[1]:+.1f}]"


def fmt0(v):
    """实力值的写法，照第 6 集：取整，不显示 "-0"。"""
    if v is None:
        return "—"
    v = round(v)
    return f"{v + 0 if v else 0:d}"


def ci0(c):
    return "—" if not c or c[0] is None else f"{fmt0(c[0])} ~ {fmt0(c[1])}"


BT_RULE = ("计分按 RULES.md 第 3 节：每个局型（1v1、3～4 人合并、12 人）各拟合一个 Bradley–Terry 实力值。"
           "每局把所有选手两两比较，名次高的算赢（1），同一回合死的算平（各 0.5）；每对加一局虚拟平局；"
           "greedy = 0，强 10 倍 = +400（和 Elo 同一把尺：差 400 分 ≈ 10:1）。")
RANK_RULE = ("名次是自助法 95% 名次区间：每次重抽样按总分重新排名（只在选手之间排，greedy 不占名次），"
             "取每人名次的 2.5% 和 97.5% 分位，写作“第 a～b 名”，a = b 时写单一名次。"
             "两人分不分得开看两人总分之差的 95% 区间是否含 0；这是两两之间的关系，不传递（A 和 B 分不开、B 和 C 分不开，"
             "不等于 A 和 C 分不开），见第 2 节。")


def write_results_md(cfg, out, sc, progs, faults, spar, seals, plan, timing, env, ref=None):
    """sc：score() 的结果（BT，定名次）；ref：rank_score() 的结果（旧的名次分，只进附表）。"""
    base = sc["baseline"]
    w = sc["weights"]
    L = [f"# {cfg['name']} · 正式成绩", "",
         f"生成于 {H.stamp()}。" + BT_RULE
         + f"总分 = 1v1 {w['1v1']:.0%}、3～4 人 {w['3-4']:.0%}、12 人 {w['12']:.0%} 加权。greedy 恰好 0 分。", "",
         f"区间：自助法 {sc['boot']} 次，按局型分层整块重抽样（1v1 以“同一对、同一张图的换边两局”为单位，3～4 人局以一张桌子的"
         "座位轮换为单位，12 人局以单局为单位），三个局型同一次一起抽、各自重新拟合再加权，95% 区间。", "", RANK_RULE, ""]
    if set(sc["modes_present"]) != set(MODES):
        L += [f"注意：这次只打了 {'、'.join(MODE_TEXT[m] for m in sc['modes_present'])}，总分只含这些局型。", ""]
    L += ["## 1. 总分", "", "| 名次 | 选手 | 总分 | 95% 区间 | 1v1 | 3～4 人 | 12 人 | 上场的程序 |", "|---|---|---:|---|---:|---:|---:|---|"]
    for e in sc["order"] + [base]:
        t = sc["table"][e]
        rk = "—（基准）" if e == base else rank_text(sc["rank"][e])
        md = [fmt0(t["modes"].get(m, {}).get("score")) for m in MODES]
        p = progs[e]
        used = ("基准程序" if e == base else "提交的版本") if p["status"] == "正常" else f"模板顶替（{p['why']}）"
        ci = "—" if e == base else ci0(t["total_ci"])
        L.append(f"| {rk} | {H.shown(cfg, e)} | {fmt0(t['total'])} | {ci} | {' | '.join(md)} | {used} |")
    L += ["", "## 2. 两两比较", "", "每位选手和谁分不开（总分之差的 95% 区间含 0）：", "",
          "| 选手 | 名次 | 和他分不开的 |", "|---|---|---|"]
    for e in sc["order"]:
        rk = sc["rank"][e]
        L.append(f"| {e} | {rank_text(rk)} | {'、'.join(rk['indistinct']) or '（都分得开）'} |")
    L += ["", "总分之差（行 − 列）。带 ≈ 的是差的 95% 区间含 0、分不开；不带的分得开。", "",
          "| 行 − 列 | " + " | ".join(sc["order"]) + " |", "|---|" + "---:|" * len(sc["order"])]
    pw = {(x["upper"], x["lower"]): x for x in sc["pairwise"]}
    for a in sc["order"]:
        cells = []
        for b in sc["order"]:
            if a == b:
                cells.append("·")
                continue
            x, sign = (pw[a, b], 1) if (a, b) in pw else (pw[b, a], -1)
            d = fmt0(sign * x["diff"])
            cells.append(d if x["separated"] else f"{d} ≈")
        L.append(f"| {a} | " + " | ".join(cells) + " |")
    L += ["", "相邻两名之差的区间：", "", "| 上 | 下 | 总分之差 | 差的 95% 区间 | 判定 |", "|---|---|---:|---|---|"]
    for x in sc["adjacent"]:
        L.append(f"| {x['upper']} | {x['lower']} | {fmt0(x['diff'])} | {ci0(x['ci'])} | {'分得开' if x['separated'] else '分不开（区间含 0）'} |")
    L += ["", "## 3. 各局型（BT 实力值）", ""]
    for m in sc["modes_present"]:
        L += [f"### {MODE_TEXT[m]}（权重 {w[m]:.0%}，两两比较 {sc['pairs'][m]} 次）", "",
              "| 选手 | 实力值 | 95% 区间 |", "|---|---:|---|"]
        for e in sorted(sc["entries"], key=lambda e: -(sc["table"][e]["modes"][m]["score"] or 0)):
            x = sc["table"][e]["modes"][m]
            L.append(f"| {H.shown(cfg, e)} | {fmt0(x['score'])} | {'—' if e == base else ci0(x['ci'])} |")
        L.append("")
    L += ["## 4. 对陪练（参考，不计分）", "",
          f"每位参赛者 1v1 对陪练，{plan['schedule']['sparring']['maps']} 张图 × 换边。得分率 = 平均名次分（胜 1、同死 0.5、负 0）。", "",
          "| 选手 | 局数 | 胜 | 同死 | 负 | 得分率 | 平均回合 |", "|---|---:|---:|---:|---:|---:|---:|"]
    for e in sorted(spar, key=lambda e: -(spar[e]["rate"] or 0)):
        s = spar[e]
        if s["games"]:
            L.append(f"| {H.shown(cfg, e)} | {s['games']} | {s['w']} | {s['d']} | {s['l']} | {s['rate']:.1%} | {s['turns']} |")
    L += ["", "## 5. 编译、超时、崩溃", "",
          "计分对局与对陪练的对局合起来统计。超时 = 超过 CPU 预算（首回合 2000、之后 50 CPU 毫秒）或墙上保险的回合，"
          "这一回合沿上一回合方向走。CPU 用量是裁判按进程量的。", "",
          "| 选手 | 编译 | 局数 | 超时回合 | 有超时的局 | 不合法 | 崩溃的局 | 首回合最多 | 每回合平均 | 每回合最多 |",
          "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for e in sc["entries"]:
        f, p = faults[e], progs[e]
        comp = "通过" if p["status"] == "正常" else f"**{p['why']}**，模板顶替"
        L.append(f"| {H.shown(cfg, e)} | {comp} | {f['games']} | {f['timeouts']} | {f['timeout_games']} | {f['invalid']} | "
                 f"{f['crash_games']} | {f['first_max']:.0f} | {f['avg_ms']:.1f} | {f['max_ms']:.1f} |")
    L += ["", "## 6. 用时", "",
          f"推荐 {cfg['target_minutes']} 分钟，{cfg['limit_minutes']} 分钟强制收卷。“最后修改”是封存的 bot.cpp 的修改时间"
          "减去开赛时刻（文件时间，只作参考）。", "",
          "| 选手 | 工具 | 封存方式 | 开赛到封存 | 最后修改于 | 复核 | sha256 |", "|---|---|---|---:|---:|---|---|"]
    for p in cfg["players"]:
        s = seals.get(p["name"]) or {}
        rc = s.get("recheck")
        check = "—" if not rc else ("没变" if rc["same"] else "**封存后又变了**")
        L.append(f"| {H.shown(cfg, p['name'])} | {p['tool']}{' ' + p['model'] if p.get('model') else ''} | {s.get('how', '—')} | "
                 f"{H.minutes(s.get('elapsed_seconds'))} | {s.get('last_edit', '—')} | {check} | "
                 f"{(s.get('sha256') or '—')[:12]} |")
    cnt = count_by_type(plan["games"])
    L += ["", "## 7. 赛程与环境", "",
          f"- 参赛者 {len(sc['entries'])} 位（选手 {len(sc['entries']) - 1} + greedy），陪练只进第 4 节。",
          f"- 局数：1v1 {cnt['1v1']}、4 人 {cnt['4p']}、3 人 {cnt['3p']}、12 人 {cnt['12p']}、对陪练 {cnt['sparring']}，"
          f"共 {len(plan['games'])} 局。地图种子在 `赛程.json`（不公开）。",
          f"- 赛程指纹 {plan['digest'][:16]}…，分桌种子 {plan['schedule']['table_seed']}。",
          f"- 绑核：{env.get('cpus')}；12 人局 {env.get('cpus12')}。电源方案：{env.get('power')}。",
          f"- 每 CPU 毫秒周期数（裁判测定）：{env.get('per_ms')}。"]
    for k, v in (timing or {}).items():
        L.append(f"- {k}：{v['games']} 局，{v['seconds'] / 60:.1f} 分钟（这一次运行）")
    L += ["", "回放在 `回放\\`（目录 `回放\\INDEX.md`），逐局结果在 `results.jsonl`，汇总在 `results.json`，"
          "给视频用的统计在 `视频统计.md`。", ""]
    if ref:
        L += ["## 附表：名次分（旧计分，只作参考，不定名次）", "",
              "每局名次归一成 (N − 名次) / (N − 1)（同回合死的取平均），每个局型求平均、减去 greedy 的平均、×100，"
              f"再按同样的权重加权。区间是同样分层重抽样 {ref['boot']} 次的 95% 区间。", "",
              "| 选手 | 名次分总分 | 95% 区间 | 1v1 | 3～4 人 | 12 人 |", "|---|---:|---|---:|---:|---:|"]
        for e in ref["order"] + [base]:
            t = ref["table"][e]
            L.append(f"| {H.shown(cfg, e)} | {fmt(t['total'])} | {ci_text(t['total_ci'])} | "
                     + " | ".join(fmt(t["modes"].get(m, {}).get("score")) for m in MODES) + " |")
        L.append("")
    (out / "RESULTS.md").write_text("\n".join(L), encoding="utf-8")


SOURCE_TEXT = {"final": "终场新打"}


def write_board_md(cfg, out, bd, sc, ref, progs, faults, spar, seals, info, timing, env):
    """一张榜的成绩表 RESULTS_<榜名>.md。bd：hostlib.parse_boards 的一项；info：赛程说明（host 里算好）：
    {"schedule": [文字行], "sources": {来源: {类型: 局数}}, "coverage": coverage() 的结果, "filler": bool, "imports": [报告]}。

    锚点（sc["anchor"]，配置的 anchor）定为 0 分、不显示区间，照常占名次；基准（greedy）不是锚点时显示成“greedy 基准线”，
    带分数和区间、不占名次；lines（例如强档陪练线）同样带分数和区间、不占名次。参考线按总分插在表里相应的位置。"""
    base, lines, w = sc["baseline"], bd["lines"], sc["weights"]
    anc = sc.get("anchor") or base
    label = dict(bd["line_labels"])
    has_base = base in sc["entries"]                           # 基准计不计分（配置 baseline_scored）
    if base != anc:
        label[base] = f"{base} 基准线"
    refl = list(lines) + ([base] if has_base and base != anc else [])       # 不占名次、有区间的参考线

    def tag(e):
        return f"（{label[e]}）" if e in refl else "（基准线）" if e == base else "（锚点，0 分）" if e == anc else ""
    L = [f"# {cfg['name']} · {bd['name']}", ""]
    if bd.get("note"):
        L += [bd["note"], ""]
    who = f"占名次的 {len(bd['players'])} 位：{'、'.join(bd['players'])}"
    if base != anc:
        who += f"；**{anc} 是锚点，定为 0 分**（照常占名次，没有区间；其他人的分数、区间、名次区间都是相对它的）"
        if has_base:
            who += f"；{base} 照常参赛，显示成“{label[base]}”（有分数和区间，不占名次）"
        else:
            who += (f"；{base} 只坐座位、不计分（和 12 人局的补位副本一样：凡是涉及它的两两比较，包括 1v1 里和它的对局，"
                    "一律不计），不上榜")
    else:
        who += f"；{base} 是基准线（定为 0 分）"
    if lines:
        who += "；" + "、".join(f"{x} 是{label[x]}" for x in lines) + "（正常参赛、参与拟合，不占名次）"
    rule = BT_RULE if base == anc else BT_RULE.replace(f"{base} = 0", f"{anc} = 0（锚点）")
    if not has_base:
        rule = rule.replace("每局把所有选手两两比较", f"每局把上榜的程序两两比较（{base} 和补位不算）")
    L += [f"生成于 {H.stamp()}。{who}。", "",
          rule + f"总分 = 1v1 {w['1v1']:.0%}、3～4 人 {w['3-4']:.0%}、12 人 {w['12']:.0%} 加权。", "",
          f"区间：自助法 {sc['boot']} 次，按局型分层整块重抽样（1v1 以“同一对、同一张图的换边两局”为单位，3～4 人局以一张桌子的"
          "座位轮换为单位，12 人局以单局为单位；并入的预览对局按同样的规则分块），三个局型同一次一起抽、各自重新拟合再加权，95% 区间。"
          + (f"每次重抽样都把 {anc} 重新定为 0，所以区间是“相对 {anc}”的。" if base != anc else ""), "",
          RANK_RULE.replace("greedy 不占名次", ("参考线不占名次" if not has_base else f"{base} 和参考线不占名次")
                            + (f"，{anc} 照常排" if base != anc else "")), ""]
    if set(sc["modes_present"]) != set(MODES):
        L += [f"注意：这次只有 {'、'.join(MODE_TEXT[m] for m in sc['modes_present'])}，总分只含这些局型。", ""]
    L += ["## 1. 总分", "", "参考线按总分插在相应的位置。", "",
          "| 名次 | 选手 | 总分 | 95% 区间 | 1v1 | 3～4 人 | 12 人 | 上场的程序 |", "|---|---|---:|---|---:|---:|---:|---|"]
    shown = sc["full_order"] + ([base] if has_base and base not in sc["full_order"] else [])
    for e in sorted(shown, key=lambda e: -(sc["table"][e]["total"] or 0) if sc["table"][e]["total"] is not None else 1e9):
        t = sc["table"][e]
        if e in refl:
            rk, name, ci = f"—（{label[e]}）", f"**{H.shown(cfg, e)}（{label[e]}）**", ci0(t["total_ci"])
        elif e == base:
            rk, name, ci = "—（基准线）", f"**{e}（基准线）**", "—"
        elif e == anc:
            rk, name, ci = rank_text(sc["rank"][e]), f"**{H.shown(cfg, e)}（锚点，0 分）**", "—"
        else:
            rk, name, ci = rank_text(sc["rank"][e]), H.shown(cfg, e), ci0(t["total_ci"])
        md = [fmt0(t["modes"].get(m, {}).get("score")) for m in MODES]
        p = progs[e]
        used = ("基准程序" if e == base else "陪练程序" if p.get("origin") == "陪练" else "提交的版本") if p["status"] == "正常" \
            else f"模板顶替（{p['why']}）"
        L.append(f"| {rk} | {name} | {fmt0(t['total'])} | {ci} | {' | '.join(md)} | {used} |")
    pw = {(x["upper"], x["lower"]): x for x in sc["pairwise"]}

    def diff(a, b):
        x, sign = (pw[a, b], 1) if (a, b) in pw else (pw[b, a], -1)
        ci = x["ci"] if sign == 1 else [None if v is None else -v for v in x["ci"][::-1]]
        return sign * x["diff"], ci, x["separated"]
    if refl or base == anc:
        old = base == anc
        L += ["", "### 和参考线比", "", "每位选手总分减参考线总分（括号里是差的 95% 区间）；带 ≈ 的是区间含 0、分不开。"
              + ("greedy 线看总分区间是否含 0。" if old else ""), "",
              "| 选手 | 名次 | " + " | ".join(f"减 {x}（{label[x]}）" for x in refl) + (" | 和 greedy 比" if old else "") + " |",
              "|---|---|" + "---|" * (len(refl) + old)]
        for e in sc["order"]:
            cells = []
            for x in refl:
                d, ci, sep = diff(e, x)
                cells.append(f"{fmt0(d)}（{ci0(ci)}）" + ("" if sep else " ≈"))
            if old:
                lo, hi = sc["table"][e]["total_ci"]
                cells.append("—" if lo is None else "之上" if lo > 0 else "之下" if hi < 0 else "分不开")
            L.append(f"| {e} | {rank_text(sc['rank'][e])} | {' | '.join(cells)} |")
    L += ["", "## 2. 两两比较", "", "每位选手和谁分不开（总分之差的 95% 区间含 0；只在占名次的选手之间）：", "",
          "| 选手 | 名次 | 和他分不开的 |", "|---|---|---|"]
    for e in sc["order"]:
        rk = sc["rank"][e]
        L.append(f"| {e} | {rank_text(rk)} | {'、'.join(rk['indistinct']) or '（都分得开）'} |")
    cols = sc["order"] + refl
    L += ["", "总分之差（行 − 列）。带 ≈ 的是差的 95% 区间含 0、分不开；不带的分得开。", "",
          "| 行 − 列 | " + " | ".join(cols) + " |", "|---|" + "---:|" * len(cols)]
    for a in cols:
        cells = []
        for b in cols:
            if a == b:
                cells.append("·")
                continue
            d, _, sep = diff(a, b)
            cells.append(fmt0(d) if sep else f"{fmt0(d)} ≈")
        L.append(f"| {a} | " + " | ".join(cells) + " |")
    L += ["", "相邻两名之差的区间：", "", "| 上 | 下 | 总分之差 | 差的 95% 区间 | 判定 |", "|---|---|---:|---|---|"]
    for x in sc["adjacent"]:
        L.append(f"| {x['upper']} | {x['lower']} | {fmt0(x['diff'])} | {ci0(x['ci'])} | {'分得开' if x['separated'] else '分不开（区间含 0）'} |")
    L += ["", "## 3. 各局型（BT 实力值）", ""]
    for m in sc["modes_present"]:
        L += [f"### {MODE_TEXT[m]}（权重 {w[m]:.0%}，两两比较 {sc['pairs'][m]} 次）", "",
              "| 选手 | 实力值 | 95% 区间 |", "|---|---:|---|"]
        for e in sorted(sc["entries"], key=lambda e: -(sc["table"][e]["modes"][m]["score"] or 0)):
            x = sc["table"][e]["modes"][m]
            L.append(f"| {H.shown(cfg, e)}{tag(e)} | {fmt0(x['score'])} | {'—' if e == anc else ci0(x['ci'])} |")
        L.append("")
    sp = cfg["sparring_name"]
    L += [f"## 4. 1v1 直接对 {sp}（参考，不单独计分）", "",
          f"取自共用 1v1 里和 {sp} 的换边对局（本榜不含 {sp} 时，这些局不进本榜的计分），含并入的预览对局。"
          "得分率 = 平均名次分（胜 1、同死 0.5、负 0）。", "",
          "| 选手 | 局数 | 胜 | 同死 | 负 | 得分率 | 平均回合 |", "|---|---:|---:|---:|---:|---:|---:|"]
    for e in sorted(spar, key=lambda e: -(spar[e]["rate"] or 0)):
        s = spar[e]
        if s["games"]:
            L.append(f"| {H.shown(cfg, e)} | {s['games']} | {s['w']} | {s['d']} | {s['l']} | {s['rate']:.1%} | {s['turns']} |")
    L += ["", "## 5. 编译、超时、崩溃", "",
          "只统计终场新打的本榜对局（正式模式）；并入的预览对局是快速模式，有超时的局已经整局丢掉，不进这张表。"
          "超时 = 超过 CPU 预算（首回合 2000、之后 50 CPU 毫秒）或墙上保险的回合，这一回合沿上一回合方向走。", "",
          "| 选手 | 编译 | 局数 | 超时回合 | 有超时的局 | 不合法 | 崩溃的局 | 首回合最多 | 每回合平均 | 每回合最多 |",
          "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for e in sc["entries"]:
        f, p = faults[e], progs[e]
        comp = "通过" if p["status"] == "正常" else f"**{p['why']}**，模板顶替"
        L.append(f"| {H.shown(cfg, e)} | {comp} | {f['games']} | {f['timeouts']} | {f['timeout_games']} | {f['invalid']} | "
                 f"{f['crash_games']} | {f['first_max']:.0f} | {f['avg_ms']:.1f} | {f['max_ms']:.1f} |")
    L += ["", "## 6. 用时", "",
          f"推荐 {cfg['target_minutes']} 分钟，{cfg['limit_minutes']} 分钟强制收卷。“最后修改”是封存的 bot.cpp 的修改时间"
          "减去开赛时刻（文件时间，只作参考）。", "",
          "| 选手 | 工具 | 封存方式 | 开赛到封存 | 最后修改于 | 复核 | sha256 |", "|---|---|---|---:|---:|---|---|"]
    for p in cfg["players"]:
        if p["name"] not in bd["entries"]:
            continue
        s = seals.get(p["name"]) or {}
        rc = s.get("recheck")
        check = "—" if not rc else ("没变" if rc["same"] else "**封存后又变了**")
        L.append(f"| {H.shown(cfg, p['name'])} | {p['tool']}{' ' + p['model'] if p.get('model') else ''} | {s.get('how', '—')} | "
                 f"{H.minutes(s.get('elapsed_seconds'))} | {s.get('last_edit', '—')} | {check} | "
                 f"{(s.get('sha256') or '—')[:12]} |")
    L += ["", "## 7. 赛程、对局来源与环境", ""] + [f"- {x}" for x in info["schedule"]]
    src = info["sources"]
    kinds = [t for t in ("1v1", "4p", "3p", "12p") if any(src[s].get(t) for s in src)]
    L += ["", "本榜计分用的对局，按来源：", "", "| 来源 | " + " | ".join(kinds) + " | 合计 |", "|---|" + "---:|" * (len(kinds) + 1)]
    formal = {rp["source"]: f"{rp['source']}（{rp.get('note') or '之前的终场'}）"
              for rp in info.get("imports") or [] if rp.get("formal")}
    for s, c in src.items():
        L.append(f"| {SOURCE_TEXT.get(s) or formal.get(s, s)} | " + " | ".join(str(c.get(t, 0)) for t in kinds) + f" | {sum(c.get(t, 0) for t in kinds)} |")
    L.append("| 合计 | " + " | ".join(str(sum(src[s].get(t, 0) for s in src)) for t in kinds)
             + f" | {sum(src[s].get(t, 0) for s in src for t in kinds)} |")
    cov = info["coverage"]
    L += ["", "每人上场局数 / 计分的两两比较次数（看覆盖度；补位、榜外的人" + ("" if has_base else f"、{base}") + "不算）：", "",
          "| 选手 | " + " | ".join(kinds) + " |", "|---|" + "---:|" * len(kinds)]
    for e in sc["entries"]:
        L.append(f"| {e} | " + " | ".join(f"{cov.get(t, {}).get(e, [0, 0])[0]} / {cov.get(t, {}).get(e, [0, 0])[1]}" for t in kinds) + " |")
    for rp in info.get("imports") or []:
        dropped = "、".join(f"{k} {v}" for k, v in rp["dropped"].items()) or "无"
        bad = [n for n, x in rp["names"].items() if not x["same"]]
        L.append(f"\n并入 {rp['source']}：读 {sum(f['rows'] for f in rp['files'])} 局，留 {rp['kept']} 局（所有榜合计），丢掉：{dropped}。"
                 f"选手包核对：{rp['kit']}；源码核对：" + ("全部一致" if not bad else "不一致：" + "、".join(bad)) + "。")
    L += ["", f"- 绑核：{env.get('cpus')}；12 人局 {env.get('cpus12')}。电源方案：{env.get('power')}。",
          f"- 每 CPU 毫秒周期数（裁判测定）：{env.get('per_ms')}。"]
    for k, v in (timing or {}).items():
        L.append(f"- 终场 {k}（两张榜合计）：{v['games']} 局，{v['seconds'] / 60:.1f} 分钟（这一次运行）")
    L += ["", f"回放在 `回放\\`（共用 1v1 在 `回放\\1v1\\`，本榜的多人局在 `回放\\{bd['name']}\\`，目录 `回放\\INDEX.md`），"
          f"逐局结果在 `results.jsonl`（终场新打）和 `预览并入.jsonl`，汇总在 `results.json` 的 `boards`，"
          f"给视频用的统计在 `视频统计_{bd['name']}.md`。", ""]
    if ref:
        L += ["## 附表：名次分（旧计分，只作参考，不定名次）", "",
              "每局名次归一成 (N − 名次) / (N − 1)（同回合死的取平均；补位照常占座），每个局型求平均、"
              f"减去锚点 {ref.get('anchor') or base} 的平均、×100，"
              f"再按同样的权重加权。区间是同样分层重抽样 {ref['boot']} 次的 95% 区间。", "",
              "| 选手 | 名次分总分 | 95% 区间 | 1v1 | 3～4 人 | 12 人 |", "|---|---:|---|---:|---:|---:|"]
        for e in ref["order"] + list(lines) + ([base] if base in ref["table"] else []):
            t = ref["table"][e]
            L.append(f"| {H.shown(cfg, e)} | {fmt(t['total'])} | {ci_text(t['total_ci'])} | "
                     + " | ".join(fmt(t["modes"].get(m, {}).get("score")) for m in MODES) + " |")
        L.append("")
    (out / f"RESULTS_{bd['name']}.md").write_text("\n".join(L), encoding="utf-8")


def write_video_md(cfg, out, vs, entries, filename="视频统计.md", title="给视频用的统计", note=None):
    L = [f"# {cfg['name']} · {title}", ""] + ([note, ""] if note else []) + [
         "按局型分开。存活回合 = 死前走了几回合（活到最后的按全局回合数）；存活比例 = 存活回合之和 / 所在对局回合数之和。"
         "撞死别人 = 有人撞上了他的轨迹（含他的头）；抢格胜负 = 同一回合争同一格，赢的人占下、输的人死。", ""]
    for m in list(MODES) + ["sparring"]:
        if not any(m in vs["per_entry"].get(e, {}) for e in entries):
            continue
        L += [f"## {MODE_TEXT.get(m, '对陪练（参考）')}", "",
              "| 选手 | 局数 | 独占第一 | 平均存活回合 | 存活比例 | 撞死别人 | 抢格胜 | 抢格负 | 死因分布 |",
              "|---|---:|---:|---:|---:|---:|---:|---:|---|"]
        for e in sorted(entries, key=lambda e: -vs["per_entry"].get(e, {}).get(m, {"alive_share": -1})["alive_share"]):
            o = vs["per_entry"].get(e, {}).get(m)
            if not o:
                continue
            causes = "、".join(f"{CAUSE_ZH.get(c, c)} {v}" for c, v in sorted(o["causes"].items(), key=lambda x: -x[1]))
            L.append(f"| {H.shown(cfg, e)} | {o['games']} | {o['wins']} | {o['avg_alive']} | {o['alive_share']:.1%} | "
                     f"{o['kills']} | {o['headon_won']} | {o['headon_lost']} | {causes} |")
        if m in vs["longest"]:
            lg = vs["longest"][m]
            L.append(f"\n最长的一局：{lg['turns']} 回合（{'、'.join(lg['seats'])}）" + (f"，回放 `{lg['replay']}`" if lg.get("replay") else ""))
        L.append("")
    L += ["## 局长", "", "| 类型 | 局数 | 平均回合 | 中位 | 最短 | 最长 |", "|---|---:|---:|---:|---:|---:|"]
    for t, v in vs["lengths"].items():
        L.append(f"| {t} | {v['games']} | {v['mean']} | {v['median']} | {v['min']} | {v['max']} |")
    (out / filename).write_text("\n".join(L) + "\n", encoding="utf-8")


def write_replay_index(out, rows):
    items = [r for r in rows if r.get("replay")]
    L = ["# 回放目录", "", f"共 {len(items)} 局。格式 tron-replay-1（与选手本地 arena.py play 存的相同）。", "",
         "| 文件 | 榜 | 类型 | 种子 | 回合 | 各座位（名次） |", "|---|---|---|---:|---:|---|"]
    for r in sorted(items, key=lambda r: r["replay"]):
        seats = "、".join(f"{s}({k + 1})" for s, k in zip(r["seats"], r["rank"]))
        L.append(f"| `{Path(r['replay']).as_posix()}` | {r.get('board') or '共用'} | {r['type']} | {r['seed']} | {r['turns']} | {seats} |")
    (out / "回放").mkdir(parents=True, exist_ok=True)
    (out / "回放" / "INDEX.md").write_text("\n".join(L) + "\n", encoding="utf-8")
    H.write_json(out / "回放" / "index.json", [{"file": r["replay"], "gid": r["gid"], "type": r["type"], "board": r.get("board"), "seed": r["seed"],
                                               "seats": r["seats"], "rank": r["rank"], "turns": r["turns"]} for r in items])


# ====================================================================
# 估时
# ====================================================================

DEFAULT_TURNS = {"1v1": 150, "3p": 100, "4p": 100, "12p": 170, "sparring": 150}


def estimate_table(cfg, counts, cpus, cpus12, turns=None, per_turn=None, first=None):
    """各局型的耗时（分钟）。turns：每局平均回合数；per_turn：每回合墙上毫秒（全员用满预算时）；first：首回合墙上秒。"""
    turns = {**DEFAULT_TURNS, **(turns or {})}
    out = {}
    for t, n in SIZE.items():
        c = cpus12 if t == "12p" else cpus
        w = workers_for(n, c)
        over = max(1.0, n / len(c)) if w == 1 else 1.0
        pt = (per_turn or {}).get(t, 51.0 * over)
        f = (first or {}).get(t, 0.2)
        out[t] = {"games": counts.get(t, 0), "workers": w, "turns": turns[t], "per_turn_ms": round(pt, 1),
                  "minutes": round(counts.get(t, 0) / w * (f + turns[t] * pt / 1000) / 60, 1)}
    return out
