"""足球回放复盘工具（只列事实，不做分析、不给建议）。只用 Python 标准库。

用法（FILE 是回放文件，arena / match.py 生成在 replays/ 下）：
  python replay.py info    FILE                          概况：双方、比分、每个进球、踢球次数、故障统计
  python replay.py events  FILE [-t 类型,...] [-p 球员] [--from N] [--to M] [--page K]
                                                         事件列表（默认：开球、踢球、进球、重开、故障）
  python replay.py state   FILE N                        第 N 步开始时的完整状态表 + ASCII 小地图 + 本步 4 人实际执行的动作
  python replay.py around  FILE G [-b 20] [-a 3]         第 G 个进球前 b 步到后 a 步，逐步位置、动作、事件
  python replay.py player  FILE P [--from N] [--to M] [--page K]
                                                         P 号球员每步应有的观测（含队友消息）和实际执行的动作
  python replay.py msgs    FILE [--from N] [--to M] [--page K]
                                                         4 名球员每步发出的队友消息
  python replay.py png     FILE N [M] OUT.png            第 N 步（或 N..M 步抽 12 帧拼图）导出 PNG
  python replay.py gif     FILE N M OUT.gif [--every K]  N..M 步导出 GIF 动图（每 K 步一帧，默认 1）
  python replay.py verify  FILE                          按种子和动作重算，核对文件结构、步号、每步状态、事件行和终局摘要
                                                         （只核对物理重算与回放记录一致，不证明故障记录真实或文件未被篡改）
  python replay.py convert OLD.txt NEW.txt               把原型的旧格式（REPLAY 1）转成新格式（REPLAY 2）。
                                                         只接受 REPLAY 1，输入已是 REPLAY 2 时拒绝（退出码 2）；旧文件有 END 行时
                                                         先核对重算结果，不一致也拒绝。转出的文件带 "FAULTS unknown" 一行，
                                                         表示故障信息未知（旧格式不记录超时、非法输出、崩溃）

事件类型：kickoff 开球、kick 踢球、nokick 踢球无效、touch 触球、bump 相撞、wall 球撞墙、goal 进球、
          stall 久无踢球重开、fault 故障（超时 / 非法输出 / 崩溃等）。-t all 表示全部。
球员编号：0=A1 1=A2（A 队向右攻）  2=B1 3=B2（B 队向左攻）。也可以写 A1 / A2 / B1 / B2。
坐标一律是场地坐标；player 子命令给的是该球员自己的视角（自己向右进攻）。
动作行记录的是"实际执行的动作"（合法化之后；超时、不合法、程序已被结束的步都是 0 0 0 0 0），不是程序的原始输出。
player 列出的观测是"本步应有的观测"：由种子和动作重算，与程序收到的输入相同，前提是程序这一步还在运行。
程序被结束（崩溃、失联、累计超时、启动失败、输出超量）之后，裁判不再给它发输入，每步由裁判代填 0 0 0 0 0。
输出默认每页 40 行，末尾提示下一页的命令。
"""
import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soccer_engine as engine  # noqa: E402

PAGE = 40
EV_NAMES = {'kickoff': '开球', 'kick': '踢球', 'nokick': '踢球无效', 'touch': '触球', 'bump': '相撞',
            'wall': '球撞墙', 'goal': '进球', 'stall': '久无踢球重开', 'fault': '故障'}
WALL_NAMES = ['左墙', '右墙', '上墙', '下墙', '左上切角', '右上切角', '左下切角', '右下切角']
FAULT_NAMES = {'timeout': '超时（本步动作作废，按 0 0 0 0 0）', 'invalid': '输出不合法（本步按 0 0 0 0 0）',
               'crash': '程序退出 / 崩溃（本步起不再发送输入，裁判代填 0 0 0 0 0）',
               'lost': '失去响应，被结束（本步起不再发送输入，裁判代填 0 0 0 0 0）',
               'timeout_limit': '累计超时过多，被结束（本步起不再发送输入，裁判代填 0 0 0 0 0）',
               'start_fail': '程序启动失败（本步起不再发送输入，裁判代填 0 0 0 0 0）',
               'output_limit': '输出超量，被结束（本步起不再发送输入，裁判代填 0 0 0 0 0）'}
DEAD_KINDS = ('crash', 'lost', 'timeout_limit', 'start_fail', 'output_limit')   # F 行的 t 这一步起（含 t）不再询问
VERIFY_SCOPE = '验证范围：只核对物理重算与回放记录一致，不证明故障记录真实或文件未被篡改。'


def pname(p):
    return f"{'AB'[p // 2]}{p % 2 + 1}"


def parse_player(s):
    s = s.strip().upper()
    if s in ('A1', 'A2', 'B1', 'B2'):
        return 'AB'.index(s[0]) * 2 + int(s[1]) - 1
    return int(s)


# ---------------------------------------------------------------- 读与重算


def parse_val(v):
    if v == 'None':
        return None
    try:
        return int(v)
    except ValueError:
        return v


class Replay:
    def __init__(self, path):
        self.path = path
        self.names = ['?'] * 4
        self.actions = []
        self.action_t = []       # 动作行上写的步号（verify 核对是否从 0 连续）
        self.recorded = {}       # 新格式：t -> S 行记录的状态（重复的以第一条为准，另记进 dup_states）
        self.dup_states = []     # 重复出现的 S 行步号
        self.rec_events = []     # EV 行：(t, 类型, [整数参数...])
        self.faults = []         # (t, p, kind, detail)
        self.faults_unknown = False   # 有 "FAULTS unknown" 行（由旧格式转换而来）
        self.errors = []         # (行号, 说明)：读不懂的行
        self.version = 1
        self.limits = None
        self.end = None
        self.steps_line = None
        self.cfg = engine.make_cfg()
        self.seed, self.first_kick = 0, 0
        # 按字节读：回放里的故障说明可能含任意字节，UTF-8 解码失败的部分用替换字符
        with open(path, 'rb') as f:
            raw = f.read()
        for no, bl in enumerate(raw.split(b'\n'), 1):
            line = bl.decode('utf-8', errors='replace').rstrip('\r')
            if not line:
                continue
            try:
                self._parse_line(line)
            except (ValueError, IndexError):
                self.errors.append((no, line[:80]))
        if self.version < 2:
            self.faults_unknown = True
        self._sim()

    def _parse_line(self, line):
        if line.startswith('SOCCER2V2-REPLAY'):
            self.version = int(line.split()[1])
        elif line.startswith('CFG '):
            self.cfg = {k: parse_val(v) for k, v in (kv.split('=', 1) for kv in line[4:].split())}
        elif line.startswith('SEED '):
            self.seed = int(line.split()[1])
        elif line.startswith('FIRST_KICK '):
            self.first_kick = int(line.split()[1])
        elif line.startswith('PLAYER '):
            parts = line.split(maxsplit=4)
            self.names[int(parts[1])] = parts[4] if len(parts) > 4 else '?'
        elif line.startswith('LIMITS '):
            self.limits = tuple(float(x) for x in line.split()[1:3])
        elif line.startswith('STEPS '):
            self.steps_line = int(line.split()[1])
        elif line.startswith('FAULTS '):
            if line.split()[1] == 'unknown':
                self.faults_unknown = True
        elif line.startswith('END '):
            _, a, b, dg = line.split()[:4]
            self.end = (int(a), int(b), dg)
        elif line.startswith('S '):
            v = [int(x) for x in line.split()[1:]]
            if v[0] in self.recorded:
                self.dup_states.append(v[0])
            else:
                self.recorded[v[0]] = v
        elif line.startswith('EV '):
            parts = line.split()
            self.rec_events.append((int(parts[1]), parts[2], [int(x) for x in parts[3:]]))
        elif line.startswith('F '):
            parts = line.split(maxsplit=4)
            self.faults.append((int(parts[1]), int(parts[2]), parts[3], parts[4] if len(parts) > 4 else ''))
        elif line[0].isdigit():
            parts = line.split('|')
            acts = [tuple(int(v) for v in seg.split()) for seg in parts[1:5]]
            if len(acts) != 4 or any(len(a) != 5 for a in acts):
                raise ValueError('动作行不是 4 组 × 5 个整数')
            self.action_t.append(int(parts[0]))
            self.actions.append(acts)
        else:
            raise ValueError('不认识的行')

    def dead_from(self, p):
        """p 号程序从哪一步起被结束（不再发送输入，裁判代填 0）；没被结束返回 None。"""
        ts = [f[0] for f in self.faults if f[1] == p and f[2] in DEAD_KINDS]
        return min(ts) if ts else None

    def _sim(self):
        """重算整场：记录每步开始时的状态、各人收到的观测和事件。"""
        g = engine.Game(self.cfg, self.seed, self.first_kick, log=True)
        self.states = []
        self.obs = []
        for acts in self.actions:
            self.states.append(self._snap(g))
            self.obs.append([g.observe(p) for p in range(4)])
            g.step(acts)
        self.states.append(self._snap(g))
        self.final = g
        self.events = list(g.events)
        for t, p, kind, detail in self.faults:
            self.events.append(('fault', t, p, kind, detail))
        self.events.sort(key=lambda e: (e[1], 1 if e[0] == 'fault' else 0))   # 稳定排序：同一步内保持引擎顺序，故障排最后

    @staticmethod
    def _snap(g):
        return dict(t=g.t, score=tuple(g.score), ball=(g.bx, g.by, g.bvx, g.bvy),
                    pl=[(g.px[p], g.py[p], g.pvx[p], g.pvy[p], g.cd[p]) for p in range(4)], msg=tuple(g.msg))

    def state_ints(self, st):
        v = [st['t'], st['score'][0], st['score'][1], *st['ball']]
        for p in range(4):
            v += list(st['pl'][p])
        return v


# ---------------------------------------------------------------- 文字


def ev_text(e):
    k, t = e[0], e[1]
    if k == 'kickoff':
        who = '' if e[2] < 0 else f" {'AB'[e[2]]} 队开球"
        return f'{t:5d}  开球{who}  球在 ({e[3]}, {e[4]})'
    if k == 'kick':
        _, _, p, kx, ky, vx, vy, n = e
        same = f'  （本步 {n} 人同时踢，向量相加）' if n > 1 else ''
        return f'{t:5d}  踢球  {pname(p)}  踢球向量 ({kx:5d}, {ky:5d})  球速变为 ({vx:5d}, {vy:5d}){same}'
    if k == 'nokick':
        why = f'冷却中（剩 {e[4]} 步）' if e[3] == 'cooldown' else f'够不着（距离 {e[4]}）'
        return f'{t:5d}  踢球无效  {pname(e[2])}  {why}'
    if k == 'touch':
        bounce = '（反弹）' if len(e) > 3 and e[3] == 1 else ''
        return f'{t:5d}  触球  球与 {pname(e[2])} 重叠，被推开{bounce}'
    if k == 'bump':
        return f'{t:5d}  相撞  {pname(e[2])} 与 {pname(e[3])}'
    if k == 'wall':
        return f'{t:5d}  球撞墙  {WALL_NAMES[e[2]]}'
    if k == 'stall':
        return f'{t:5d}  连续 {e[2]} 步无人有效踢球，重新开球（比分不变）'
    if k == 'goal':
        return f'{t:5d}  进球  {"AB"[e[2]]} 队得分  比分 {e[4]}:{e[5]}  {goal_touch_text(e)}'
    if k == 'fault':
        return f'{t:5d}  故障  {pname(e[2])}  {FAULT_NAMES.get(e[3], e[3])}  {e[4]}'
    return str(e)


def ev_players(e):
    k = e[0]
    if k in ('kick', 'nokick', 'touch', 'fault'):
        return {e[2]}
    if k == 'bump':
        return {e[2], e[3]}
    if k == 'goal':
        return goal_touchers(e)
    return set()


def goal_touchers(e):
    """进球事件里最后一次触球的球员集合（有 mask 用 mask，否则用 last）。"""
    if len(e) > 6 and e[6] is not None:
        return {p for p in range(4) if e[6] >> p & 1}
    return {e[3]} if e[3] >= 0 else set()


def goal_touch_text(e):
    """进球的"最后触球"说明：同一步同一阶段多人触球时不分先后。"""
    if len(e) > 6 and e[6] is not None:
        ps = sorted(goal_touchers(e))
        if not ps:
            return '开球后无人触球'
        if len(ps) == 1:
            return f'最后触球 {pname(ps[0])}'
        return '最后一次触球为 ' + '、'.join(pname(p) for p in ps) + ' 同时'
    return f"最后触球 {pname(e[3]) if e[3] >= 0 else '-'}"


def paged(lines, page, hint):
    n = len(lines)
    pages = max(1, (n + PAGE - 1) // PAGE)
    page = min(max(1, page), pages)
    for ln in lines[(page - 1) * PAGE: page * PAGE]:
        print(ln)
    if pages > 1:
        print(f'-- 第 {page}/{pages} 页，共 {n} 行。看下一页：{hint} --page {page + 1}' if page < pages else
              f'-- 第 {page}/{pages} 页（末页），共 {n} 行 --')


def ascii_map(cfg, st, cols=60, rows=20):
    W, H = cfg['W'], cfg['H']
    gy0 = (H - cfg['GOAL']) // 2
    gy1 = gy0 + cfg['GOAL']
    C = cfg['CHAMFER']
    cw, rh = W / cols, H / rows
    grid = [[' '] * (cols + 2) for _ in range(rows + 2)]
    for r in range(rows + 2):
        for c in range(cols + 2):
            x = (c - 0.5) * cw
            y = (r - 0.5) * rh
            border = r in (0, rows + 1) or c in (0, cols + 1)
            if border:
                if c in (0, cols + 1) and 0 < r < rows + 1 and gy0 <= y <= gy1:
                    grid[r][c] = ']' if c == 0 else '['
                else:
                    grid[r][c] = '#'
            elif C and (x + y < C or (W - x) + y < C or x + (H - y) < C or (W - x) + (H - y) < C):
                grid[r][c] = '#'
            elif c == cols // 2 + 1:
                grid[r][c] = ':'
            else:
                grid[r][c] = '.'

    def put(x, y, ch):
        c = min(cols, max(1, int(x / cw) + 1))
        r = min(rows, max(1, int(y / rh) + 1))
        grid[r][c] = ch
    put(st['ball'][0], st['ball'][1], 'o')
    for p in range(4):
        put(st['pl'][p][0], st['pl'][p][1], 'ab'[p // 2] if p % 2 == 0 else 'AB'[p // 2])
    out = [''.join(row) for row in grid]
    out.append(f'图例：a=A1 A=A2（A 队向右攻）  b=B1 B=B2（B 队向左攻）  o=球  ]/[=球门  每格 {cw:.0f}×{rh:.0f}'
               '（同一格里后画的盖住先画的）')
    return out


def state_lines(rp, st):
    L = [f"第 {st['t']} 步开始时  比分 A {st['score'][0]} : {st['score'][1]} B  剩余 {rp.cfg['STEPS'] - st['t']} 步"]
    b = st['ball']
    L.append(f"{'':4s}{'x':>7s}{'y':>7s}{'vx':>7s}{'vy':>7s}{'冷却':>5s}{'离球':>7s}  上一步发出的消息")
    L.append(f"{'球':4s}{b[0]:7d}{b[1]:7d}{b[2]:7d}{b[3]:7d}")
    for p in range(4):
        x, y, vx, vy, cd = st['pl'][p]
        d = engine.isqrt((x - b[0]) ** 2 + (y - b[1]) ** 2)
        L.append(f"{pname(p):4s}{x:7d}{y:7d}{vx:7d}{vy:7d}{cd:5d}{d:7d}  {st['msg'][p]}")
    return L


def fmt_act(a):
    return f'{a[0]:4d}{a[1]:4d} {a[2]:5d}{a[3]:5d} {a[4]:3d}'


def fault_at(rp, t, p):
    return [f for f in rp.faults if f[0] == t and f[1] == p]


def step_notes(rp, t, p):
    """第 t 步 p 号程序的状况标注（只按回放里的 F 行列事实）。"""
    out = []
    for f in fault_at(rp, t, p):
        kind, detail = f[2], f[3]
        if kind == 'timeout':
            out.append('超时，动作作废' + (f'（{detail}）' if detail else ''))
        elif kind == 'invalid':
            out.append('不合法，按 0' + (f'（{detail}）' if detail else ''))
        else:
            out.append(FAULT_NAMES.get(kind, kind) + (f' {detail}' if detail else ''))
    d = rp.dead_from(p)
    if d is not None and t >= d and not any(f[2] in DEAD_KINDS for f in fault_at(rp, t, p)):
        out.append('已被结束，不再发送输入，裁判代填 0')
    return out


# ---------------------------------------------------------------- 图片（纯标准库：PNG / GIF 编码）

PALETTE = [
    (255, 255, 255),   # 0 背景
    (120, 190, 110),   # 1 草地
    (235, 245, 230),   # 2 白线
    (60, 70, 80),      # 3 墙 / 文字
    (40, 110, 220),    # 4 A 队
    (230, 90, 40),     # 5 B 队
    (250, 250, 250),   # 6 球
    (20, 20, 20),      # 7 黑
    (200, 200, 205),   # 8 门框外
    (255, 215, 60),    # 9 踢球闪光
    (150, 210, 140),   # 10 草地浅
    (90, 160, 250),    # 11 A 队浅
    (250, 150, 110),   # 12 B 队浅
]
FONT = {  # 3×5 点阵
    '0': '111101101101111', '1': '010110010010111', '2': '111001111100111', '3': '111001111001111',
    '4': '101101111001001', '5': '111100111001111', '6': '111100111101111', '7': '111001001001001',
    '8': '111101111101111', '9': '111101111001111', 'A': '010101111101101', 'B': '110101110101110',
    ':': '000010000010000', '-': '000000111000000', ' ': '000000000000000', 't': '010111010010011',
    '=': '000111000111000',
}


class Canvas:
    def __init__(self, w, h, bg=0):
        self.w, self.h = w, h
        self.px = bytearray([bg]) * (w * h)

    def rect(self, x0, y0, x1, y1, c):
        x0, y0 = max(0, int(x0)), max(0, int(y0))
        x1, y1 = min(self.w, int(x1)), min(self.h, int(y1))
        if x1 <= x0:
            return
        row = bytes([c]) * (x1 - x0)
        for y in range(y0, y1):
            self.px[y * self.w + x0: y * self.w + x1] = row

    def disc(self, cx, cy, r, c):
        r2 = r * r
        for y in range(int(cy - r), int(cy + r) + 1):
            dy = y - cy
            if dy * dy > r2 or not 0 <= y < self.h:
                continue
            dx = int((r2 - dy * dy) ** 0.5)
            self.rect(cx - dx, y, cx + dx + 1, y + 1, c)

    def text(self, x, y, s, c, k=2):
        for ch in s:
            g = FONT.get(ch, FONT[' '])
            for i, bit in enumerate(g):
                if bit == '1':
                    self.rect(x + (i % 3) * k, y + (i // 3) * k, x + (i % 3 + 1) * k, y + (i // 3 + 1) * k, c)
            x += 4 * k

    def paste(self, o, x, y):
        for r in range(o.h):
            self.px[(y + r) * self.w + x:(y + r) * self.w + x + o.w] = o.px[r * o.w:(r + 1) * o.w]


def draw_frame(cfg, st, trail, kicks, scale=0.05, label=''):
    W, H, C = cfg['W'], cfg['H'], cfg['CHAMFER']
    gy0 = (H - cfg['GOAL']) // 2
    gy1 = gy0 + cfg['GOAL']
    m = 24
    cw, ch = int(W * scale) + 2 * m, int(H * scale) + 2 * m + 14
    cv = Canvas(cw, ch, 0)
    oy = 14

    def X(x):
        return m + x * scale

    def Y(y):
        return oy + m + y * scale
    cv.rect(X(0) - 4, Y(0) - 4, X(W) + 4, Y(H) + 4, 3)          # 墙
    for yy in range(int(Y(0)), int(Y(H)) + 1):                   # 草地（切角）
        y = (yy - oy - m) / scale
        cut = max(0.0, C - y, C - (H - y)) if C else 0.0
        cv.rect(X(cut), yy, X(W - cut) + 1, yy + 1, 1 if (int(y) // 1000) % 2 else 10)
    cv.rect(X(W / 2) - 1, Y(0), X(W / 2) + 1, Y(H), 2)          # 中线
    cv.rect(X(0) - 12, Y(gy0), X(0), Y(gy1), 8)                  # 球门
    cv.rect(X(W), Y(gy0), X(W) + 12, Y(gy1), 8)
    for i, (x, y) in enumerate(trail):
        cv.disc(X(x), Y(y), 1.5, 2)
    for p in range(4):
        x, y = st['pl'][p][0], st['pl'][p][1]
        if p in kicks:
            cv.disc(X(x), Y(y), cfg['KICK_REACH'] * scale, 9)
        cv.disc(X(x), Y(y), cfg['RP'] * scale, 4 if p < 2 else 5)
        cv.disc(X(x), Y(y), cfg['RP'] * scale - 2, 11 if p < 2 else 12)
        cv.text(int(X(x)) - 7, int(Y(y)) - 5, pname(p), 7, 1 if scale < 0.04 else 2)
    bx, by = st['ball'][0], st['ball'][1]
    cv.disc(X(bx), Y(by), max(3, cfg['RB'] * scale), 7)
    cv.disc(X(bx), Y(by), max(2, cfg['RB'] * scale - 1.5), 6)
    cv.text(4, 3, f"t={st['t']}  {st['score'][0]}-{st['score'][1]}", 3)
    return cv


def frames_for(rp, n, m, every=1):
    T = len(rp.actions)
    out = []
    kick_at = {}
    for e in rp.events:
        if e[0] == 'kick':
            kick_at.setdefault(e[1], set()).add(e[2])
    for t in range(max(0, n), min(m, T) + 1, max(1, every)):
        st = rp.states[t]
        trail = [rp.states[j]['ball'][:2] for j in range(max(0, t - 15), t)]
        out.append((st, trail, kick_at.get(t, set())))
    return out


def write_png(path, cv):
    raw = b''.join(b'\x00' + bytes(cv.px[y * cv.w:(y + 1) * cv.w]) for y in range(cv.h))

    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    pal = b''.join(bytes(c) for c in PALETTE)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', cv.w, cv.h, 8, 3, 0, 0, 0)))
        f.write(chunk(b'PLTE', pal))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def lzw_gif(data, min_size=4):
    clear, eoi = 1 << min_size, (1 << min_size) + 1
    out = bytearray()
    bitbuf = nbits = 0

    def emit(code, size):
        nonlocal bitbuf, nbits
        bitbuf |= code << nbits
        nbits += size
        while nbits >= 8:
            out.append(bitbuf & 255)
            bitbuf >>= 8
            nbits -= 8
    size = min_size + 1
    table = {bytes([i]): i for i in range(clear)}
    nxt = eoi + 1
    emit(clear, size)
    w = b''
    for b in data:
        wc = w + bytes([b])
        if wc in table:
            w = wc
            continue
        emit(table[w], size)
        if nxt < 4096:
            table[wc] = nxt
            nxt += 1
            if nxt > (1 << size) and size < 12:
                size += 1
        else:
            emit(clear, size)
            table = {bytes([i]): i for i in range(clear)}
            nxt = eoi + 1
            size = min_size + 1
        w = bytes([b])
    if w:
        emit(table[w], size)
    emit(eoi, size)
    if nbits:
        out.append(bitbuf & 255)
    return bytes(out)


def write_gif(path, canvases, delay_cs=5):
    w, h = canvases[0].w, canvases[0].h
    pal = list(PALETTE) + [(0, 0, 0)] * (16 - len(PALETTE))
    with open(path, 'wb') as f:
        f.write(b'GIF89a' + struct.pack('<HHBBB', w, h, 0xF3, 0, 0))
        f.write(b''.join(bytes(c) for c in pal))
        f.write(b'\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00')
        for cv in canvases:
            f.write(b'\x21\xF9\x04\x00' + struct.pack('<H', delay_cs) + b'\x00\x00')
            f.write(b'\x2C' + struct.pack('<HHHHB', 0, 0, w, h, 0))
            f.write(b'\x04')
            d = lzw_gif(cv.px, 4)
            for i in range(0, len(d), 255):
                blk = d[i:i + 255]
                f.write(bytes([len(blk)]) + blk)
            f.write(b'\x00')
        f.write(b'\x3B')


# ---------------------------------------------------------------- 旧格式转换


def ev_record_form(e):
    """引擎事件 → 回放 EV 行的形式 (t, 类型, [参数])；回放不记录的类型返回 None。"""
    k = e[0]
    if k == 'kickoff':
        return (e[1], k, [e[2], e[3], e[4]])
    if k == 'kick':
        return (e[1], k, list(e[2:8]))
    if k == 'goal':
        return (e[1], k, list(e[2:7]))
    if k == 'stall':
        return (e[1], k, [e[2]])
    return None


def write_atomic(path, text):
    tmp = f'{path}.tmp{os.getpid()}'
    try:
        with open(tmp, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)


def convert(rp, out):
    """旧格式（REPLAY 1）→ REPLAY 2，带 "FAULTS unknown"。返回 (退出码, 说明)。"""
    if rp.version >= 2:
        return 2, (f'输入已是 REPLAY {rp.version} 格式，convert 只接受旧格式（REPLAY 1）；'
                   '新格式不需要转换（为免丢失 F、LIMITS 等故障记录，拒绝转换）')
    if rp.errors:
        return 1, f'输入有 {len(rp.errors)} 行读不懂（第一处在第 {rp.errors[0][0]} 行），不转换'
    if rp.end is None:
        return 1, '输入没有 END 行（可能没写完），无法确认按当前引擎重算的结果与原比赛相同，不转换'
    if rp.final.digest() != rp.end[2] or tuple(rp.final.score) != rp.end[:2]:
        return 1, (f'按当前引擎重算的终局与文件记录不同（重算 {rp.final.score[0]}:{rp.final.score[1]} {rp.final.digest()}，'
                   f'文件 {rp.end[0]}:{rp.end[1]} {rp.end[2]}），不转换')
    g = engine.Game(rp.cfg, rp.seed, rp.first_kick, log=True)
    L = ['SOCCER2V2-REPLAY 2', 'CFG ' + ' '.join(f'{k}={v}' for k, v in rp.cfg.items()), f'SEED {rp.seed}',
         f'FIRST_KICK {rp.first_kick}']
    for p in range(4):
        L.append(f"PLAYER {p} {'AB'[p // 2]} {p % 2 + 1} {rp.names[p]}")
    L.append(f"STEPS {rp.cfg['STEPS']}")
    L.append('FAULTS unknown')     # 旧格式不记录故障：故障信息未知（不是"没有故障"）

    def evs(lst):
        for e in lst:
            r = ev_record_form(e)
            if r:
                L.append(f'EV {r[0]} {r[1]} ' + ' '.join(str(x) for x in r[2]))

    def sline():
        v = [g.t, g.score[0], g.score[1], g.bx, g.by, g.bvx, g.bvy]
        for p in range(4):
            v += [g.px[p], g.py[p], g.pvx[p], g.pvy[p], g.cd[p]]
        return 'S ' + ' '.join(str(x) for x in v)
    evs(g.events)
    for t, acts in enumerate(rp.actions):
        L.append(sline())
        e0 = len(g.events)
        g.step(acts)
        L.append(f'{t} | ' + ' | '.join(' '.join(str(v) for v in a) for a in acts))
        evs(g.events[e0:])
    L.append(sline())
    L.append(f'END {g.score[0]} {g.score[1]} {g.digest()}')
    write_atomic(out, '\n'.join(L) + '\n')
    return 0, f'已写出 {out}（REPLAY 2 格式，带 FAULTS unknown：故障信息未知）'


# ---------------------------------------------------------------- 校验


def verify_problems(rp):
    """返回问题列表（空 = 一致）。只核对物理重算与回放记录，不核对故障记录的真实性。"""
    P = []
    n = len(rp.actions)
    if rp.version not in (1, 2):
        P.append(f'不认识的格式版本 REPLAY {rp.version}')
    for no, ln in rp.errors[:5]:
        P.append(f'第 {no} 行读不懂：{ln}')
    if len(rp.errors) > 5:
        P.append(f'另有 {len(rp.errors) - 5} 行读不懂')
    # 动作行步号
    bad_t = [i for i, t in enumerate(rp.action_t) if t != i]
    if bad_t:
        i = bad_t[0]
        P.append(f'动作行步号不是从 0 连续递增：第 {i + 1} 条动作行写的是第 {rp.action_t[i]} 步（应为 {i}），共 {len(bad_t)} 处')
    # 步数
    steps = rp.cfg.get('STEPS')
    if n != steps:
        P.append(f'部分回放/步数不符：动作行 {n} 步，CFG 的 STEPS={steps}')
    if rp.steps_line is not None and rp.steps_line != steps:
        P.append(f'STEPS 行 {rp.steps_line} 与 CFG 的 STEPS={steps} 不同')
    # S 行
    if rp.dup_states:
        P.append(f'S 行步号重复：{sorted(set(rp.dup_states))[:10]}')
    out_rng = sorted(t for t in rp.recorded if not 0 <= t <= n)
    if out_rng:
        P.append(f'S 行步号越界（应在 0..{n}）：{out_rng[:10]}')
    if rp.version >= 2:
        miss = [t for t in range(n + 1) if t not in rp.recorded]
        if miss:
            P.append(f'缺少 {len(miss)} 个 S 行，第一处是第 {miss[0]} 步（REPLAY 2 应有 0..{n} 每一步）')
    bad = [t for t, v in rp.recorded.items() if 0 <= t <= n and rp.state_ints(rp.states[t]) != v]
    if bad:
        P.append(f'有 {len(bad)} 步的记录状态与重算不同，第一处在第 {min(bad)} 步')
    # EV 行
    if rp.version >= 2 or rp.rec_events:
        calc = [r for r in (ev_record_form(e) for e in rp.final.events) if r]
        rec = rp.rec_events
        for i in range(max(len(calc), len(rec))):
            c = calc[i] if i < len(calc) else None
            r = rec[i] if i < len(rec) else None
            same = c is not None and r is not None and c[0] == r[0] and c[1] == r[1] and (
                c[2] == r[2] or (c[1] == 'goal' and len(r[2]) == 4 and c[2][:4] == r[2]))   # 旧 goal 行没有 mask
            if not same:
                fmt = (lambda x: '（无）' if x is None else f'EV {x[0]} {x[1]} ' + ' '.join(map(str, x[2])))
                P.append(f'EV 行与重算事件不一致：第 {i + 1} 条，文件 {fmt(r)}，重算 {fmt(c)}'
                         f'（文件 {len(rec)} 条，重算 {len(calc)} 条）')
                break
    # F 行（只查格式范围）
    badf = [f for f in rp.faults if not (0 <= f[0] <= n and 0 <= f[1] < 4)]
    if badf:
        P.append(f'F 行的步号或球员号越界：第一条为 F {badf[0][0]} {badf[0][1]} {badf[0][2]}')
    # END
    if rp.end is None:
        P.append('文件没有 END 行（可能没写完）')
    elif rp.final.digest() != rp.end[2] or tuple(rp.final.score) != rp.end[:2]:
        P.append(f'终局不一致：重算 {rp.final.score[0]}:{rp.final.score[1]} {rp.final.digest()}，'
                 f'文件记录 {rp.end[0]}:{rp.end[1]} {rp.end[2]}')
    return P


# ---------------------------------------------------------------- 命令


def main(argv=None):
    if hasattr(sys.stdout, 'reconfigure'):
        try:
            sys.stdout.reconfigure(encoding='utf-8')
        except Exception:
            pass
    ap = argparse.ArgumentParser(description='足球回放复盘工具（只列事实）。详细用法见文件开头的说明或 README.md')
    ap.add_argument('cmd', choices=['info', 'events', 'state', 'around', 'player', 'msgs', 'png', 'gif', 'verify',
                                    'convert'])
    ap.add_argument('file')
    ap.add_argument('args', nargs='*')
    ap.add_argument('-t', '--type', default='')
    ap.add_argument('-p', '--player', default=None)
    ap.add_argument('--from', dest='frm', type=int, default=0)
    ap.add_argument('--to', type=int, default=None)
    ap.add_argument('--page', type=int, default=1)
    ap.add_argument('-b', '--before', type=int, default=20)
    ap.add_argument('-a', '--after', type=int, default=3)
    ap.add_argument('--every', type=int, default=1)
    ap.add_argument('--scale', type=float, default=0.05, help='图片比例：每毫多少像素（默认 0.05，即 1 像素 = 20 毫）')
    o = ap.parse_args(argv)
    rp = Replay(o.file)
    T = len(rp.actions)
    to = T if o.to is None else min(T, o.to + 1)
    me = os.path.basename(sys.argv[0]) if sys.argv and sys.argv[0] else 'replay.py'
    base = f'python {me} {o.cmd} {o.file}'
    if rp.errors and o.cmd not in ('verify', 'convert'):
        print(f'注意：回放文件有 {len(rp.errors)} 行读不懂（第一处在第 {rp.errors[0][0]} 行），已跳过；用 verify 查看')

    if o.cmd == 'info':
        print(f"A 队（向右攻）: A1={rp.names[0]}  A2={rp.names[1]}")
        print(f"B 队（向左攻）: B1={rp.names[2]}  B2={rp.names[3]}")
        print(f"种子 {rp.seed}  第一次开球 {'AB'[rp.first_kick]} 队  步数 {T}  比分 A {rp.final.score[0]} : {rp.final.score[1]} B")
        if T != rp.cfg.get('STEPS'):
            print(f"部分回放：动作行 {T} 步，规定 {rp.cfg.get('STEPS')} 步")
        goals = [e for e in rp.events if e[0] == 'goal']
        for i, e in enumerate(goals, 1):
            print(f'  进球 {i}: ' + ev_text(e).strip())
        k = [sum(1 for e in rp.events if e[0] == 'kick' and e[2] == p) for p in range(4)]
        print('有效踢球次数: ' + '  '.join(f'{pname(p)} {k[p]}' for p in range(4)))
        st = sum(1 for e in rp.events if e[0] == 'stall')
        if st:
            print(f'久无踢球重开: {st} 次')
        if rp.faults:
            print('故障统计:')
            for p in range(4):
                fs = [f for f in rp.faults if f[1] == p]
                if fs:
                    cnt = {}
                    for f in fs:
                        cnt[f[2]] = cnt.get(f[2], 0) + 1
                    print(f'  {pname(p)}: ' + '  '.join(f'{FAULT_NAMES.get(k_, k_)} {v} 次' for k_, v in cnt.items()) +
                          f'  （第一次在第 {fs[0][0]} 步）')
        elif rp.faults_unknown:
            print('故障：未知（旧格式回放不记录）')
        else:
            print('故障: 无（回放里没有超时、非法输出、崩溃等故障记录）')
        dead = [(p, rp.dead_from(p)) for p in range(4) if rp.dead_from(p) is not None]
        for p, d in dead:
            print(f'  {pname(p)} 从第 {d} 步起已被结束，不再发送输入，裁判代填 0 0 0 0 0')
        print(f"下一步：python {me} events FILE 看事件；around FILE <进球序号> 看进球前后；state FILE <步> 看某一步")
    elif o.cmd == 'events':
        types = set(o.type.split(',')) - {''}
        rev = {v: k for k, v in EV_NAMES.items()}
        types = {rev.get(t, t) for t in types}
        if 'all' in types:
            types = set(EV_NAMES)
        if not types:
            types = {'kickoff', 'kick', 'goal', 'stall', 'fault'}
            print('（默认只列 开球、踢球、进球、重开、故障；用 -t all 看全部，或 -t kick,nokick,touch,bump,wall 自选）')
        pl = None if o.player is None else parse_player(o.player)
        lines = [ev_text(e) for e in rp.events
                 if e[0] in types and o.frm <= e[1] < to and (pl is None or pl in ev_players(e))]
        if not lines:
            print('（没有符合条件的事件）')
        hint = base + (f' -t {o.type}' if o.type else '') + (f' -p {o.player}' if o.player is not None else '') + \
            (f' --from {o.frm}' if o.frm else '') + (f' --to {o.to}' if o.to is not None else '')
        paged(lines, o.page, hint)
    elif o.cmd == 'state':
        if not o.args:
            print('用法：state FILE <步>')
            return
        n = max(0, min(int(o.args[0]), T))
        st = rp.states[n]
        for ln in state_lines(rp, st) + ascii_map(rp.cfg, st):
            print(ln)
        if n < T:
            print('本步各人实际执行的动作（各自视角：ax ay  kx ky  msg）: ' + ' | '.join(f'{pname(p)} ' + fmt_act(rp.actions[n][p]) for p in range(4)))
            for p in range(4):
                for s_ in step_notes(rp, n, p):
                    print(f'  {pname(p)} 本步：{s_}')
            evs = [ev_text(e).split(None, 1)[1] for e in rp.events if e[1] == n and e[0] not in ('fault',)]
            for s in evs:
                print('  本步事件：' + s)
    elif o.cmd == 'around':
        goals = [e for e in rp.events if e[0] == 'goal']
        if not o.args:
            print(f'用法：around FILE <进球序号 1..{len(goals)}>')
            return
        gi = int(o.args[0])
        if not 1 <= gi <= len(goals):
            print(f'只有 {len(goals)} 个进球')
            return
        e = goals[gi - 1]
        print(ev_text(e).strip())
        gt = e[1]
        lo, hi = max(0, gt - o.before), min(T, gt + 1 + o.after)
        print(f"{'步':>5s} {'球x':>6s}{'球y':>6s}{'vx':>5s}{'vy':>5s} | " + ' | '.join(
            f"{pname(p)}{'x':>5s}{'y':>6s} 动作ax ay  kx   ky msg" for p in range(4)))
        evs = {}
        for x in rp.events:
            if lo <= x[1] < hi and x[0] in ('kick', 'goal', 'kickoff', 'nokick', 'touch', 'fault', 'stall'):
                evs.setdefault(x[1], []).append(ev_text(x).split(None, 1)[1])
        for t in range(lo, hi):
            st = rp.states[t]
            b = st['ball']
            row = f'{t:5d} {b[0]:6d}{b[1]:6d}{b[2]:5d}{b[3]:5d} | ' + ' | '.join(
                f"{st['pl'][p][0]:7d}{st['pl'][p][1]:6d} " + fmt_act(rp.actions[t][p]) for p in range(4))
            print(row)
            for s in evs.get(t, []):
                print(f'{"":6s}↳ {s}')
        print('（位置是该步开始时；动作是该步各人实际执行的动作，各自视角：B 队的 ax、kx 与场地方向相反）')
    elif o.cmd == 'player':
        if not o.args:
            print('用法：player FILE <球员 0..3 或 A1/A2/B1/B2>')
            return
        p = parse_player(o.args[0])
        lines = [f"{pname(p)}（{rp.names[p]}）视角：自己向右攻。每行：步 比分 球(x y vx vy) 自己/队友/对手1/对手2(x y vx vy 冷却) "
                 "队友消息 → 实际执行的动作 ax ay kx ky msg",
                 "（观测是本步应有的观测，与程序收到的输入相同，前提是程序这一步还在运行；"
                 "动作是合法化之后实际执行的，超时、不合法、已被结束的步为 0 0 0 0 0）"]
        if rp.faults_unknown:
            lines.append('（故障信息未知：旧格式回放不记录，无法区分程序输出的 0 和裁判代填的 0）')
        for t in range(o.frm, to):
            ob = rp.obs[t][p]
            b = ob['ball']
            pls = ' '.join('(' + ' '.join(str(v) for v in q) + ')' for q in ob['pl'])
            mark = ''.join(f'  [{s_}]' for s_ in step_notes(rp, t, p))
            lines.append(f"{t:5d} 比分{ob['me_score']}:{ob['opp_score']} 球({b[0]} {b[1]} {b[2]} {b[3]}) {pls} 队友消息 {ob['mate_msg']}  →  " +
                         ' '.join(str(v) for v in rp.actions[t][p]) + mark)
        paged(lines, o.page, f'{base} {o.args[0]}' + (f' --from {o.frm}' if o.frm else '') + (f' --to {o.to}' if o.to is not None else ''))
    elif o.cmd == 'msgs':
        lines = [f"{'步':>5s}  " + '  '.join(f'{pname(p)}发出' for p in range(4)) +
                 '   （本步实际执行的消息，下一步队友收到；0..255；超时、不合法、已被结束的步为 0）']
        for t in range(o.frm, to):
            lines.append(f'{t:5d}  ' + '  '.join(f'{rp.actions[t][p][4]:6d}' for p in range(4)))
        paged(lines, o.page, base + (f' --from {o.frm}' if o.frm else '') + (f' --to {o.to}' if o.to is not None else ''))
    elif o.cmd == 'png':
        if len(o.args) < 2:
            print('用法：png FILE N [M] OUT.png')
            return
        n = int(o.args[0])
        m = int(o.args[1]) if len(o.args) >= 3 else n
        out = o.args[-1]
        fr = frames_for(rp, n, m)
        if len(fr) > 12:
            step = (len(fr) + 11) // 12
            fr = fr[::step][:12]
        cvs = [draw_frame(rp.cfg, *f, scale=o.scale) for f in fr]
        if len(cvs) == 1:
            sheet = cvs[0]
        else:
            w, h = cvs[0].w, cvs[0].h
            cols = 3
            sheet = Canvas(w * cols, h * ((len(cvs) + cols - 1) // cols), 0)
            for i, c in enumerate(cvs):
                sheet.paste(c, (i % cols) * w, (i // cols) * h)
        write_png(out, sheet)
        print(f'已导出 {out}（{len(cvs)} 帧，{sheet.w}×{sheet.h}）')
    elif o.cmd == 'gif':
        if len(o.args) < 3:
            print('用法：gif FILE N M OUT.gif [--every K]')
            return
        n, m, out = int(o.args[0]), int(o.args[1]), o.args[2]
        fr = frames_for(rp, n, m, o.every)
        if len(fr) > 400:
            print(f'帧数太多（{len(fr)}），请缩小范围或加 --every')
            return
        cvs = [draw_frame(rp.cfg, *f, scale=o.scale) for f in fr]
        write_gif(out, cvs, delay_cs=max(2, 5 * o.every))
        print(f'已导出 {out}（{len(cvs)} 帧）')
    elif o.cmd == 'verify':
        probs = verify_problems(rp)
        for x in probs:
            print(x)
        print('一致' if not probs else '不一致')
        print(VERIFY_SCOPE)
        sys.exit(0 if not probs else 1)
    elif o.cmd == 'convert':
        if not o.args:
            print('用法：convert OLD.txt NEW.txt')
            sys.exit(2)
        code, msg = convert(rp, o.args[0])
        print(msg)
        sys.exit(code)


if __name__ == '__main__':
    main()
