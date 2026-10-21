"""赛事脚本共用的部分：读配置、路径、赛事日志、选手状态、文件创建时间、找选手解压出来的东西、消息模板。

从第 6 集（A05_坦克大战/主办方/hostlib.py）复制后精简：本期只有一个半场，没有中场、进步赛、陪跑、照搬。
所有路径都从赛事配置（JSON）里读，相对路径相对配置文件所在目录。字段见 赛事配置_示例.json 和 赛事层说明.md。
"""
from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import json
import os
import re
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent

NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,39}$")
KIT_SKIP = {"build", "history", "replays", "__pycache__", "_work"}

DEFAULTS = {
    "name": "光轮对战",
    "kit": "../workspace",
    "release_dir": "../发布",
    "prompt": "../提示词/开赛提示词.md",
    "messages": "../提示词/消息模板_v0.md",
    "trash_talk": None,
    "compiler": None,
    "baseline": "../workspace/bots/greedy.cpp",
    "sparring": "../workspace/bots/sparring_hard.exe",    # 参考表用强陪练（中等的 sparring.exe 也在包里）
    "substitute": "../workspace/submission/bot.cpp",     # 没交、编译不过时顶替上场的程序：选手包里的模板（能走就直走）
    "baseline_name": "greedy",
    "sparring_name": "sparring_hard",
    "target_minutes": 45,
    "limit_minutes": 60,
    "recheck_seconds": 60,
    "cpus": "auto",        # bot 用哪些逻辑处理器："auto" = 性能最高那类核、每个物理核取一个；或者直接给编号列表
    "max_cpus": 8,         # auto 时最多取几个
    "schedule": {},
    "weights": {"1v1": 0.5, "3-4": 0.3, "12": 0.2},
    "boot": 1000,
    "replays": {},
    "final_dir": "终场",   # 终场写到赛事目录下的哪个子目录（补打时换一个新目录名，旧终场原样不动、作为并入来源）
}
SCHEDULE_DEFAULTS = {
    "table_seed": 7007,    # 随机分桌、12 人局座位顺序用的种子（固定，可复现；公开也无妨）
    "map_seed": None,      # 地图种子的来源。None = 生成赛程时用系统随机数现抽（正式赛：赛前生成、不公开）；整数 = 由它推出（演练用）
    "1v1": {"maps": 10},   # 全体单循环，每对每张图换边各一局
    "4p": {"rounds": 20},  # 每轮随机分 4 人桌，每桌一张图、座位轮换 4 局
    "3p": {"rounds": 10},  # 每轮随机分 3 人桌，每桌一张图、座位轮换 3 局
    "12p": {"maps": 4},    # 12 人局：每张图座位全轮换 12 局
    "sparring": {"maps": 10},    # 参考表：每位参赛者 1v1 对陪练，每张图换边（不计分）
}
REPLAY_DEFAULTS = {
    "1v1_maps": 1,         # 1v1：前几张图上的全部对局存回放（每对 2 局 × 图数）
    "4p_rounds": 2,        # 4 人局：前几轮的全部对局
    "3p_rounds": 1,        # 3 人局：前几轮的全部对局
    "12p": True,           # 12 人局：全存
    "sparring_maps": 1,    # 对陪练：前几张图
    "faults": True,        # 另外：有人超时、崩溃、返回不合法值的对局都存
}
PATH_KEYS = ("work_dir", "event_dir", "players_root", "kit", "release_dir", "prompt", "messages", "trash_talk",
             "compiler", "baseline", "sparring", "substitute")
TOOLS = ("opencode", "codex", "human", "other")


class Stop(SystemExit):
    """停下并说明原因。"""

    def __init__(self, text):
        super().__init__(text)


# ====================================================================
# 小工具
# ====================================================================

def setup_console():
    for s in (sys.stdout, sys.stderr):
        try:
            s.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass


def is_ascii(path):
    return all(ord(ch) < 128 for ch in str(path))


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def replace_file(tmp, path):
    """原子替换；Windows 上目标文件正被别的进程读时偶尔失败，重试几次。"""
    for i in range(20):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            time.sleep(0.05 * (i + 1))
    os.replace(tmp, path)


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(f"tmp-{os.getpid()}-{path.name}")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")
    replace_file(tmp, path)


def append_line(path, record):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")


def read_lines(path):
    """读 jsonl；最后一行写了一半（进程被结束）时忽略它。"""
    out = []
    path = Path(path)
    if not path.is_file():
        return out
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return out


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def sha_file(path):
    return sha_bytes(Path(path).read_bytes())


def stamp(t=None):
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(time.time() if t is None else t))


def hms(epoch):
    """时:分:秒；不是今天的时刻前面带上月-日。"""
    if epoch is None:
        return "—"
    t = time.localtime(epoch)
    same_day = time.strftime("%Y%m%d", t) == time.strftime("%Y%m%d")
    return time.strftime("%H:%M:%S" if same_day else "%m-%d %H:%M:%S", t)


def minutes(seconds):
    return "—" if seconds is None else f"{seconds / 60:.1f} 分钟"


# ====================================================================
# 配置
# ====================================================================

def load_config(path):
    """读赛事配置。路径都换成绝对路径的 Path；players 是 [{"name", "folder", "display", "tool", "model", "external"}]。"""
    path = Path(path).resolve()
    if not path.is_file():
        raise Stop(f"找不到赛事配置：{path}")
    raw = read_json(path)
    cfg = {**DEFAULTS, **raw}
    sched = {**SCHEDULE_DEFAULTS, **(raw.get("schedule") or {})}
    for key in ("1v1", "4p", "3p", "12p", "sparring"):
        sched[key] = {**SCHEDULE_DEFAULTS[key], **((raw.get("schedule") or {}).get(key) or {})}
    cfg["schedule"] = sched
    cfg["replays"] = {**REPLAY_DEFAULTS, **(raw.get("replays") or {})}
    cfg["weights"] = {**DEFAULTS["weights"], **(raw.get("weights") or {})}
    for key in ("work_dir", "event_dir", "players_root", "players"):
        if not cfg.get(key):
            raise Stop(f"赛事配置里缺 {key}：{path}")
    base = path.parent
    for key in PATH_KEYS:
        cfg[key] = (base / cfg[key]).resolve() if cfg.get(key) else None
    for key in ("work_dir", "players_root"):
        if not is_ascii(cfg[key]):
            raise Stop(f"{key} 必须是纯英文路径（编译器和新编译的程序要在那里运行）：{cfg[key]}")
    official = {cfg["baseline_name"].lower(), cfg["sparring_name"].lower()}
    players, seen = [], set()
    for item in cfg["players"]:
        item = {"name": item} if isinstance(item, str) else dict(item)
        name = item.get("name", "")
        if not NAME_RE.match(name):
            raise Stop(f"选手名字只能用英文字母、数字、点、下划线、短横线（它会用作文件夹名）：{name!r}")
        if name.lower() in seen or name.lower() in official:
            raise Stop(f"选手名字重复，或者和基准、陪练重名：{name}")
        seen.add(name.lower())
        item.setdefault("folder", name)
        if not is_ascii(item["folder"]):
            raise Stop(f"选手文件夹名必须是纯英文：{item['folder']}")
        shown = item.get("display") or None
        if shown is not None and ("|" in shown or "\n" in shown):
            raise Stop(f"{name} 的显示名不能含竖线和换行：{shown!r}")
        item["display"] = shown.strip() if shown else None
        item["tool"] = item.get("tool") or "other"
        if item["tool"] not in TOOLS:
            raise Stop(f"{name} 的 tool 只能是 {'、'.join(TOOLS)}：{item['tool']!r}")
        item["model"] = item.get("model")
        item["external"] = bool(item.get("external")) or item["tool"] == "human"
        item["void"] = bool(item.get("void"))
        players.append(item)
    cfg["players"] = players
    cfg["filler_name"] = raw.get("filler_name") or f"{cfg['baseline_name']}_fill"
    if cfg["filler_name"].lower() in seen | official:
        raise Stop(f"filler_name 和选手、基准或陪练重名：{cfg['filler_name']}")
    cfg["leaderboards"] = parse_boards(cfg, raw.get("leaderboards"))
    # 计分锚点（定为 0 分）和基准计不计分：只对多张榜（leaderboards）生效，旧的单一成绩表仍以基准为 0
    cfg["anchor"] = raw.get("anchor") or cfg["baseline_name"]
    cfg["baseline_scored"] = bool(raw.get("baseline_scored", True))
    # 基准进不进共用 1v1 的单循环（默认进）。不计分时和它的 1v1 本来就不算，正赛配置把它拿掉省时间；多人局照样有它补位
    cfg["baseline_in_1v1"] = bool(raw.get("baseline_in_1v1", True))
    # 不计分的局型（例如 ["3p"]：不打 3 人局，3～4 人局型只用 4 人局；并入的预览 3 人局也不计）
    cfg["exclude_types"] = [str(t) for t in raw.get("exclude_types") or []]
    bad = [t for t in cfg["exclude_types"] if t not in ("1v1", "3p", "4p", "12p")]
    if bad:
        raise Stop(f"exclude_types 只能是 1v1、3p、4p、12p：{bad}")
    if cfg["leaderboards"]:
        if not cfg["baseline_scored"] and cfg["anchor"] == cfg["baseline_name"]:
            raise Stop("baseline_scored = false 时基准不计分，anchor 不能是基准，要指定一位选手（例如 \"Doubao\"）")
        for b in cfg["leaderboards"]:
            if cfg["anchor"] not in b["entries"]:
                raise Stop(f"锚点 {cfg['anchor']} 不在榜 {b['name']} 里（每张榜都要有锚点）")
    cfg["final_dir"] = str(cfg.get("final_dir") or "终场")
    if any(ch in cfg["final_dir"] for ch in '\\/:*?"<>|') or cfg["final_dir"] in (".", ".."):
        raise Stop(f"final_dir 只能是赛事目录下的一个目录名，不能含路径符号：{cfg['final_dir']!r}")
    # 补打（schedule.focus）：只新打和这几位有关的局（1v1 只打含他们的对子，3/4 人局每桌都有他们；12 人局照常全员）
    focus = sched.get("focus") or []
    if isinstance(focus, str):
        focus = [focus]
    if focus:
        sched["focus"] = [str(x) for x in focus]
    else:
        sched.pop("focus", None)
    if focus:
        if not cfg["leaderboards"]:
            raise Stop("schedule.focus（补打）只支持多张榜（leaderboards）的配置")
        bad = [n for n in sched["focus"] if n not in board_pool(cfg)]
        if bad:
            raise Stop(f"schedule.focus 里的 {bad} 不在任何榜里")
    # 共用 1v1 沿用一份已有赛程（例如之前的终场）的同一组图：schedule.1v1.seeds_from = 那份 赛程.json
    if sched["1v1"].get("seeds_from"):
        sched["1v1"]["seeds_from"] = str((base / sched["1v1"]["seeds_from"]).resolve())
    cfg["imports"] = parse_imports(cfg, raw.get("imports"), base)
    known = {f"{i['source']}/{Path(f).stem}" for i in cfg["imports"] for f in i["results"]}
    for b in cfg["leaderboards"] or []:
        bad = [k for k in b["import_multi"] or [] if k not in known]
        if bad:
            raise Stop(f"榜 {b['name']} 的 import_multi 里有 imports 中没有的结果：{bad}（可用的：{sorted(known)}）")
    cfg["config_path"] = path
    return cfg


BOARD_KINDS = ("4p", "3p", "12p")


def parse_boards(cfg, raw):
    """leaderboards：[{"name", "players": [...], "lines": [...], "line_labels": {名字: 叫法}, "note", "schedule": {4p/3p/12p}}]。

    - name 用在文件名里（RESULTS_<name>.md），不能有路径符号；
    - players 占名次；lines 正常参赛、参与拟合，但不占名次（参考线）；baseline（greedy）自动加进每张榜，定为 0 分；
    - 作废（"void": true）的选手不能出现在任何榜里；
    - 1v1 所有榜共用：所有榜的参赛者合起来单循环，每张榜只取榜内成员之间的对局；
    - 多人局每张榜各打各的，图数 / 轮数默认取顶层 schedule，可在榜里单独给。没有 leaderboards 时走旧的单一成绩表。"""
    if not raw:
        return None
    known = {p["name"]: p for p in cfg["players"]}
    out, names = [], set()
    for b in raw:
        name = str(b.get("name") or "").strip()
        if not name or any(ch in name for ch in '\\/:*?"<>|') or name in names:
            raise Stop(f"榜名为空、重复或含路径符号：{name!r}")
        names.add(name)
        players, lines = list(b.get("players") or []), list(b.get("lines") or [])
        if not players:
            raise Stop(f"榜 {name} 没有选手")
        if len(set(players + lines)) != len(players + lines):
            raise Stop(f"榜 {name} 里有重复的名字")
        for n in players + lines:
            if n == cfg["baseline_name"]:
                raise Stop(f"榜 {name}：{n} 是基准，自动在每张榜里，不用写")
            if n == cfg["sparring_name"] and n in lines:
                continue
            if n not in known:
                raise Stop(f"榜 {name} 里的 {n} 不在选手名单里（陪练只能放进 lines）")
            if known[n]["void"]:
                raise Stop(f"榜 {name} 里的 {n} 已作废（void），不能参加任何榜")
        sched = {k: {**cfg["schedule"][k], **(((b.get("schedule") or {}).get(k)) or {})} for k in BOARD_KINDS}
        labels = {cfg["sparring_name"]: "强档陪练线"}
        labels.update(b.get("line_labels") or {})
        out.append({"name": name, "note": b.get("note"), "players": players, "lines": lines,
                    "line_labels": {n: labels.get(n, "参考线") for n in lines}, "schedule": sched,
                    "entries": players + lines + [cfg["baseline_name"]],
                    "import_multi": None if b.get("import_multi") is None else [str(x) for x in b["import_multi"]]})
    return out


def parse_imports(cfg, raw, base):
    """imports：并入终场的已有对局（例如快速预览）。[{"source": 来源名, "results": [jsonl 文件或通配符], "bots": 源码目录,
    "kit": 当时用的选手包目录, "rename": {当时的名字: 现在的名字 或 "@filler"}}]。核对和筛选见 tourney.import_rows。

    并入一次已经打完的终场（正式模式）：{"source": 来源名, "final": 那个终场目录}。不用写 results / bots / kit：
    读那个目录里的 results.jsonl，选手包、编译器按其中的 清单.json 核对，各名字的源码按其中的 程序.json 核对；
    超时的局照收（正式模式已经判罚过）；多人局带着当时的榜名，只进同名的榜。"""
    if not raw:
        return []
    out = []
    for item in raw:
        src = str(item.get("source") or "")
        if not NAME_RE.match(src) or src == "final" or src in {x["source"] for x in out}:
            raise Stop(f"imports 的 source 只能用英文字母数字、不能重复、不能叫 final：{src!r}")
        if item.get("final"):
            fin = (base / item["final"]).resolve()
            out.append({"source": src, "final": fin, "results": [str(fin / "results.jsonl")], "bots": None, "kit": None,
                        "rename": dict(item.get("rename") or {}), "note": item.get("note")})
            continue
        out.append({"source": src, "results": [str((base / p)) for p in item.get("results") or []],
                    "bots": (base / item["bots"]).resolve(), "kit": (base / item["kit"]).resolve(),
                    "rename": dict(item.get("rename") or {}), "note": item.get("note")})
    return out


def pool_1v1(cfg):
    """共用 1v1 单循环的名单：board_pool，配置 baseline_in_1v1 = false 时去掉基准。"""
    pool = board_pool(cfg)
    return pool if cfg.get("baseline_in_1v1", True) else [n for n in pool if n != cfg["baseline_name"]]


def board_pool(cfg):
    """所有榜的参赛者合起来（共用 1v1 的单循环名单），按榜的顺序去重，基准放最后。"""
    pool = []
    for b in cfg["leaderboards"] or []:
        for n in b["players"] + b["lines"]:
            if n not in pool:
                pool.append(n)
    return pool + [cfg["baseline_name"]]


def player_names(cfg):
    return [p["name"] for p in cfg["players"]]


def active_names(cfg):
    """没有作废的选手（作废的不封存检查、不编译、不上场、不进任何榜）。有榜时只取至少在一张榜里的选手。"""
    names = [p["name"] for p in cfg["players"] if not p["void"]]
    if cfg.get("leaderboards"):
        pool = set(board_pool(cfg))
        names = [n for n in names if n in pool]
    return names


def get_player(cfg, name):
    for p in cfg["players"]:
        if p["name"] == name:
            return p
    raise Stop(f"名单里没有这位选手：{name}（名单：{'、'.join(player_names(cfg))}）")


def player_folder(cfg, name):
    return cfg["players_root"] / get_player(cfg, name)["folder"]


def event_path(cfg, *parts):
    return cfg["event_dir"].joinpath(*parts)


def shown(cfg, name):
    """成绩表里怎么写一位参赛者：“显示名（名字）”；没有显示名就是名字。"""
    for p in cfg["players"]:
        if p["name"] == name and p["display"]:
            return f"{p['display']}（{name}）"
    return name


def entry_names(cfg):
    """计分的参赛者（旧的单一成绩表）：没作废的选手 + 基准（greedy 作为最后一位）。"""
    return active_names(cfg) + [cfg["baseline_name"]]


# ====================================================================
# 日志与锁
# ====================================================================

def log(cfg, op, target=None, before=None, after=None, note=None, **extra):
    now = time.time()
    record = {"time": stamp(now), "epoch": round(now, 3), "op": op, "target": target, "before": before,
              "after": after, "note": note, **extra}
    append_line(event_path(cfg, "赛事日志.jsonl"), record)
    return record


@contextlib.contextmanager
def locked(cfg, wait=30.0):
    """改选手状态时加锁：常驻计时器的定时检查和人手动敲的命令可能同时发生。"""
    path = event_path(cfg, ".lock")
    path.parent.mkdir(parents=True, exist_ok=True)
    deadline = time.time() + wait
    while True:
        try:
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, f"{os.getpid()} {stamp()}".encode())
            os.close(fd)
            break
        except FileExistsError:
            try:
                if time.time() - path.stat().st_mtime > 120:
                    path.unlink()
                    continue
            except OSError:
                pass
            if time.time() > deadline:
                raise Stop(f"等了 {wait:.0f} 秒也没拿到锁 {path}。如果没有别的赛事命令在运行，删掉这个文件再试。")
            time.sleep(0.1)
    try:
        yield
    finally:
        try:
            path.unlink()
        except OSError:
            pass


# ====================================================================
# 文件创建时间（时钟就看它）
# ====================================================================

def birth_time(path):
    """文件在本机被创建（解压出来）的时刻，和选手包里 clock.py 的读法相同。"""
    st = Path(path).stat()
    return getattr(st, "st_birthtime", None) or st.st_ctime


def set_birth_time(path, epoch):
    """把文件的创建时间改成 epoch（只改创建时间）。只支持 Windows。"""
    if os.name != "nt":
        raise Stop("改文件创建时间只在 Windows 上实现了。")
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    k.CreateFileW.restype = wintypes.HANDLE
    k.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
                              wintypes.DWORD, wintypes.HANDLE]
    k.SetFileTime.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    k.SetFileTime.restype = wintypes.BOOL
    k.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = k.CreateFileW(str(path), 0x0100, 0x7, None, 3, 0x02000000, None)
    if handle in (None, wintypes.HANDLE(-1).value):
        raise Stop(f"打不开文件，改不了创建时间：{path}（错误码 {ctypes.get_last_error()}）")
    try:
        value = ctypes.c_uint64(int(round((epoch + 11644473600) * 10_000_000)))
        if not k.SetFileTime(handle, ctypes.byref(value), None, None):
            raise Stop(f"改创建时间失败：{path}（错误码 {ctypes.get_last_error()}）")
    finally:
        k.CloseHandle(handle)


# ====================================================================
# 选手文件夹里的东西：模型不一定按我们想的位置解压
# ====================================================================

WALK_SKIP = {"build", "history", "replays", "__pycache__", ".git", "node_modules", "judge", "bots"}


def _walk(folder, depth):
    level = [Path(folder)]
    for _ in range(depth + 1):
        deeper = []
        for d in level:
            yield d
            try:
                deeper += [x for x in d.iterdir() if x.is_dir() and x.name not in WALK_SKIP]
            except OSError:
                pass
        level = deeper


def find_workspaces(folder):
    """选手文件夹里解压出来的选手包（含 clock.py 和 arena.py 的目录），按解压时刻从早到晚。"""
    found = []
    if Path(folder).is_dir():
        for d in _walk(folder, 5):
            if (d / "clock.py").is_file() and (d / "arena.py").is_file():
                found.append(d)
    return sorted(found, key=lambda d: birth_time(d / "clock.py"))


def pick_workspace(folder):
    """选手实际在用的那份选手包：只有一份就是它；几份时取 submission/bot.cpp 最近改过的那份。返回 (目录或 None, 份数)。"""
    found = find_workspaces(folder)
    if not found:
        return None, 0

    def recent(d):
        bot = d / "submission" / "bot.cpp"
        return bot.stat().st_mtime if bot.is_file() else 0.0

    return max(found, key=recent), len(found)


# ====================================================================
# 选手状态（本期只有一个半场）
# ====================================================================

def state_path(cfg, name):
    return event_path(cfg, "选手", name, "状态.json")


def load_state(cfg, name):
    path = state_path(cfg, name)
    if path.is_file():
        return read_json(path)
    return {"name": name, "folder": str(player_folder(cfg, name)), "package": None, "start": None, "start_file": None,
            "first_seen": None, "adjustments": [], "snapshot": None, "seal": None, "voided": [], "noted": []}


def save_state(cfg, state):
    write_json(state_path(cfg, state["name"]), state)


def note_once(cfg, state, key, text, quiet=False):
    """同一件异常只记一次、报一次。"""
    if key in state["noted"]:
        return False
    state["noted"].append(key)
    log(cfg, "异常", state["name"], note=text)
    if not quiet:
        print(f"！{state['name']}：{text}", flush=True)
    return True


def sealed_path(cfg, name):
    return event_path(cfg, "选手", name, "封存_bot.cpp")


def any_started(cfg):
    """有没有选手已经开赛（解压了选手包、记了起点或有封存）。开赛之后不允许重新打包、换包。"""
    for name in player_names(cfg):
        path = state_path(cfg, name)
        if path.is_file():
            st = read_json(path)
            if st.get("start") is not None or st.get("seal") is not None:
                return True
        if cfg["players_root"].is_dir() and find_workspaces(player_folder(cfg, name)):
            return True
    return False


# ====================================================================
# 选手包副本与 arena.py（编译、运行都在 work_dir 里，纯英文路径）
# ====================================================================

def sync_kit(cfg):
    """把选手包源目录铺到 work_dir/kit。内容不同的文件覆盖，源里没有的删掉；build/ 不动。"""
    src, dst = cfg["kit"], cfg["work_dir"] / "kit"
    if not (src / "tron.hpp").is_file():
        raise Stop(f"选手包源目录不对（没有 tron.hpp）：{src}")
    wanted = set()
    for p in src.rglob("*"):
        rel = p.relative_to(src)
        if any(part in KIT_SKIP for part in rel.parts) or not p.is_file():
            continue
        out = dst / rel
        wanted.add(out)
        if not out.is_file() or out.read_bytes() != p.read_bytes():
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(p, out)
    if dst.is_dir():
        for p in dst.rglob("*"):
            rel = p.relative_to(dst)
            if p.is_file() and p not in wanted and not any(part in KIT_SKIP for part in rel.parts):
                p.unlink()
    return dst


_arena = {}


def arena(cfg):
    """work_dir/kit 里的 arena.py（编译 bot、裁判、算名次分都用它，和选手本地完全相同）。"""
    if "m" in _arena:
        return _arena["m"]
    dst = sync_kit(cfg)
    if cfg["compiler"]:
        os.environ["CXX"] = str(cfg["compiler"])
    spec = importlib.util.spec_from_file_location("tron_arena", dst / "arena.py")
    mod = importlib.util.module_from_spec(spec)
    old, sys.dont_write_bytecode = sys.dont_write_bytecode, True
    try:
        spec.loader.exec_module(mod)
    finally:
        sys.dont_write_bytecode = old
    _arena["m"] = mod
    return mod


_clock = {}


def clock_module(cfg):
    """选手包里的 clock.py：用它算"选手自己的时钟现在显示什么"。"""
    if "m" not in _clock:
        spec = importlib.util.spec_from_file_location("tron_clock", cfg["kit"] / "clock.py")
        mod = importlib.util.module_from_spec(spec)
        old, sys.dont_write_bytecode = sys.dont_write_bytecode, True
        try:
            spec.loader.exec_module(mod)
        finally:
            sys.dont_write_bytecode = old
        _clock["m"] = mod
    return _clock["m"]


# ====================================================================
# 消息模板
# ====================================================================

def text_block(path, section=None):
    """从 Markdown 里取四个反引号围起来的 text 块。section=None 取第一个；否则取 “## 第 section 节” 下的那个。"""
    if not path or not Path(path).is_file():
        raise Stop(f"找不到消息模板：{path}")
    text = Path(path).read_text(encoding="utf-8")
    if section is None:
        m = re.search(r"^````text\n(.*?)^````", text, re.S | re.M)
    else:
        m = re.search(rf"^## {section}\. .*?^````text\n(.*?)^````", text, re.S | re.M)
    if not m:
        raise Stop(f"{path} 里没有找到{'第 ' + str(section) + ' 节的' if section else ''}文本块")
    return m.group(1).rstrip("\n") + "\n"


def fill(template, **values):
    """把 {名字} 换成值。模板里还有没填的花括号时报错。"""
    need = set(re.findall(r"\{([^{}\n]{1,20})\}", template))
    left = sorted(need - set(values))
    if left:
        raise Stop(f"消息模板里有脚本不认识的占位：{left}")
    return re.sub(r"\{([^{}\n]{1,20})\}", lambda m: str(values[m.group(1)]), template)
