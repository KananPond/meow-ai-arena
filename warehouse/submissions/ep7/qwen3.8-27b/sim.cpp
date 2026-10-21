// sim.cpp  仿真主体（重写版）
//
// 行为与 baseline/ 逐字节一致。重构点（详见 HANDOFF.md）：
//   1. 事件在 ReadAll 时一次性解析并按 tick 分桶 -> 每 tick 只处理自己的事件
//      （基线是每 tick 扫全部事件，O(T*E) 字符串操作，是最大瓶颈）。
//   2. BFS 改扁平数组 + 每 tick 内按起点缓存（同一 tick 地图不变，缓存有效）。
//   3. 派单里 a 距离改为“从取货点做一次 BFS”读所有机器人，而不是
//      “每个机器人各做一次 BFS”（基线 O(P*R) 次 BFS/tick）。
//   4. OCC 占据判断改 O(1) 占据网格。
//   5. 报表热点用 (u=x+y, v=x-y) 变换 + 二维前缀和，O(1)/格。
//   6. 状态/订单 id 全部整数化；日志攒在一个大 buffer 里最后一次性输出。
//
// 注意保留的“怪行为”（以基线为准，勿“修复”）：
//   - 救援只补到 batteryMax/2（不是满电）
//   - 充电到 >=90% 就变 IDLE（不是满电）
//   - 派单平局取编号“大”的（代码 a<=best，注释却写“编号小”）
#include "common.h"

// 方向顺序：上 右 下 左（与基线一致）
static const int DX4[4] = {0, 1, 0, -1};
static const int DY4[4] = {-1, 0, 1, 0};

static inline bool INMAP(int x, int y) {
    return x >= 0 && y >= 0 && x < W && y < H;
}
static inline bool ISWALL(int x, int y) { return MAP[y][x] == '#'; }
static inline bool OK(int x, int y) {
    return (x >= 0 && y >= 0 && x < W && y < H) && passable[y * W + x];
}
// 与基线 atoi 语义相同的快速整数解析（合法输入下 token 都是标准整数）
static inline int tokint(const string& s) {
    int i = 0; bool neg = false; long long v = 0;
    if (s[i] == '-') { neg = true; i++; }
    for (; i < (int)s.size() && isdigit((unsigned char)s[i]); i++) v = v * 10 + (s[i] - '0');
    return neg ? (int)-v : (int)v;
}

// ================= 读输入 =================
void ReadAll(istream& in) {
    in >> W >> H >> T;
    MAP.resize(H);
    for (int i = 0; i < H; i++) in >> MAP[i];
    string tmp;
    in >> tmp;                       // "PARAMS"
    for (int i = 0; i < 6; i++) in >> PRM[i];
    in >> tmp >> NR;                 // "ROBOTS"
    RX.resize(NR); RY.resize(NR); RB.resize(NR);
    RTX.resize(NR); RTY.resize(NR); RWS.resize(NR); RDS.resize(NR);
    RWT.resize(NR); RTR.resize(NR); RST.resize(NR); ROID.resize(NR);
    for (int i = 0; i < NR; i++) {
        in >> RX[i] >> RY[i];
        RB[i] = PRM[0]; RTX[i] = -1; RTY[i] = -1;
        RWS[i] = 0; RDS[i] = -1; RWT[i] = 0; RTR[i] = 0;
        RST[i] = S_IDLE; ROID[i] = 0;
    }
    RNAMES.resize(NR);
    for (int i = 0; i < NR; i++) {
        char b[16]; int n = snprintf(b, sizeof b, "R%d", i);
        RNAMES[i].assign(b, n);
    }
    initChargers();
    initPassable();
    occ.assign(W * H, 0);
    for (int i = 0; i < NR; i++) occ[RY[i] * W + RX[i]] = i + 1;

    int ne = 0;
    in >> tmp >> ne;                 // "EVENTS"
    string line;
    getline(in, line);               // 吃掉 EVENTS 行的行尾
    EVT.reserve(ne);
    EVTT.assign(T, {});
    for (int i = 0; i < ne; i++) {
        getline(in, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // 与基线 SPLIT 相同：按空白切分
        vector<string> w;
        string cur;
        for (char c : line) {
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
                if (!cur.empty()) { w.push_back(cur); cur.clear(); }
            } else cur.push_back(c);
        }
        if (!cur.empty()) w.push_back(cur);
        if (w.size() < 2) continue;                    // 基线：不足两个 token 直接跳过
        int tick = tokint(w[0]);
        Event ev{tick, 7, 0, 0, 0, 0, 0};
        const string& typ = w[1];
        if (typ == "ORDER") {
            ev.type = EV_ORDER;
            ev.a = tokint(w[2]); ev.b = tokint(w[3]); ev.c = tokint(w[4]);
            ev.d = tokint(w[5]); ev.e = tokint(w[6]); ev.f = tokint(w[7]);
        } else if (typ == "CANCEL") {
            ev.type = EV_CANCEL; ev.a = tokint(w[2]);
        } else if (typ == "BLOCK") {
            ev.type = EV_BLOCK; ev.a = tokint(w[2]); ev.b = tokint(w[3]);
        } else if (typ == "UNBLOCK") {
            ev.type = EV_UNBLOCK; ev.a = tokint(w[2]); ev.b = tokint(w[3]);
        }
        EVT.push_back(ev);
        if (tick >= 0 && tick < T) EVTT[tick].push_back((int)EVT.size() - 1);
    }
}

// ================= 主循环 =================
void RunSim() {
    // BFS 缓存：起点扁平下标 -> 距离表。只在地图（BLK）真正变化时失效。
    // runBFS 始终返回缓存中的稳定指针（满了就淘汰一个再存），避免多指针冲突。
    size_t memBudget = (size_t)384 * 1024 * 1024;              // ~384MB 给 BFS 缓存
    size_t cacheMax = memBudget / ((size_t)W * H * 4) + 1;
    unordered_map<int, vector<int>> bfsCache;
    bfsCache.reserve(min<size_t>(cacheMax, 4096));

    vector<int> hotGrid;
    vector<int> aForRobot; aForRobot.resize(NR);   // 每单派单时暂存各机器人的 a

    auto runBFS = [&](int sx, int sy) -> const int* {
        int key = sy * W + sx;
        auto it = bfsCache.find(key);
        if (it != bfsCache.end()) return it->second.data();
        if (bfsCache.size() >= cacheMax) bfsCache.erase(bfsCache.begin());
        bfsCache.emplace(key, vector<int>(W * H));
        bfs(sx, sy, bfsCache[key]);
        return bfsCache[key].data();
    };

    for (int t = 0; t < T; t++) {
        // BFS 缓存只在地图（BLK）真正变化时失效；其余 tick 复用上一 tick 的结果。
        bool blkChanged = false;

        // ============ 1. 事件 ============
        for (int ei : EVTT[t]) {
            const Event& ev = EVT[ei];
            if (ev.type == EV_ORDER) {
                int id = ev.a, px = ev.b, py = ev.c, dx = ev.d, dy = ev.e, pr = ev.f;
                bool good = true;
                if (!INMAP(px, py)) good = false;
                if (good && !INMAP(dx, dy)) good = false;
                if (good && ISWALL(px, py)) good = false;
                if (good && ISWALL(dx, dy)) good = false;
                if (!good) { writeLog(t, "REJECT " + to_string(id)); continue; }
                Order o; o.id = id; o.px = px; o.py = py; o.dx = dx; o.dy = dy;
                o.prio = pr; o.arrival = t; o.status = O_PENDING; o.robot = 0;
                ORD[id] = o;
                PEND.push_back(id);
            } else if (ev.type == EV_CANCEL) {
                int id = ev.a;
                auto it = ORD.find(id);
                if (it == ORD.end()) { writeLog(t, "CANCEL_FAIL " + to_string(id)); continue; }
                CNT[3]++;   // 基线：订单存在即计数（哪怕随后 CANCEL_FAIL）
                Order& o = it->second;
                if (o.status == O_PENDING) {
                    o.status = O_CANCELLED; o.robot = 0;
                    PEND.erase(remove_if(PEND.begin(), PEND.end(), [id](int v) { return v == id; }), PEND.end());
                    writeLog(t, "CANCEL " + to_string(id));
                } else if (o.status == O_ASSIGNED) {
                    int ri = o.robot - 1;
                    RST[ri] = S_IDLE; ROID[ri] = 0;
                    RTX[ri] = -1; RTY[ri] = -1; RWS[ri] = 0;
                    o.status = O_CANCELLED; o.robot = 0;
                    writeLog(t, "CANCEL " + to_string(id));
                } else {
                    writeLog(t, "CANCEL_FAIL " + to_string(id));
                }
            } else if (ev.type == EV_BLOCK) {
                int x = ev.a, y = ev.b;
                if (INMAP(x, y) && !ISWALL(x, y)) {
                    int cell = y * W + x;
                    if (BLK.insert(cell).second) { passable[cell] = 0; blkChanged = true; }
                }
            } else if (ev.type == EV_UNBLOCK) {
                int x = ev.a, y = ev.b;
                if (INMAP(x, y)) {
                    int cell = y * W + x;
                    if (BLK.erase(cell)) { passable[cell] = 1; blkChanged = true; }
                }
            }
        }
        if (blkChanged) bfsCache.clear();

        // ============ 2. 救援 ============
        // 怪行为：只补到 PRM[0]/2（不是满电）
        for (int i = 0; i < NR; i++) {
            if (RST[i] != S_DEAD) continue;
            if (t - RDS[i] >= 100) {
                int cell = RY[i] * W + RX[i];
                if (occ[cell] == 0 || occ[cell] == i + 1) {
                    RST[i] = S_IDLE;
                    RB[i] = PRM[0] / 2;
                    RWS[i] = 0; RDS[i] = -1;
                    occ[cell] = i + 1;
                    writeLog(t, "RESCUE " + RNAMES[i]);
                }
            }
        }

        // ============ 3. 充电 ============
        // 怪行为：>=90% 即完成（不是满电）
        for (int i = 0; i < NR; i++) {
            if (RST[i] != S_CHARGING) continue;
            RB[i] += PRM[1];
            if (RB[i] > PRM[0]) RB[i] = PRM[0];
            if (RB[i] * 10 >= PRM[0] * 9) {
                RST[i] = S_IDLE; RTX[i] = -1; RTY[i] = -1;
                writeLog(t, "CHARGED " + RNAMES[i]);
            }
        }

        // ============ 4. 派单 ============
        if (!PEND.empty()) {
            vector<int> ids;
            ids.reserve(PEND.size());
            for (int id : PEND) ids.push_back(id);
            // 优先级大 -> 到达早 -> 编号小
            sort(ids.begin(), ids.end(), [](int a, int b) {
                const Order& oa = ORD[a]; const Order& ob = ORD[b];
                if (oa.prio != ob.prio) return oa.prio > ob.prio;
                if (oa.arrival != ob.arrival) return oa.arrival < ob.arrival;
                return a < b;
            });
            unordered_set<int> assigned;
            assigned.reserve(ids.size() * 2);
            for (int id : ids) {
                Order& o = ORD[id];
                if (o.status != O_PENDING) continue;   // 防御
                int px = o.px, py = o.py, dx = o.dx, dy = o.dy;
                // dpick: 从取货点做一次 BFS（起点不限可通行，与基线 BFS 一致）。
                // 它同时给出 b 和所有机器人的 a（见下）。
                const int* dpick = runBFS(px, py);
                int b = dpick[dy * W + dx];            // 与基线 DIST(pickup,delivery) 完全一致
                if (b >= BIGNUM) continue;
                bool pickOK = OK(px, py);
                // 先把所有机器人的 a 从 dpick 读出来（随后的 runBFS 可能淘汰该缓存条目）
                for (int i = 0; i < NR; i++) {
                    int rx = RX[i], ry = RY[i];
                    int a;
                    if (rx == px && ry == py) {
                        a = 0;                          // 就在取货点上（基线 DIST 首判）
                    } else if (!pickOK) {
                        a = BIGNUM;                     // 取货点被封且机器人不在上面
                    } else if (passable[ry * W + rx]) {
                        a = dpick[ry * W + rx];         // 无向图，机器人格可通行
                    } else {
                        // 机器人站在被封格上：基线从该格 BFS（起点可不可通行），
                        // 等于先花 1 步走进任一可通行邻居，再走 dpick 距离。
                        a = BIGNUM;
                        for (int k = 0; k < 4; k++) {
                            int nx = rx + DX4[k], ny = ry + DY4[k];
                            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                            int ni = ny * W + nx;
                            if (!passable[ni]) continue;
                            int dd = dpick[ni];
                            if (dd < BIGNUM && dd + 1 < a) a = dd + 1;
                        }
                    }
                    aForRobot[i] = a;
                }
                const int* ddel = runBFS(dx, dy);      // 从送货点 BFS，用于 c
                int c = BIGNUM;
                for (int chg : chargers) {
                    if (!passable[chg]) continue;
                    int d = ddel[chg];
                    if (d < c) c = d;
                }
                if (c >= BIGNUM) continue;
                int best = BIGNUM, who = -1;
                for (int i = 0; i < NR; i++) {
                    if (RST[i] != S_IDLE) continue;
                    if (RB[i] < PRM[2]) continue;
                    int a = aForRobot[i];
                    if (a >= BIGNUM) continue;
                    if (RB[i] < a + b + c + PRM[3]) continue;
                    if (a <= best) { best = a; who = i; }   // 怪行为：平局取编号大的
                }
                if (who < 0) continue;
                RST[who] = S_TOPICKUP;
                ROID[who] = id; RTX[who] = px; RTY[who] = py; RWS[who] = 0;
                o.status = O_ASSIGNED; o.robot = who + 1;
                assigned.insert(id);
                writeLog(t, "ASSIGN " + to_string(id) + " " + RNAMES[who] + " " + to_string(best));
            }
            if (!assigned.empty()) {
                vector<int> np;
                np.reserve(PEND.size());
                for (int id : PEND) if (!assigned.count(id)) np.push_back(id);
                PEND.swap(np);
            }
        }

        // ============ 5. 低电量去充电 ============
        for (int i = 0; i < NR; i++) {
            if (RST[i] != S_IDLE) continue;
            if (RB[i] >= PRM[2]) continue;
            const int* dr = runBFS(RX[i], RY[i]);
            int best = BIGNUM, cx = -1, cy = -1;
            for (int chg : chargers) {
                if (!passable[chg]) continue;
                int x = chg % W, y = chg / W;
                bool used = false;
                for (int j = 0; j < NR; j++) {
                    if (j == i) continue;
                    if (RST[j] == S_CHARGING && RX[j] == x && RY[j] == y) { used = true; break; }
                    if (RST[j] == S_TOCHARGER && RTX[j] == x && RTY[j] == y) { used = true; break; }
                }
                if (used) continue;
                int d = dr[chg];
                if (d + PRM[3] > RB[i]) continue;
                if (d < best) { best = d; cx = x; cy = y; }
            }
            if (cx < 0) {
                int b2 = BIGNUM;
                for (int chg : chargers) {
                    if (!passable[chg]) continue;
                    int d = dr[chg];
                    if (d < b2) { b2 = d; cx = chg % W; cy = chg / W; }
                }
            }
            if (cx < 0) continue;
            RST[i] = S_TOCHARGER;
            RTX[i] = cx; RTY[i] = cy; RWS[i] = 0;
            writeLog(t, "GO_CHARGE " + RNAMES[i] + " " + to_string(cx) + " " + to_string(cy));
        }

        // ============ 6. 移动 ============
        // arrive/die 逻辑（与基线 3.6/3.8 一致）
        auto arrive = [&](int i) {
            RWS[i] = 0;
            if (RST[i] == S_TOPICKUP) {
                int oid = ROID[i];
                ORD[oid].status = O_PICKED;
                RST[i] = S_DELIVERING;
                RTX[i] = ORD[oid].dx; RTY[i] = ORD[oid].dy;
                writeLog(t, "PICK " + to_string(oid) + " " + RNAMES[i]);
            } else if (RST[i] == S_DELIVERING) {
                int oid = ROID[i];
                ORD[oid].status = O_DONE;
                int lat = t - ORD[oid].arrival;
                writeLog(t, "DELIVER " + to_string(oid) + " " + RNAMES[i] + " " + to_string(lat));
                RST[i] = S_IDLE; ROID[i] = 0; RTX[i] = -1; RTY[i] = -1;
            } else if (RST[i] == S_TOCHARGER) {
                RST[i] = S_CHARGING;
                writeLog(t, "CHARGE " + RNAMES[i]);
            }
        };
        auto die = [&](int i) {
            writeLog(t, "DEAD " + RNAMES[i]);
            int oid = ROID[i];
            if (oid != 0) {
                if (RST[i] == S_TOPICKUP) {
                    ORD[oid].status = O_PENDING; ORD[oid].robot = 0;
                    PEND.push_back(oid);
                    writeLog(t, "REQUEUE " + to_string(oid));
                } else if (RST[i] == S_DELIVERING) {
                    ORD[oid].status = O_LOST;
                    writeLog(t, "LOST " + to_string(oid));
                }
            }
            occ[RY[i] * W + RX[i]] = 0;
            RST[i] = S_DEAD; RDS[i] = t;
            ROID[i] = 0; RTX[i] = -1; RTY[i] = -1;
        };

        for (int i = 0; i < NR; i++) {
            int st = RST[i];
            if (st != S_TOPICKUP && st != S_DELIVERING && st != S_TOCHARGER) continue;
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) { arrive(i); continue; }
            if (RB[i] == 0) { die(i); continue; }
            if (!OK(RTX[i], RTY[i])) { RWT[i]++; continue; }
            const int* d = runBFS(RTX[i], RTY[i]);
            int best = BIGNUM, bx = -1, by = -1;
            for (int k = 0; k < 4; k++) {
                int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                int ni = ny * W + nx;
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                if (!passable[ni]) continue;
                int dd = d[ni];
                if (dd < best) { best = dd; bx = nx; by = ny; }
            }
            if (best >= BIGNUM) { RWT[i]++; continue; }
            int ni = by * W + bx;
            bool occd = (occ[ni] != 0 && occ[ni] != i + 1);
            if (occd) {
                bool moved = false;
                if (RWS[i] >= 4 && !(bx == RTX[i] && by == RTY[i])) {
                    for (int k = 0; k < 4; k++) {
                        int nx = RX[i] + DX4[k], ny = RY[i] + DY4[k];
                        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                        int sj = ny * W + nx;
                        if (!passable[sj]) continue;
                        if (occ[sj] != 0 && occ[sj] != i + 1) continue;
                        occ[RY[i] * W + RX[i]] = 0;
                        RX[i] = nx; RY[i] = ny; occ[sj] = i + 1;
                        RB[i]--; RTR[i]++; RWS[i] = 0;
                        writeLog(t, "SIDESTEP " + RNAMES[i]);
                        moved = true;
                        break;
                    }
                }
                if (!moved) { RWT[i]++; RWS[i]++; continue; }
            } else {
                occ[RY[i] * W + RX[i]] = 0;
                RX[i] = bx; RY[i] = by; occ[ni] = i + 1;
                RB[i]--; RTR[i]++; RWS[i] = 0;
            }
            if (RX[i] == RTX[i] && RY[i] == RTY[i]) arrive(i);
        }

        // ============ 7. 报表 ============
        if ((t + 1) % PRM[4] == 0) {
            int c[6] = {0, 0, 0, 0, 0, 0};
            for (int i = 0; i < NR; i++) c[RST[i]]++;
            // 热点：曼哈顿半径 r 内的非停机机器人数。
            // |dx|+|dy| <= r  <=>  max(|du|,|dv|) <= r，其中 u=x+y, v=x-y+(H-1)
            int Udim = W + H - 1, Vdim = W + H - 1;
            size_t gs = (size_t)Vdim * Udim;
            if (hotGrid.size() != gs) hotGrid.assign(gs, 0);
            else fill(hotGrid.begin(), hotGrid.end(), 0);
            for (int i = 0; i < NR; i++) {
                if (RST[i] == S_DEAD) continue;
                int u = RX[i] + RY[i];
                int v = RX[i] - RY[i] + (H - 1);
                hotGrid[(size_t)v * Udim + u]++;
            }
            for (int v = 0; v < Vdim; v++) {
                size_t row = (size_t)v * Udim, prev = row - Udim;
                int run = 0;
                for (int u = 0; u < Udim; u++) {
                    run += hotGrid[row + u];
                    hotGrid[row + u] = (v > 0 ? (int)hotGrid[prev + u] : 0) + run;
                }
            }
            int r = PRM[5];
            int hx = -1, hy = -1, hc = -1;
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    if (MAP[y][x] == '#') continue;
                    int u = x + y, v = x - y + (H - 1);
                    int u1 = u - r, u2 = u + r, v1 = v - r, v2 = v + r;
                    if (u1 < 0) u1 = 0;
                    if (v1 < 0) v1 = 0;
                    if (u2 >= Udim) u2 = Udim - 1;
                    if (v2 >= Vdim) v2 = Vdim - 1;
                    int cnt = (int)hotGrid[(size_t)v2 * Udim + u2];
                    if (u1 > 0) cnt -= (int)hotGrid[(size_t)v2 * Udim + u1 - 1];
                    if (v1 > 0) cnt -= (int)hotGrid[(size_t)(v1 - 1) * Udim + u2];
                    if (u1 > 0 && v1 > 0) cnt += (int)hotGrid[(size_t)(v1 - 1) * Udim + u1 - 1];
                    if (cnt > hc) { hc = cnt; hx = x; hy = y; }
                }
            }
            string s = "REPORT pending=" + to_string(PEND.size()) +
                       " idle=" + to_string(c[0]) + " to_pickup=" + to_string(c[1]) +
                       " delivering=" + to_string(c[2]) + " to_charger=" + to_string(c[3]) +
                       " charging=" + to_string(c[4]) + " dead=" + to_string(c[5]) +
                       " hot=" + to_string(hx) + "," + to_string(hy) + "," + to_string(hc);
            writeLog(t, s);
        }
    }
}

// ================= 收尾 =================
void Finish() {
    long long open = 0;
    for (auto& kv : ORD) {
        int st = kv.second.status;
        if (st == O_PENDING || st == O_ASSIGNED || st == O_PICKED) open++;
    }
    writeRaw("SUMMARY delivered=" + to_string(CNT[0]) + " lost=" + to_string(CNT[1]) +
             " rejected=" + to_string(CNT[2]) + " cancelled=" + to_string(CNT[3]) +
             " open=" + to_string(open));
    long long avg = 0;
    if (CNT[0] > 0) avg = (CNT[4] + CNT[0] / 2) / CNT[0];
    writeRaw("LATENCY sum=" + to_string(CNT[4]) + " max=" + to_string(CNT[5]) +
             " avg=" + to_string(avg));
    for (int i = 0; i < NR; i++) {
        writeRaw("ROBOT " + RNAMES[i] + " " + to_string(RX[i]) + " " + to_string(RY[i]) +
                 " " + to_string(RB[i]) + " " + STNAME[RST[i]] + " " +
                 to_string(RTR[i]) + " " + to_string(RWT[i]));
    }
}
