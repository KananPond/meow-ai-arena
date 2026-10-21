// sim.cpp  仓储机器人调度仿真：读入、每个 tick 的各个阶段、输出
#include "sim.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {

const char* stateName(RobotState s) {
    static const char* const kNames[] = {"IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"};
    return kNames[static_cast<int>(s)];
}

// 停机满这么多 tick 后才会被救援
const int kRescueAfter = 100;
// 连续等待达到这个次数后，允许让路
const int kSidestepAfter = 4;

}  // namespace

Sim::Sim(std::istream& in) {
    int W = 0, H = 0;
    in >> W >> H >> T_;
    std::vector<std::string> rows(H);
    for (auto& row : rows) in >> row;
    grid_ = Grid(W, H, rows);

    std::string tag;
    in >> tag;  // PARAMS
    in >> batteryMax_ >> chargeRate_ >> lowThreshold_ >> safetyMargin_ >> reportEvery_ >> hotRadius_;

    int nr = 0;
    in >> tag >> nr;  // ROBOTS
    robots_.resize(nr);
    occ_.assign(W * H, 0);
    chargeUse_.assign(W * H, 0);
    for (int i = 0; i < nr; i++) {
        Robot& r = robots_[i];
        in >> r.x >> r.y;
        r.battery = batteryMax_;  // 满电出发
        occ_[grid_.index(r.x, r.y)]++;
    }

    int ne = 0;
    in >> tag >> ne;  // EVENTS
    std::string line;
    std::getline(in, line);  // 吃掉 EVENTS 行的剩余部分
    byTick_.assign(T_, {});
    for (int e = 0; e < ne; e++) {
        std::getline(in, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        Tokens w;
        for (std::string tok; ss >> tok;) w.push_back(tok);
        if (w.size() < 2) continue;
        int t = std::atoi(w[0].c_str());
        if (t >= 0 && t < T_) byTick_[t].push_back(std::move(w));
    }
}

std::string Sim::run() {
    for (int t = 0; t < T_; t++) tick(t);

    long long open = 0;
    for (const Order& o : orders_) {
        if (o.state == OrderState::PENDING || o.state == OrderState::ASSIGNED || o.state == OrderState::PICKED) open++;
    }
    out_ += "SUMMARY delivered=" + std::to_string(delivered_) + " lost=" + std::to_string(lost_) +
            " rejected=" + std::to_string(rejected_) + " cancelled=" + std::to_string(cancelled_) +
            " open=" + std::to_string(open) + "\n";
    // 平均延迟为四舍五入的整数除法，与基线一致
    long long avg = delivered_ > 0 ? (latencySum_ + delivered_ / 2) / delivered_ : 0;
    out_ += "LATENCY sum=" + std::to_string(latencySum_) + " max=" + std::to_string(latencyMax_) +
            " avg=" + std::to_string(avg) + "\n";
    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        const Robot& r = robots_[i];
        out_ += "ROBOT " + robotName(i) + " " + std::to_string(r.x) + " " + std::to_string(r.y) + " " +
                std::to_string(r.battery) + " " + stateName(r.state) + " " + std::to_string(r.moves) + " " +
                std::to_string(r.waits) + "\n";
    }
    return out_;
}

void Sim::tick(int t) {
    grid_.trimCache();
    applyEvents(t);
    rescue(t);
    chargeRobots(t);
    dispatch(t);
    sendLowBatteryToCharger(t);
    moveRobots(t);
    report(t);
}

void Sim::log(int t, const std::string& msg) { out_ += std::to_string(t) + " " + msg + "\n"; }

bool Sim::validPoint(int x, int y) const { return grid_.inMap(x, y) && !grid_.isWall(x, y); }

bool Sim::occupied(int x, int y, int except) const {
    int n = occ_[grid_.index(x, y)];
    if (except >= 0) {
        const Robot& r = robots_[except];
        if (r.state != RobotState::DEAD && r.x == x && r.y == y) n--;
    }
    return n > 0;
}

void Sim::moveRobot(int i, int nx, int ny) {
    Robot& r = robots_[i];
    occ_[grid_.index(r.x, r.y)]--;
    occ_[grid_.index(nx, ny)]++;
    r.x = nx;
    r.y = ny;
    r.battery--;
    r.moves++;
    r.waitStreak = 0;
}

// ---------------------------------------------------------------- 1. 外部事件
void Sim::applyEvents(int t) {
    for (const Tokens& w : byTick_[t]) {
        const std::string& type = w[1];
        if (type == "ORDER") {
            if (w.size() >= 8) onOrder(t, w);
        } else if (type == "CANCEL") {
            if (w.size() >= 3) onCancel(t, w);
        } else if (type == "BLOCK" || type == "UNBLOCK") {
            if (w.size() >= 4) grid_.setBlocked(std::atoi(w[2].c_str()), std::atoi(w[3].c_str()), type == "BLOCK");
        }
        // 未知事件类型忽略
    }
}

// ORDER id px py dx dy priority
void Sim::onOrder(int t, const Tokens& w) {
    const std::string& id = w[2];
    int px = std::atoi(w[3].c_str()), py = std::atoi(w[4].c_str());
    int dx = std::atoi(w[5].c_str()), dy = std::atoi(w[6].c_str());
    int prio = std::atoi(w[7].c_str());
    if (!validPoint(px, py) || !validPoint(dx, dy)) {
        rejected_++;
        log(t, "REJECT " + id);
        return;
    }
    Order o;
    o.id = id;
    o.idNum = std::atoi(id.c_str());
    o.px = px;
    o.py = py;
    o.dx = dx;
    o.dy = dy;
    o.prio = prio;
    o.arrival = t;
    orders_.push_back(o);
    int idx = static_cast<int>(orders_.size()) - 1;
    orderIndex_[id] = idx;
    pending_.push_back(idx);
}

// CANCEL id
void Sim::onCancel(int t, const Tokens& w) {
    const std::string& id = w[2];
    auto it = orderIndex_.find(id);
    if (it == orderIndex_.end()) {
        log(t, "CANCEL_FAIL " + id);
        return;
    }
    cancelled_++;  // 订单存在就计数，即使最终取消失败也算
    Order& o = orders_[it->second];
    if (o.state == OrderState::PENDING) {
        pending_.erase(std::find(pending_.begin(), pending_.end(), it->second));
        o.state = OrderState::CANCELLED;
        log(t, "CANCEL " + id);
    } else if (o.state == OrderState::ASSIGNED) {
        Robot& r = robots_[o.robot];
        r.state = RobotState::IDLE;
        r.order = -1;
        r.tx = r.ty = -1;
        r.waitStreak = 0;
        o.state = OrderState::CANCELLED;
        o.robot = -1;
        log(t, "CANCEL " + id);
    } else {
        // 已取货或已结束，无法取消
        log(t, "CANCEL_FAIL " + id);
    }
}

// ---------------------------------------------------------------- 2. 救援
void Sim::rescue(int t) {
    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        Robot& r = robots_[i];
        if (r.state != RobotState::DEAD) continue;
        if (t - r.deadTick < kRescueAfter) continue;
        if (occupied(r.x, r.y, i)) continue;
        r.state = RobotState::IDLE;
        r.battery = batteryMax_ / 2;  // 救援后只恢复一半电量（基线行为）
        r.waitStreak = 0;
        r.deadTick = -1;
        occ_[grid_.index(r.x, r.y)]++;
        log(t, "RESCUE " + robotName(i));
    }
}

// ---------------------------------------------------------------- 3. 充电
void Sim::chargeRobots(int t) {
    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        Robot& r = robots_[i];
        if (r.state != RobotState::CHARGING) continue;
        r.battery = std::min(r.battery + chargeRate_, batteryMax_);
        if (r.battery * 10 >= batteryMax_ * 9) {  // 充到 90% 即离开充电桩
            r.state = RobotState::IDLE;
            r.tx = r.ty = -1;
            log(t, "CHARGED " + robotName(i));
        }
    }
}

// ---------------------------------------------------------------- 4. 派单
int Sim::dist(int ax, int ay, int bx, int by) {
    if (ax == bx && ay == by) return 0;
    if (!grid_.isOK(bx, by)) return BIGNUM;
    return toInt(grid_.distFrom(ax, ay)[grid_.index(bx, by)]);
}

void Sim::routeCost(Order& o) {
    if (o.costVersion == grid_.version()) return;
    o.costVersion = grid_.version();
    // b：取货点到送货点
    if (o.px == o.dx && o.py == o.dy) {
        o.b = 0;
    } else if (!grid_.isOK(o.dx, o.dy)) {
        o.b = BIGNUM;
    } else {
        o.b = grid_.distUntil(o.px, o.py, grid_.index(o.dx, o.dy));
    }
    // c：送货点到最近的可通行充电桩
    o.c = grid_.distUntil(o.dx, o.dy, -1);
}

void Sim::dispatch(int t) {
    if (pending_.empty()) return;
    // 候选机器人：空闲且电量不低于阈值，按下标升序
    std::vector<int> cand;
    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        const Robot& r = robots_[i];
        if (r.state == RobotState::IDLE && r.battery >= lowThreshold_) cand.push_back(i);
    }
    if (cand.empty()) return;

    // 优先级高的先，同优先级到达早的先，再按编号小的先
    std::vector<int> ids = pending_;
    std::sort(ids.begin(), ids.end(), [this](int a, int b) {
        const Order& x = orders_[a];
        const Order& y = orders_[b];
        if (x.prio != y.prio) return x.prio > y.prio;
        if (x.arrival != y.arrival) return x.arrival < y.arrival;
        return x.idNum < y.idNum;
    });

    for (int o : ids) {
        if (cand.empty()) break;
        Order& ord = orders_[o];
        int best = BIGNUM, who = -1, whoPos = -1;
        for (int k = 0; k < static_cast<int>(cand.size()); k++) {
            int i = cand[k];
            const Robot& r = robots_[i];
            int a = dist(r.x, r.y, ord.px, ord.py);
            if (a >= BIGNUM) continue;
            routeCost(ord);
            if (ord.b >= BIGNUM || ord.c >= BIGNUM) continue;
            if (r.battery < a + ord.b + ord.c + safetyMargin_) continue;
            // 选距离最短的；距离相同时后遍历到的（编号更大的）覆盖前者，与基线一致
            if (a <= best) {
                best = a;
                who = i;
                whoPos = k;
            }
        }
        if (who < 0) continue;
        Robot& r = robots_[who];
        r.state = RobotState::TO_PICKUP;
        r.order = o;
        r.tx = ord.px;
        r.ty = ord.py;
        r.waitStreak = 0;
        ord.state = OrderState::ASSIGNED;
        ord.robot = who;
        cand.erase(cand.begin() + whoPos);
        log(t, "ASSIGN " + ord.id + " " + robotName(who) + " " + std::to_string(best));
    }

    pending_.clear();
    for (int o : ids) {
        if (orders_[o].state == OrderState::PENDING) pending_.push_back(o);
    }
}

// ---------------------------------------------------------------- 5. 低电量去充电
void Sim::sendLowBatteryToCharger(int t) {
    // 充电桩占用计数：充电中的机器人占其所在格，前往充电的占其目标格。
    // 本阶段内只会新增 TO_CHARGER，计数在阶段末统一撤销。
    for (const Robot& r : robots_) {
        if (r.state == RobotState::CHARGING) chargeUse_[grid_.index(r.x, r.y)]++;
        else if (r.state == RobotState::TO_CHARGER) chargeUse_[grid_.index(r.tx, r.ty)]++;
    }

    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        Robot& r = robots_[i];
        if (r.state != RobotState::IDLE || r.battery >= lowThreshold_) continue;
        const uint16_t* d = grid_.distFrom(r.x, r.y);
        const int W = grid_.W();

        // 第一步：未被占用、电量够到达并留出安全余量的最近充电桩（行优先，平局取靠前的）
        int best = BIGNUM, cx = -1, cy = -1;
        for (int q : grid_.chargers()) {
            int x = q % W, y = q / W;
            if (!grid_.isOK(x, y) || chargeUse_[q] > 0) continue;
            int dq = toInt(d[q]);
            if (dq + safetyMargin_ > r.battery) continue;
            if (dq < best) {
                best = dq;
                cx = x;
                cy = y;
            }
        }
        // 第二步：都被占用时，去最近的可达充电桩排队（不看占用）
        if (cx < 0) {
            for (int q : grid_.chargers()) {
                int x = q % W, y = q / W;
                if (!grid_.isOK(x, y)) continue;
                int dq = toInt(d[q]);
                if (dq < best) {
                    best = dq;
                    cx = x;
                    cy = y;
                }
            }
        }
        if (cx < 0) continue;

        r.state = RobotState::TO_CHARGER;
        r.tx = cx;
        r.ty = cy;
        r.waitStreak = 0;
        chargeUse_[grid_.index(cx, cy)]++;
        log(t, "GO_CHARGE " + robotName(i) + " " + std::to_string(cx) + " " + std::to_string(cy));
    }

    for (const Robot& r : robots_) {
        if (r.state == RobotState::CHARGING) chargeUse_[grid_.index(r.x, r.y)]--;
        else if (r.state == RobotState::TO_CHARGER) chargeUse_[grid_.index(r.tx, r.ty)]--;
    }
}

// ---------------------------------------------------------------- 6. 移动
void Sim::arrive(int t, int i) {
    Robot& r = robots_[i];
    switch (r.state) {
        case RobotState::TO_PICKUP: {
            Order& o = orders_[r.order];
            o.state = OrderState::PICKED;
            r.state = RobotState::DELIVERING;
            r.tx = o.dx;
            r.ty = o.dy;
            log(t, "PICK " + o.id + " " + robotName(i));
            break;
        }
        case RobotState::DELIVERING: {
            Order& o = orders_[r.order];
            o.state = OrderState::DONE;
            int lat = t - o.arrival;
            delivered_++;
            latencySum_ += lat;
            latencyMax_ = std::max<long long>(latencyMax_, lat);
            log(t, "DELIVER " + o.id + " " + robotName(i) + " " + std::to_string(lat));
            r.state = RobotState::IDLE;
            r.order = -1;
            r.tx = r.ty = -1;
            break;
        }
        case RobotState::TO_CHARGER:
            r.state = RobotState::CHARGING;
            log(t, "CHARGE " + robotName(i));
            break;
        default:
            break;
    }
}

void Sim::die(int t, int i) {
    Robot& r = robots_[i];
    log(t, "DEAD " + robotName(i));
    if (r.order >= 0) {
        Order& o = orders_[r.order];
        if (r.state == RobotState::TO_PICKUP) {
            // 取货途中停机：订单退回待派单，保留原到达时间
            o.state = OrderState::PENDING;
            o.robot = -1;
            pending_.push_back(r.order);
            log(t, "REQUEUE " + o.id);
        } else if (r.state == RobotState::DELIVERING) {
            o.state = OrderState::LOST;
            lost_++;
            log(t, "LOST " + o.id);
        }
    }
    occ_[grid_.index(r.x, r.y)]--;
    r.state = RobotState::DEAD;
    r.deadTick = t;
    r.order = -1;
    r.tx = r.ty = -1;
}

void Sim::moveRobots(int t) {
    for (int i = 0; i < static_cast<int>(robots_.size()); i++) {
        Robot& r = robots_[i];
        if (r.state != RobotState::TO_PICKUP && r.state != RobotState::DELIVERING &&
            r.state != RobotState::TO_CHARGER) {
            continue;
        }
        if (r.x == r.tx && r.y == r.ty) {
            r.waitStreak = 0;
            arrive(t, i);
            continue;
        }
        if (r.battery == 0) {
            die(t, i);
            continue;
        }
        if (!grid_.isOK(r.tx, r.ty)) {  // 目标被封锁，等待
            r.waits++;
            continue;
        }

        // 距离表从目标出发：选能让机器人离目标最近的可通行相邻格
        const uint16_t* d = grid_.distFrom(r.tx, r.ty);
        int best = BIGNUM, bx = -1, by = -1;
        for (int k = 0; k < 4; k++) {
            int nx = r.x + DX4[k], ny = r.y + DY4[k];
            if (!grid_.isOK(nx, ny)) continue;
            int dk = toInt(d[grid_.index(nx, ny)]);
            if (dk < best) {
                best = dk;
                bx = nx;
                by = ny;
            }
        }
        if (best >= BIGNUM) {  // 走不过去
            r.waits++;
            continue;
        }

        if (occupied(bx, by, i)) {
            bool moved = false;
            bool nextIsTarget = (bx == r.tx && by == r.ty);
            if (r.waitStreak >= kSidestepAfter && !nextIsTarget) {
                // 等得太久了：让到第一个可通行且空闲的相邻格（按方向顺序，不看距离）
                for (int k = 0; k < 4; k++) {
                    int nx = r.x + DX4[k], ny = r.y + DY4[k];
                    if (grid_.isOK(nx, ny) && !occupied(nx, ny, i)) {
                        moveRobot(i, nx, ny);
                        log(t, "SIDESTEP " + robotName(i));
                        moved = true;
                        break;
                    }
                }
            }
            if (!moved) {
                r.waits++;
                r.waitStreak++;
                continue;
            }
        } else {
            moveRobot(i, bx, by);
        }

        if (r.x == r.tx && r.y == r.ty) {
            r.waitStreak = 0;
            arrive(t, i);
        }
    }
}

// ---------------------------------------------------------------- 7. 报表
void Sim::report(int t) {
    if ((t + 1) % reportEvery_ != 0) return;

    int counts[6] = {0, 0, 0, 0, 0, 0};
    for (const Robot& r : robots_) counts[static_cast<int>(r.state)]++;

    // 热点：对每个非墙格，统计曼哈顿距离 <= hotRadius 的非停机机器人数，取最大值（行优先取第一个）。
    // 令 u = x + y、v = x - y + H - 1，则曼哈顿菱形变为 |du| <= r 且 |dv| <= r 的正方形，
    // 用二维前缀和即可对每个格子 O(1) 计数。
    const int W = grid_.W(), H = grid_.H();
    const int N = W + H - 1;  // u、v 的取值范围都是 [0, N)
    const int S = N + 1;
    prefix_.assign(S * S, 0);
    for (const Robot& r : robots_) {
        if (r.state == RobotState::DEAD) continue;
        int u = r.x + r.y, v = r.x - r.y + H - 1;
        prefix_[(u + 1) * S + (v + 1)]++;
    }
    for (int a = 1; a <= N; a++) {
        for (int b = 1; b <= N; b++) {
            prefix_[a * S + b] += prefix_[(a - 1) * S + b] + prefix_[a * S + b - 1] - prefix_[(a - 1) * S + b - 1];
        }
    }
    int hx = -1, hy = -1, hc = -1;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid_.isWall(x, y)) continue;
            int u = x + y, v = x - y + H - 1;
            int uLo = std::max(0, u - hotRadius_), uHi = std::min(N - 1, u + hotRadius_);
            int vLo = std::max(0, v - hotRadius_), vHi = std::min(N - 1, v + hotRadius_);
            int n = prefix_[(uHi + 1) * S + (vHi + 1)] - prefix_[uLo * S + (vHi + 1)] -
                    prefix_[(uHi + 1) * S + vLo] + prefix_[uLo * S + vLo];
            if (n > hc) {
                hc = n;
                hx = x;
                hy = y;
            }
        }
    }

    log(t, "REPORT pending=" + std::to_string(pending_.size()) + " idle=" + std::to_string(counts[0]) +
               " to_pickup=" + std::to_string(counts[1]) + " delivering=" + std::to_string(counts[2]) +
               " to_charger=" + std::to_string(counts[3]) + " charging=" + std::to_string(counts[4]) +
               " dead=" + std::to_string(counts[5]) + " hot=" + std::to_string(hx) + "," + std::to_string(hy) +
               "," + std::to_string(hc));
}
