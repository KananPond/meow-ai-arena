// sim.h — 仓储机器人调度仿真：数据结构与主循环
#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "grid.h"

enum class RState : uint8_t { IDLE, TO_PICKUP, DELIVERING, TO_CHARGER, CHARGING, DEAD };
enum class OState : uint8_t { PENDING, ASSIGNED, PICKED, DONE, CANCELLED, LOST };

struct Params {
    int batteryMax, chargeRate, lowThreshold, safetyMargin, reportEvery, hotRadius;
};

struct Robot {
    int x = 0, y = 0;
    int battery = 0;
    int tx = -1, ty = -1;          // 目标；CHARGING 期间仍保留充电桩坐标（基线如此）
    int waitStreak = 0;            // 连续等待次数
    int deadSince = -1;
    long long waitTotal = 0;       // 累计等待
    long long moves = 0;           // 移动步数
    RState st = RState::IDLE;
    int order = -1;                // orders[] 下标
};

struct Order {
    int id = 0;
    int px = 0, py = 0, dx = 0, dy = 0, prio = 0, arrival = 0;
    OState st = OState::PENDING;
    int robot = -1;
    // 派单用的 b = dist(取货, 送货)、c = 送货点到最近可通行充电桩，只依赖地图。
    // 缓存到 bcLogPos（封锁日志位置），之后只有可能改变它们的封锁变化才触发重算（见 computeBC）
    uint64_t bcLogPos = ~0ull;
    int b = 0, c = 0;
};

enum class EvType : uint8_t { ORDER, CANCEL, BLOCK, UNBLOCK };
struct Event {
    int tick;
    EvType type;
    int id;                 // ORDER / CANCEL
    int a, b, c, d, prio;   // ORDER: px py dx dy prio；BLOCK/UNBLOCK: x=a y=b
};

// 待派订单的排序键：优先级大 → 到达早 → 编号小
struct PendKey {
    int prio, arrival, id, idx;
    bool operator<(const PendKey& o) const {
        if (prio != o.prio) return prio > o.prio;
        if (arrival != o.arrival) return arrival < o.arrival;
        return id < o.id;
    }
};

class Sim {
public:
    void read(const std::string& input);
    void run();
    void finish();
    const std::string& output() const { return out; }

private:
    int T = 0;
    Params P{};
    Grid grid;
    FieldCache fields;
    std::vector<Robot> robots;
    std::vector<Order> orders;
    std::unordered_map<int, int> orderIdx;   // 订单编号 -> orders 下标（被 REJECT 的不在里面）
    std::set<PendKey> pending;
    std::vector<Event> events;
    int lastGridChange = -1000000;           // 最近一次封锁状态真正改变的 tick
    std::vector<int> occ;                    // 每格上非 DEAD 机器人数
    long long delivered = 0, lost = 0, rejected = 0, cancelled = 0, latSum = 0, latMax = 0;
    std::string out;

    // 临时缓冲
    std::vector<int> cand, usedStamp, hotCnt;
    int stampNow = 0;

    void processEvents(int t, size_t& ev);
    void rescue(int t);
    void charge(int t);
    void dispatch(int t);
    bool chooseRobot(Order& o, int& who, int& best);
    void goCharge(int t);
    void move(int t);
    void arrive(int t, int i);
    void die(int t, int i);
    void report(int t);

    int distToSource(const uint16_t* F, int src, int from) const;
    void computeBC(Order& o);
    bool occupied(int c, int except) const;
    void setPos(int i, int x, int y);

    PendKey keyOf(int idx) const {
        const Order& o = orders[idx];
        return {o.prio, o.arrival, o.id, idx};
    }
    void log(int t, const std::string& msg);
};
