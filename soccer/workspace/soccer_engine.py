"""2v2 简化足球引擎的 Python 版（复盘工具 replay.py 用它重算整场比赛）。

规则以 C++ 引擎 soccer.hpp 为准；本文件与 soccer.hpp 经过逐步逐位对拍（1 万局 + 20 万个随机局面全部一致）。
唯一的差别：C++ 裁判把绝对值超过 10^9 的输出整行判为不合法；本文件不检查（回放里记录的动作已经是合法化之后的）。

坐标：x 向右增大，y 向下增大，单位"毫"（场地 12000 × 8000）。
球员 0、1 是 A 队，开局在左半场、向右进攻；球员 2、3 是 B 队，开局在右半场、向左进攻。
（换边由赛程负责：让两支队交换 A/B 身份即可。）

整数约定（C++ 照抄即可逐位一致）：
  tdiv(a, b)  b > 0，向零取整的整数除法（C/C++ 的 a / b）。
  isqrt(n)    n >= 0，向下取整的平方根。
  全部中间量用 64 位有符号整数，不会溢出。

每步结算顺序（step）：
  1. 读入 4 名球员的动作 (ax, ay, kx, ky, msg)，均已换算回场地坐标。
  2. 踢球：球员与球的圆心距 <= KICK_REACH，冷却为 0，且 (kx, ky) != (0, 0) 时有效。
     每个有效踢球向量先限长到 KICK_MAX，所有有效踢球向量相加，再限长到 KICK_MAX，
     直接成为球的新速度。有效踢球的球员冷却置为 KICK_COOLDOWN，且本步末尾不减
     （所以踢球后接下来 KICK_COOLDOWN 步都不能踢）。
  3. 球员加速：(ax, ay) 限长到 ACC_MAX，加到速度上。
  4. 移动：球员和球的位置都加上各自速度。
  5. 碰撞：球员与球员 → 球员与墙 → 球与球员 → 进球判定 → 球与墙。
     "球员与球员""球与球员"两类都是同时结算：所有修正量由这一类开始前的同一状态算出再相加，
     与编号顺序无关，所以左右镜像严格对称。
     两名球员圆心完全重合时的分离方向（只用镜像不变的量）：
       a) 按两人本步移动前的相对位置分开（= 相对速度的反方向）；
       b) 若移动前也重合（相对速度为 0）：不同队 → 沿 x 方向各自退向本方球门；同队 → 队内 0 号向上、1 号向下。
  6. 摩擦：球员速度 ×PF_N/PF_D，球速度 ×BF_N/BF_D（都向零取整）。冷却减 1（不低于 0；本步刚踢球的人不减）。
  7. 步数 +1。若本步进球：比分 +1，全部回到开球站位。
     否则若连续 STALL 步没有任何有效踢球（从上次有效踢球或开球算起）：比分不变，回到开球站位。
     到 STEPS 步比赛结束。

限长 clamp(x, y, m)：若 x²+y² > m²，d = isqrt(x²+y²)，(x, y) ← (tdiv(x·m, d), tdiv(y·m, d))。
"""
import hashlib

SPLITMIX_MASK = (1 << 64) - 1


def tdiv(a, b):
    """向零取整的整数除法，b > 0。"""
    q = abs(a) // b
    return q if a >= 0 else -q


def isqrt(n):
    import math
    return math.isqrt(n)


def clamp(x, y, m):
    s = x * x + y * y
    if s > m * m:
        d = isqrt(s)
        return tdiv(x * m, d), tdiv(y * m, d)
    return x, y


class SplitMix64:
    """开局扰动用的随机数（规则里写死，C++ 照抄）。"""

    def __init__(self, seed):
        self.s = seed & SPLITMIX_MASK

    def next(self):
        self.s = (self.s + 0x9E3779B97F4A7C15) & SPLITMIX_MASK
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & SPLITMIX_MASK
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & SPLITMIX_MASK
        return z ^ (z >> 31)

    def jitter(self, j):
        """[-j, j] 内的整数。"""
        return self.next() % (2 * j + 1) - j


DEFAULT = dict(
    W=12000, H=8000,
    GOAL=2000,            # 球门宽（两侧墙正中开口）
    CHAMFER=1000,         # 四角切角（45° 斜墙，两条直角边各 CHAMFER）；0 表示直角
    RP=300, RB=150,       # 球员半径、球半径
    ACC_MAX=40,           # 每步加速度上限
    PF_N=9, PF_D=10,      # 球员摩擦：从静止沿坐标轴满加速，整数递推下第 10 步移动 258、第 40 步累计 12318，极限每步移动 391（保存速度 351）
    BF_N=97, BF_D=100,    # 球摩擦
    KICK_MAX=800,         # 踢球速度上限
    KICK_REACH=500,       # 可踢距离（圆心距，= RP + RB + 50）
    STALL=300,            # 连续这么多步没有任何有效踢球 → 重新开球（比分不变）；0 表示关闭
    KICK_COOLDOWN=5,      # 踢球后冷却步数（防"贴身对踢"僵局，见实验报告）
    E_WALL_N=4, E_WALL_D=5,     # 球撞墙恢复系数 0.8
    E_BALL_N=1, E_BALL_D=2,     # 球撞球员恢复系数 0.5（球员视为质量无穷大）
    E_PP_N=1, E_PP_D=2,         # 球员互撞恢复系数 0.5（等质量）
    STEPS=1200,           # 一场的步数
    KICKOFF='conceder',   # 'conceder' 失球方开球（第一次由 first_kick 队开）；'sym' 双方对称站位、无开球方
    JITTER=600,           # 开球站位扰动幅度（y 方向，左右镜像）
    KO_FRONT=1500,        # 前锋离中线的距离
    KO_BACK=2000,         # 后卫离本方底线的距离
    MSG_MAX=255,          # 队友消息取值 0..MSG_MAX（0 表示不发）；None 表示关闭通信
)


def make_cfg(**kw):
    c = dict(DEFAULT)
    for k, v in kw.items():
        assert k in c, k
        c[k] = v
    return c


class Game:
    def __init__(self, cfg=None, seed=0, first_kick=0, log=False):
        self.cfg = cfg = cfg or make_cfg()
        self.log = log
        self.events = []                  # log=True 时记录事件（复盘用，不影响结算）
        self.W, self.H = cfg['W'], cfg['H']
        self.GY0 = (self.H - cfg['GOAL']) // 2
        self.GY1 = self.GY0 + cfg['GOAL']
        self.rng = SplitMix64(seed)
        self.seed = seed
        self.score = [0, 0]
        self.t = 0
        self.over = False
        self.msg = [0, 0, 0, 0]          # 上一步各人发出的消息
        self.cd = [0, 0, 0, 0]
        self.kick_team = first_kick       # 'conceder' 模式下下一次开球的队
        self.goals = []                   # (步数, 进球队)
        self.kicks = [0, 0, 0, 0]
        self.touch_mask = 0               # 最后一次触球（同一步同一阶段）的全部球员，位掩码；开球时清零（统计用）
        self.invalid = [0, 0, 0, 0]
        self.last_kick_t = 0              # 最近一次有效踢球（或开球）的步数
        self.stalls = 0
        W, H, C = self.W, self.H, cfg['CHAMFER']
        # 墙：a·x + b·y >= c；球在 x 方向两面墙上若圆心在球门开口内则不碰
        self.walls = [(1, 0, 0), (-1, 0, -W), (0, 1, 0), (0, -1, -H)]
        if C > 0:
            self.walls += [(1, 1, C), (-1, 1, C - W), (1, -1, C - H), (-1, -1, C - W - H)]
        self.kickoff()

    # ---------------- 开球 ----------------
    def kickoff(self):
        c = self.cfg
        W, H = self.W, self.H
        db = self.rng.jitter(c['JITTER'])
        d1 = self.rng.jitter(c['JITTER'])
        d2 = self.rng.jitter(c['JITTER'])
        self.bx, self.by, self.bvx, self.bvy = W // 2, H // 2 + db, 0, 0
        front = [W // 2 - c['KO_FRONT'], H // 2 + d1]
        back = [c['KO_BACK'], H // 2 + d2]
        pos = [None] * 4
        for team in (0, 1):
            f, b = list(front), list(back)
            if c['KICKOFF'] == 'conceder' and team == self.kick_team:
                f = [W // 2 - (c['RP'] + c['RB'] + 50), H // 2 + db]
            if team == 1:
                f[0] = W - f[0]
                b[0] = W - b[0]
            pos[2 * team] = f
            pos[2 * team + 1] = b
        self.px = [p[0] for p in pos]
        self.py = [p[1] for p in pos]
        self.pvx = [0] * 4
        self.pvy = [0] * 4
        self.cd = [0, 0, 0, 0]
        self.touch_mask = 0
        self.last_kick_t = self.t
        if self.log:
            kt = self.kick_team if c['KICKOFF'] == 'conceder' else -1
            self.events.append(('kickoff', self.t, kt, self.bx, self.by))

    # ---------------- 观测（换算成"自己向右进攻"） ----------------
    def observe(self, p):
        team = p // 2
        W = self.W
        fx = (lambda x: W - x) if team == 1 else (lambda x: x)
        fv = (lambda v: -v) if team == 1 else (lambda v: v)
        order = [p, p ^ 1] + [q for q in range(4) if q // 2 != team]
        return dict(
            t=self.t, left=self.cfg['STEPS'] - self.t,
            me_score=self.score[team], opp_score=self.score[1 - team],
            ball=(fx(self.bx), self.by, fv(self.bvx), self.bvy),
            pl=[(fx(self.px[q]), self.py[q], fv(self.pvx[q]), self.pvy[q], self.cd[q]) for q in order],
            mate_msg=self.msg[p ^ 1],
            idx=p % 2,
        )

    # ---------------- 一步 ----------------
    def step(self, actions):
        """actions[p] = (ax, ay, kx, ky, msg)，球员 p 自己视角（向右进攻）的坐标。非法项视为全 0。"""
        assert not self.over
        c = self.cfg
        acts = []
        for p in range(4):
            a = actions[p]
            try:
                ax, ay, kx, ky, m = (int(v) for v in a)
            except Exception:
                self.invalid[p] += 1
                ax = ay = kx = ky = m = 0
            if p // 2 == 1:
                ax, kx = -ax, -kx
            if c['MSG_MAX'] is None or not (0 <= m <= c['MSG_MAX']):
                m = 0
            acts.append((ax, ay, kx, ky, m))
        # 2. 踢球
        reach2 = c['KICK_REACH'] ** 2
        sx = sy = 0
        kicked = False
        kickers = []
        for p in range(4):
            ax, ay, kx, ky, m = acts[p]
            if kx or ky:
                dx, dy = self.bx - self.px[p], self.by - self.py[p]
                if self.cd[p] > 0:
                    if self.log:
                        self.events.append(('nokick', self.t, p, 'cooldown', self.cd[p]))
                    continue
                if dx * dx + dy * dy <= reach2:
                    kx, ky = clamp(kx, ky, c['KICK_MAX'])
                    sx += kx
                    sy += ky
                    kicked = True
                    kickers.append((p, kx, ky))
                    self.cd[p] = c['KICK_COOLDOWN']
                    self.kicks[p] += 1
                elif self.log:
                    self.events.append(('nokick', self.t, p, 'far', isqrt(dx * dx + dy * dy)))
        kicked_now = {p for p, _, _ in kickers}
        if kicked:
            self.last_kick_t = self.t + 1
            self.touch_mask = sum(1 << p for p in kicked_now)
            self.bvx, self.bvy = clamp(sx, sy, c['KICK_MAX'])
            if self.log:
                for p, kx, ky in kickers:
                    self.events.append(('kick', self.t, p, kx, ky, self.bvx, self.bvy, len(kickers)))
        # 3. 加速
        for p in range(4):
            ax, ay = clamp(acts[p][0], acts[p][1], c['ACC_MAX'])
            self.pvx[p] += ax
            self.pvy[p] += ay
        # 4. 移动
        for p in range(4):
            self.px[p] += self.pvx[p]
            self.py[p] += self.pvy[p]
        self.bx += self.bvx
        self.by += self.bvy
        # 5. 碰撞
        self._players_collide()
        for p in range(4):
            self._wall(p)
        self._ball_players()
        scored = -1
        if self.GY0 <= self.by <= self.GY1:
            if self.bx < 0:
                scored = 1
            elif self.bx > self.W:
                scored = 0
        if scored < 0:
            self._wall(-1)
        # 6. 摩擦、冷却、消息
        for p in range(4):
            self.pvx[p] = tdiv(self.pvx[p] * c['PF_N'], c['PF_D'])
            self.pvy[p] = tdiv(self.pvy[p] * c['PF_N'], c['PF_D'])
            if self.cd[p] > 0 and p not in kicked_now:
                self.cd[p] -= 1
        self.bvx = tdiv(self.bvx * c['BF_N'], c['BF_D'])
        self.bvy = tdiv(self.bvy * c['BF_N'], c['BF_D'])
        self.msg = [a[4] for a in acts]
        # 7. 进球与结束
        self.t += 1
        if scored >= 0:
            self.score[scored] += 1
            self.goals.append((self.t, scored))
            if self.log:
                m = self.touch_mask
                last = m.bit_length() - 1 if m and m & (m - 1) == 0 else -1
                self.events.append(('goal', self.t - 1, scored, last, self.score[0], self.score[1], m))
            self.kick_team = 1 - scored
            self.kickoff()
        elif c['STALL'] and self.t - self.last_kick_t >= c['STALL'] and self.t < c['STEPS']:
            # 久无有效踢球：重新开球。'conceder' 模式下由刚才离球更远的队……为简单起见沿用上次开球队
            self.stalls += 1
            if self.log:
                self.events.append(('stall', self.t, c['STALL']))
            self.kickoff()
        if self.t >= c['STEPS']:
            self.over = True
        return scored

    # ---------------- 碰撞细节 ----------------
    def _players_collide(self):
        """球员两两碰撞。所有修正量都由碰撞前的同一状态算出，最后一起加上（与编号顺序无关）。"""
        c = self.cfg
        R = 2 * c['RP']
        en, ed = c['E_PP_N'], c['E_PP_D']
        X, Y, VX, VY = list(self.px), list(self.py), list(self.pvx), list(self.pvy)
        for a in range(4):
            for b in range(a + 1, 4):
                dx, dy = X[b] - X[a], Y[b] - Y[a]
                d2 = dx * dx + dy * dy
                if d2 >= R * R:
                    continue
                if d2 == 0:
                    # 圆心完全重合：分离方向只由镜像不变的量决定（见文件开头第 5 条）
                    pen = R + 1
                    dx, dy = VX[a] - VX[b], VY[a] - VY[b]   # 移动前 b 相对 a 的位置 = −(相对速度)
                    if dx == 0 and dy == 0:
                        if a // 2 != b // 2:
                            dx = 1      # a 属 A 队（编号小）：a 退向左（本方门），b 退向右
                        else:
                            dy = 1      # 同队：a 是队内 0 号，向上；b 向下
                    d2 = dx * dx + dy * dy
                else:
                    pen = R - isqrt(d2) + 1
                d = isqrt(d2)
                if self.log:
                    self.events.append(('bump', self.t, a, b))
                sx, sy = tdiv(pen * dx, 2 * d), tdiv(pen * dy, 2 * d)
                self.px[a] -= sx
                self.py[a] -= sy
                self.px[b] += sx
                self.py[b] += sy
                rvx, rvy = VX[b] - VX[a], VY[b] - VY[a]
                dot = rvx * dx + rvy * dy
                if dot < 0:
                    jx = tdiv((ed + en) * dot * dx, 2 * ed * d2)
                    jy = tdiv((ed + en) * dot * dy, 2 * ed * d2)
                    self.pvx[a] += jx
                    self.pvy[a] += jy
                    self.pvx[b] -= jx
                    self.pvy[b] -= jy

    def _ball_players(self):
        """球与 4 名球员：每名重叠球员的修正都由同一个碰撞前的球状态算出，再相加。"""
        c = self.cfg
        R = c['RP'] + c['RB']
        en, ed = c['E_BALL_N'], c['E_BALL_D']
        bx, by, bvx, bvy = self.bx, self.by, self.bvx, self.bvy
        touched = 0
        for p in range(4):
            dx, dy = bx - self.px[p], by - self.py[p]
            d2 = dx * dx + dy * dy
            if d2 >= R * R:
                continue
            if d2 == 0:
                dx, dy, d2 = 0, 1, 1
            d = isqrt(d2)
            pen = R - d + 1
            rvx, rvy = bvx - self.pvx[p], bvy - self.pvy[p]
            dot = rvx * dx + rvy * dy
            if self.log:
                self.events.append(('touch', self.t, p, 1 if dot < 0 else 0))   # 1 = 有反弹冲量，0 = 只修正位置
            touched |= 1 << p
            self.bx += tdiv(pen * dx, d)
            self.by += tdiv(pen * dy, d)
            if dot < 0:
                self.bvx -= tdiv((ed + en) * dot * dx, ed * d2)
                self.bvy -= tdiv((ed + en) * dot * dy, ed * d2)
        if touched:
            self.touch_mask = touched

    def _wall(self, p):
        """p >= 0：球员（恢复系数 0，法向速度清零）；p = -1：球。"""
        c = self.cfg
        if p >= 0:
            r = c['RP']
            x, y, vx, vy = self.px[p], self.py[p], self.pvx[p], self.pvy[p]
            en, ed = 0, 1
        else:
            r = c['RB']
            x, y, vx, vy = self.bx, self.by, self.bvx, self.bvy
            en, ed = c['E_WALL_N'], c['E_WALL_D']
        rd = isqrt(2 * r * r)
        for i, (a, b, cc) in enumerate(self.walls):
            if p < 0 and i < 2 and self.GY0 <= y <= self.GY1:
                continue   # 球门开口
            n2 = a * a + b * b
            need = r if n2 == 1 else rd
            pen = need - (a * x + b * y - cc)
            if pen <= 0:
                continue
            if n2 == 1:
                x += a * pen
                y += b * pen
            else:
                s = (pen + 1) // 2
                x += a * s
                y += b * s
            dot = a * vx + b * vy
            if dot < 0:
                vx -= tdiv((ed + en) * dot * a, ed * n2)
                vy -= tdiv((ed + en) * dot * b, ed * n2)
                if self.log and p < 0:
                    self.events.append(('wall', self.t, i))
        if p >= 0:
            self.px[p], self.py[p], self.pvx[p], self.pvy[p] = x, y, vx, vy
        else:
            self.bx, self.by, self.bvx, self.bvy = x, y, vx, vy

    # ---------------- 工具 ----------------
    def state(self):
        return (self.t, tuple(self.score), self.bx, self.by, self.bvx, self.bvy,
                tuple(self.px), tuple(self.py), tuple(self.pvx), tuple(self.pvy), tuple(self.cd), tuple(self.msg))

    def digest(self):
        return hashlib.sha1(repr(self.state()).encode()).hexdigest()[:16]


def predict_ball(cfg, bx, by, bvx, bvy, n, GY=None):
    """不考虑球员时，球在后续 n 步的位置（与引擎逐位一致，bot 用）。
    返回列表 [(x, y, vx, vy), ...]（第 k 项是 k+1 步后，移动后、摩擦后的速度），
    若途中进球，最后一项之后停止，并返回 goal = +1（进右门）/-1（进左门）/0。"""
    W, H = cfg['W'], cfg['H']
    GY0 = (H - cfg['GOAL']) // 2
    GY1 = GY0 + cfg['GOAL']
    C = cfg['CHAMFER']
    walls = [(1, 0, 0), (-1, 0, -W), (0, 1, 0), (0, -1, -H)]
    if C > 0:
        walls += [(1, 1, C), (-1, 1, C - W), (1, -1, C - H), (-1, -1, C - W - H)]
    r = cfg['RB']
    rd = isqrt(2 * r * r)
    en, ed = cfg['E_WALL_N'], cfg['E_WALL_D']
    fn, fd = cfg['BF_N'], cfg['BF_D']
    out = []
    x, y, vx, vy = bx, by, bvx, bvy
    for _ in range(n):
        x += vx
        y += vy
        if GY0 <= y <= GY1 and (x < 0 or x > W):
            out.append((x, y, 0, 0))
            return out, (1 if x > W else -1)
        for i, (a, b, cc) in enumerate(walls):
            if i < 2 and GY0 <= y <= GY1:
                continue
            n2 = a * a + b * b
            need = r if n2 == 1 else rd
            pen = need - (a * x + b * y - cc)
            if pen <= 0:
                continue
            if n2 == 1:
                x += a * pen
                y += b * pen
            else:
                s = (pen + 1) // 2
                x += a * s
                y += b * s
            dot = a * vx + b * vy
            if dot < 0:
                vx -= tdiv((ed + en) * dot * a, ed * n2)
                vy -= tdiv((ed + en) * dot * b, ed * n2)
        vx = tdiv(vx * fn, fd)
        vy = tdiv(vy * fn, fd)
        out.append((x, y, vx, vy))
        if vx == 0 and vy == 0:
            break
    return out, 0
