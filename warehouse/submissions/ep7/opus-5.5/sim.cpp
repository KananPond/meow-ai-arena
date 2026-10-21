// sim.cpp — 仿真主体。每个 tick 的阶段顺序与基线一致：
//   事件 → 救援 → 充电 → 派单 → 低电量去充电 → 移动 → 报表
// 与 SPEC.md 不一致的"基线怪行为"在对应位置用【基线】标出，详见 HANDOFF.md。
#include "sim.h"

#include <cstdlib>
#include <sstream>

static std::string rname(int i) { return "R" + std::to_string(i); }
static int manhattan(int ax, int ay, int bx, int by) { return std::abs(ax - bx) + std::abs(ay - by); }

void Sim::log(int t, const std::string& msg) {
    out += std::to_string(t);
    out += ' ';
    out += msg;
    out += '\n';
}

// ============================ 输入 ============================
// 输入格式由 tools/check_input.py 保证（单空格分隔、整数规范），这里按空白切词读取。
void Sim::read(const std::string& input) {
    std::istringstream in(input);
    int W, H;
    in >> W >> H >> T;
    std::vector<std::string> rows(H);
    for (auto& r : rows) in >> r;
    grid.init(W, H, rows);
    fields.init(&grid);
    occ.assign(W * H, 0);
    usedStamp.assign(W * H, 0);

    std::string tok;
    in >> tok >> P.batteryMax >> P.chargeRate >> P.lowThreshold >> P.safetyMargin >> P.reportEvery >> P.hotRadius;
    int nr;
    in >> tok >> nr;
    robots.resize(nr);
    for (int i = 0; i < nr; i++) {
        Robot& r = robots[i];
        in >> r.x >> r.y;
        r.battery = P.batteryMax;
        occ[grid.cell(r.x, r.y)]++;
    }
    int ne;
    in >> tok >> ne;
    events.reserve(ne);
    for (int i = 0; i < ne; i++) {
        Event e{};
        in >> e.tick >> tok;
        if (tok == "ORDER") {
            e.type = EvType::ORDER;
            in >> e.id >> e.a >> e.b >> e.c >> e.d >> e.prio;
        } else if (tok == "CANCEL") {
            e.type = EvType::CANCEL;
            in >> e.id;
        } else {
            e.type = (tok == "BLOCK") ? EvType::BLOCK : EvType::UNBLOCK;
            in >> e.a >> e.b;
        }
        events.push_back(e);
    }
}

// ============================ 工具 ============================

// dist(from, src)，F 是以 src 为源的距离场。路径 from→n→…→src 中除 from 外都必须可通行，
// 反过来看就是 src 的场在 from 的可通行邻居 n 上的值 + 1（from 自己可通行时就是 F[from]）。
int Sim::distToSource(const uint16_t* F, int src, int from) const {
    if (from == src) return 0;
    if (!grid.ok[src]) return INF_DIST;
    if (grid.ok[from]) return F[from] == FIELD_INF ? INF_DIST : F[from];
    int best = INF_DIST;
    for (int k = 0; k < 4; k++) {
        int n = from + grid.off[k];
        if (grid.ok[n] && F[n] != FIELD_INF && F[n] + 1 < best) best = F[n] + 1;
    }
    return best;
}

bool Sim::occupied(int c, int except) const {
    int n = occ[c];
    const Robot& r = robots[except];
    if (r.st != RState::DEAD && grid.cell(r.x, r.y) == c) n--;
    return n > 0;
}

void Sim::setPos(int i, int x, int y) {
    Robot& r = robots[i];
    occ[grid.cell(r.x, r.y)]--;
    r.x = x;
    r.y = y;
    occ[grid.cell(x, y)]++;
}

// ============================ 主循环 ============================
void Sim::run() {
    size_t ev = 0;
    for (int t = 0; t < T; t++) {
        processEvents(t, ev);
        rescue(t);
        charge(t);
        dispatch(t);
        goCharge(t);
        move(t);
        if ((t + 1) % P.reportEvery == 0) report(t);
    }
}

// ---- 1. 外部事件（事件按 tick 非递减，用指针顺序消费）
void Sim::processEvents(int t, size_t& ev) {
    const size_t logBefore = grid.log.size();
    for (; ev < events.size() && events[ev].tick == t; ev++) {
        const Event& e = events[ev];
        switch (e.type) {
        case EvType::ORDER: {
            bool good = grid.inMap(e.a, e.b) && grid.inMap(e.c, e.d) && !grid.wall[grid.cell(e.a, e.b)] &&
                        !grid.wall[grid.cell(e.c, e.d)];
            if (!good) {
                log(t, "REJECT " + std::to_string(e.id));
                rejected++;
                break;
            }
            Order o;
            o.id = e.id;
            o.px = e.a, o.py = e.b, o.dx = e.c, o.dy = e.d, o.prio = e.prio, o.arrival = t;
            int idx = (int)orders.size();
            orders.push_back(o);
            orderIdx[e.id] = idx;
            pending.insert(keyOf(idx));
            break;
        }
        case EvType::CANCEL: {
            auto it = orderIdx.find(e.id);
            if (it == orderIdx.end()) {
                log(t, "CANCEL_FAIL " + std::to_string(e.id));
                break;
            }
            // 【基线】只要订单存在就计入 cancelled，即使随后输出 CANCEL_FAIL
            cancelled++;
            Order& o = orders[it->second];
            if (o.st == OState::PENDING) {
                pending.erase(keyOf(it->second));
                o.st = OState::CANCELLED;
                o.robot = -1;
                log(t, "CANCEL " + std::to_string(e.id));
            } else if (o.st == OState::ASSIGNED) {
                Robot& r = robots[o.robot];
                r.st = RState::IDLE;
                r.order = -1;
                r.tx = r.ty = -1;
                r.waitStreak = 0;
                o.st = OState::CANCELLED;
                o.robot = -1;
                log(t, "CANCEL " + std::to_string(e.id));
            } else {
                log(t, "CANCEL_FAIL " + std::to_string(e.id));
            }
            break;
        }
        case EvType::BLOCK: grid.block(e.a, e.b); break;
        case EvType::UNBLOCK: grid.unblock(e.a, e.b); break;
        }
    }
    if (grid.log.size() != logBefore) lastGridChange = t;
}

// ---- 2. 救援
void Sim::rescue(int t) {
    for (int i = 0; i < (int)robots.size(); i++) {
        Robot& r = robots[i];
        if (r.st != RState::DEAD || t - r.deadSince < 100) continue;
        int c = grid.cell(r.x, r.y);
        if (occ[c] > 0) continue;   // 自己是 DEAD，不在 occ 里
        r.st = RState::IDLE;
        occ[c]++;
        r.battery = P.batteryMax / 2;   // 【基线】救援只给一半电，不是充满
        r.waitStreak = 0;
        r.deadSince = -1;
        log(t, "RESCUE " + rname(i));
    }
}

// ---- 3. 充电
void Sim::charge(int t) {
    for (int i = 0; i < (int)robots.size(); i++) {
        Robot& r = robots[i];
        if (r.st != RState::CHARGING) continue;
        r.battery += P.chargeRate;
        if (r.battery > P.batteryMax) r.battery = P.batteryMax;
        if (r.battery * 10 >= P.batteryMax * 9) {   // 【基线】充到 90% 就结束
            r.st = RState::IDLE;
            r.tx = r.ty = -1;
            log(t, "CHARGED " + rname(i));
        }
    }
}

// ---- 4. 派单
// b、c 的缓存校验：一条经过格子 X 的路径长度至少是曼哈顿距离之和 s。
//  * 封锁 X 只可能拉长经过 X 的最短路：s ≤ 当前值时才可能有影响；
//  * 解封 X 只可能产生经过 X 的更短路：s < 当前值时才可能有影响。
// 两者都是保守判断（可能多算，不会漏）。
static bool mayChange(const Grid::Toggle& tg, int s, int cur) {
    return tg.nowBlocked ? (cur < INF_DIST && s <= cur) : (s < cur);
}

void Sim::computeBC(Order& o) {
    const uint64_t end = grid.log.size();
    const int p = grid.cell(o.px, o.py), d = grid.cell(o.dx, o.dy);
    bool stale = o.bcLogPos == ~0ull || end - o.bcLogPos > 4096;
    for (uint64_t i = o.bcLogPos; !stale && i < end; i++) {
        const Grid::Toggle& tg = grid.log[i];
        int x = tg.cell % grid.W, y = tg.cell / grid.W;
        int sb = manhattan(o.px, o.py, x, y) + manhattan(x, y, o.dx, o.dy);
        int sc = manhattan(o.dx, o.dy, x, y) + grid.chargerManh[tg.cell];
        stale = mayChange(tg, sb, o.b) || mayChange(tg, sc, o.c);
    }
    o.bcLogPos = end;
    if (!stale) return;
    // b = dist(p, d)
    if (p == d) o.b = 0;
    else if (!grid.ok[d]) o.b = INF_DIST;
    else o.b = fields.pointDist(p, d);
    // c = min over 可通行充电桩 q 的 dist(d, q)：d 可通行时就是多源场的值，否则看 d 的可通行邻居
    const uint16_t* M = fields.chargerField();
    if (grid.ok[d]) o.c = M[d] == FIELD_INF ? INF_DIST : M[d];
    else {
        o.c = INF_DIST;
        for (int k = 0; k < 4; k++) {
            int n = d + grid.off[k];
            if (grid.ok[n] && M[n] != FIELD_INF && M[n] + 1 < o.c) o.c = M[n] + 1;
        }
    }
}

// 为订单 o 在候选 cand（按编号升序）中挑机器人；返回是否找到。
bool Sim::chooseRobot(Order& o, int& who, int& best) {
    const int margin = P.safetyMargin;
    // 先用曼哈顿下界粗筛：a ≥ |r-p|，b ≥ |p-d|，c ≥ 0
    int bLow = manhattan(o.px, o.py, o.dx, o.dy);
    bool any = false;
    for (int i : cand) {
        const Robot& r = robots[i];
        if (r.battery >= manhattan(r.x, r.y, o.px, o.py) + bLow + margin) { any = true; break; }
    }
    if (!any) return false;

    computeBC(o);
    if (o.b >= INF_DIST || o.c >= INF_DIST) return false;
    const int K = o.b + o.c + margin;
    const int p = grid.cell(o.px, o.py);
    const uint16_t* F = nullptr;
    who = -1;
    best = INF_DIST;
    for (int i : cand) {
        const Robot& r = robots[i];
        if (r.battery < manhattan(r.x, r.y, o.px, o.py) + K) continue;
        int rc = grid.cell(r.x, r.y);
        if (!F && rc != p && grid.ok[p]) F = fields.get(p);
        int a = distToSource(F, p, rc);
        if (a >= INF_DIST || r.battery < a + K) continue;
        if (a <= best) {   // 【基线】用 <=，距离相同时选编号"大"的（SPEC 说小的）
            best = a;
            who = i;
        }
    }
    return who >= 0;
}

void Sim::dispatch(int t) {
    if (pending.empty()) return;
    cand.clear();
    for (int i = 0; i < (int)robots.size(); i++)
        if (robots[i].st == RState::IDLE && robots[i].battery >= P.lowThreshold) cand.push_back(i);
    for (auto it = pending.begin(); it != pending.end() && !cand.empty();) {
        Order& o = orders[it->idx];
        int who, best;
        if (!chooseRobot(o, who, best)) {
            ++it;
            continue;
        }
        Robot& r = robots[who];
        r.st = RState::TO_PICKUP;
        r.order = it->idx;
        r.tx = o.px;
        r.ty = o.py;
        r.waitStreak = 0;
        o.st = OState::ASSIGNED;
        o.robot = who;
        log(t, "ASSIGN " + std::to_string(o.id) + " " + rname(who) + " " + std::to_string(best));
        it = pending.erase(it);
        for (size_t k = 0; k < cand.size(); k++)
            if (cand[k] == who) {
                cand.erase(cand.begin() + k);
                break;
            }
    }
}

// ---- 5. 低电量去充电
void Sim::goCharge(int t) {
    for (int i = 0; i < (int)robots.size(); i++) {
        Robot& r = robots[i];
        if (r.st != RState::IDLE || r.battery >= P.lowThreshold) continue;
        // 标记被其他机器人占用的充电桩：CHARGING 看位置，TO_CHARGER 看目标
        ++stampNow;
        for (int j = 0; j < (int)robots.size(); j++) {
            if (j == i) continue;
            const Robot& o = robots[j];
            if (o.st == RState::CHARGING) usedStamp[grid.cell(o.x, o.y)] = stampNow;
            else if (o.st == RState::TO_CHARGER) usedStamp[grid.cell(o.tx, o.ty)] = stampNow;
        }
        const uint16_t* F = fields.get(grid.cell(r.x, r.y));   // dist(r, q) = F[q]
        int best = INF_DIST, pick = -1;       // 第 1 步：未占用、电量够
        int nearD = INF_DIST, nearest = -1;   // 第 2 步：不管占用，最近可达
        for (int q : grid.chargers) {
            if (!grid.ok[q] || F[q] == FIELD_INF) continue;
            int d = F[q];
            if (d < nearD) nearD = d, nearest = q;
            if (usedStamp[q] == stampNow) continue;
            if (d + P.safetyMargin > r.battery) continue;
            if (d < best) best = d, pick = q;
        }
        if (pick < 0) pick = nearest;
        if (pick < 0) continue;
        r.st = RState::TO_CHARGER;
        r.tx = pick % grid.W;
        r.ty = pick / grid.W;
        r.waitStreak = 0;
        log(t, "GO_CHARGE " + rname(i) + " " + std::to_string(r.tx) + " " + std::to_string(r.ty));
    }
}

// ---- 6. 移动
void Sim::arrive(int t, int i) {
    Robot& r = robots[i];
    r.waitStreak = 0;
    if (r.st == RState::TO_PICKUP) {
        Order& o = orders[r.order];
        o.st = OState::PICKED;
        r.st = RState::DELIVERING;
        r.tx = o.dx;
        r.ty = o.dy;
        log(t, "PICK " + std::to_string(o.id) + " " + rname(i));
    } else if (r.st == RState::DELIVERING) {
        Order& o = orders[r.order];
        o.st = OState::DONE;
        int lat = t - o.arrival;
        delivered++;
        latSum += lat;
        if (lat > latMax) latMax = lat;
        log(t, "DELIVER " + std::to_string(o.id) + " " + rname(i) + " " + std::to_string(lat));
        r.st = RState::IDLE;
        r.order = -1;
        r.tx = r.ty = -1;
    } else if (r.st == RState::TO_CHARGER) {
        r.st = RState::CHARGING;
        log(t, "CHARGE " + rname(i));
    }
}

void Sim::die(int t, int i) {
    Robot& r = robots[i];
    log(t, "DEAD " + rname(i));
    if (r.order >= 0) {
        Order& o = orders[r.order];
        if (r.st == RState::TO_PICKUP) {
            o.st = OState::PENDING;   // 保留原到达 tick
            o.robot = -1;
            pending.insert(keyOf(r.order));
            log(t, "REQUEUE " + std::to_string(o.id));
        } else if (r.st == RState::DELIVERING) {
            o.st = OState::LOST;
            lost++;
            log(t, "LOST " + std::to_string(o.id));
        }
    }
    occ[grid.cell(r.x, r.y)]--;
    r.st = RState::DEAD;
    r.deadSince = t;
    r.order = -1;
    r.tx = r.ty = -1;
}

void Sim::move(int t) {
    for (int i = 0; i < (int)robots.size(); i++) {
        Robot& r = robots[i];
        if (r.st != RState::TO_PICKUP && r.st != RState::DELIVERING && r.st != RState::TO_CHARGER) continue;
        if (r.x == r.tx && r.y == r.ty) {   // 已在目标：到达处理，本 tick 不再移动
            arrive(t, i);
            continue;
        }
        if (r.battery == 0) {
            die(t, i);
            continue;
        }
        int tc = grid.cell(r.tx, r.ty);
        if (!grid.ok[tc]) {   // 目标被封：等待，连续等待不变
            r.waitTotal++;
            continue;
        }
        // 下一步 = 可通行邻格中 dist(目标, ·) 最小者，平局按方向顺序。
        // 地图最近稳定时用（并缓存）完整距离场；频繁封路时完整场总在失效，改用到达即停的局部 BFS。
        int rc = grid.cell(r.x, r.y);
        int next = -1;
        const uint16_t* F = fields.peek(tc);
        if (!F && t - lastGridChange >= 8) F = fields.get(tc);
        if (F) {
            int best = FIELD_INF;
            for (int k = 0; k < 4; k++) {
                int n = rc + grid.off[k];
                if (grid.ok[n] && F[n] < best) best = F[n], next = n;
            }
        } else {
            next = fields.nextStep(tc, rc);
        }
        if (next < 0) {   // 走不过去：等待，连续等待不变
            r.waitTotal++;
            continue;
        }
        if (occupied(next, i)) {
            bool moved = false;
            if (r.waitStreak >= 4 && next != tc) {   // 让路：第一个可通行且没人的邻格
                for (int k = 0; k < 4; k++) {
                    int n = rc + grid.off[k];
                    if (grid.ok[n] && !occupied(n, i)) {
                        setPos(i, n % grid.W, n / grid.W);
                        r.battery--;
                        r.moves++;
                        r.waitStreak = 0;
                        log(t, "SIDESTEP " + rname(i));
                        moved = true;
                        break;
                    }
                }
            }
            if (!moved) {
                r.waitTotal++;
                r.waitStreak++;
                continue;
            }
        } else {
            setPos(i, next % grid.W, next / grid.W);
            r.battery--;
            r.moves++;
            r.waitStreak = 0;
        }
        if (r.x == r.tx && r.y == r.ty) arrive(t, i);
    }
}

// ---- 7. 报表
// 热点：每个非墙格统计曼哈顿距离 ≤ R 的非 DEAD 机器人数。用旋转坐标 u=x+y, v=x-y+H-1，
// 菱形变成 u、v 方向各 ±R 的正方形，二维前缀和 O(1) 查询。
void Sim::report(int t) {
    int c[6] = {0, 0, 0, 0, 0, 0};
    for (const Robot& r : robots) c[(int)r.st]++;

    const int W = grid.W, H = grid.H, N = W + H - 1, S = N + 1, R = P.hotRadius;
    hotCnt.assign((size_t)S * S, 0);   // 前缀和 pre[(u+1)*S + (v+1)]
    for (const Robot& r : robots)
        if (r.st != RState::DEAD) hotCnt[(size_t)(r.x + r.y + 1) * S + (r.x - r.y + H - 1 + 1)]++;
    for (int u = 1; u < S; u++)
        for (int v = 1; v < S; v++)
            hotCnt[(size_t)u * S + v] +=
                hotCnt[(size_t)(u - 1) * S + v] + hotCnt[(size_t)u * S + v - 1] - hotCnt[(size_t)(u - 1) * S + v - 1];
    int hx = -1, hy = -1, hc = -1;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid.wall[grid.cell(x, y)]) continue;
            int u = x + y, v = x - y + H - 1;
            int u0 = std::max(0, u - R), u1 = std::min(N - 1, u + R);
            int v0 = std::max(0, v - R), v1 = std::min(N - 1, v + R);
            int n = hotCnt[(size_t)(u1 + 1) * S + v1 + 1] - hotCnt[(size_t)u0 * S + v1 + 1] -
                    hotCnt[(size_t)(u1 + 1) * S + v0] + hotCnt[(size_t)u0 * S + v0];
            if (n > hc) hc = n, hx = x, hy = y;
        }
    }
    log(t, "REPORT pending=" + std::to_string(pending.size()) + " idle=" + std::to_string(c[0]) +
               " to_pickup=" + std::to_string(c[1]) + " delivering=" + std::to_string(c[2]) +
               " to_charger=" + std::to_string(c[3]) + " charging=" + std::to_string(c[4]) +
               " dead=" + std::to_string(c[5]) + " hot=" + std::to_string(hx) + "," + std::to_string(hy) + "," +
               std::to_string(hc));
}

// ============================ 收尾 ============================
static const char* stateName(RState s) {
    static const char* names[] = {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};
    return names[(int)s];
}

void Sim::finish() {
    long long open = 0;
    for (const Order& o : orders)
        if (o.st == OState::PENDING || o.st == OState::ASSIGNED || o.st == OState::PICKED) open++;
    out += "SUMMARY delivered=" + std::to_string(delivered) + " lost=" + std::to_string(lost) +
           " rejected=" + std::to_string(rejected) + " cancelled=" + std::to_string(cancelled) +
           " open=" + std::to_string(open) + "\n";
    long long avg = delivered > 0 ? (latSum + delivered / 2) / delivered : 0;   // 【基线】四舍五入，不是截断
    out += "LATENCY sum=" + std::to_string(latSum) + " max=" + std::to_string(latMax) + " avg=" + std::to_string(avg) +
           "\n";
    for (size_t i = 0; i < robots.size(); i++) {
        const Robot& r = robots[i];
        out += "ROBOT " + rname((int)i) + " " + std::to_string(r.x) + " " + std::to_string(r.y) + " " +
               std::to_string(r.battery) + " " + stateName(r.st) + " " + std::to_string(r.moves) + " " +
               std::to_string(r.waitTotal) + "\n";
    }
}
