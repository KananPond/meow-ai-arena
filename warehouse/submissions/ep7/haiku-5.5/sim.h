// sim.h  仓储机器人调度仿真
//
// 每个 tick 依次执行：外部事件 -> 救援 -> 充电 -> 派单 -> 低电量去充电 -> 移动 -> 报表。
// 各阶段的先后、机器人的遍历顺序、平局规则都与基线程序一致，见 HANDOFF.md。
#ifndef WMS_SIM_H
#define WMS_SIM_H

#include <istream>
#include <string>
#include <unordered_map>
#include <vector>

#include "grid.h"

enum class RobotState { IDLE, TO_PICKUP, DELIVERING, TO_CHARGER, CHARGING, DEAD };
enum class OrderState { PENDING, ASSIGNED, PICKED, DONE, CANCELLED, LOST };

struct Robot {
    int x = 0, y = 0;
    int battery = 0;
    int tx = -1, ty = -1;      // 目标格，无目标时为 -1
    int waitStreak = 0;        // 连续等待次数，达到 4 时可以让路
    int deadTick = -1;         // 停机时刻
    RobotState state = RobotState::IDLE;
    int order = -1;            // 当前订单在 orders_ 中的下标，无则为 -1
    long long moves = 0;       // 移动步数
    long long waits = 0;       // 累计等待次数
};

struct Order {
    std::string id;            // 原始字符串，用于输出
    int idNum = 0;             // 数值，用于排序的最后一级
    int px = 0, py = 0;        // 取货点
    int dx = 0, dy = 0;        // 送货点
    int prio = 0;
    int arrival = 0;           // 到达 tick
    OrderState state = OrderState::PENDING;
    int robot = -1;            // 负责的机器人下标，无则为 -1
    // 派单代价 b（取货到送货）与 c（送货到最近充电桩）只依赖地图与封锁，
    // 封锁版本没变时可以直接复用
    int costVersion = -1;
    int b = 0, c = 0;
};

class Sim {
public:
    explicit Sim(std::istream& in);

    // 运行全部 tick，返回完整输出（每行以 \n 结尾）
    std::string run();

private:
    using Tokens = std::vector<std::string>;

    void tick(int t);
    void applyEvents(int t);
    void onOrder(int t, const Tokens& w);
    void onCancel(int t, const Tokens& w);
    void rescue(int t);
    void chargeRobots(int t);
    void dispatch(int t);
    void sendLowBatteryToCharger(int t);
    void moveRobots(int t);
    void arrive(int t, int i);
    void die(int t, int i);
    void report(int t);

    // 派单代价 b、c（按订单缓存）
    void routeCost(Order& o);
    // 机器人到格子的距离（与基线 DIST 语义一致）
    int dist(int ax, int ay, int bx, int by);
    // 格子是否被除自己以外的存活机器人占据
    bool occupied(int x, int y, int except) const;
    void moveRobot(int i, int nx, int ny);
    bool validPoint(int x, int y) const;

    void log(int t, const std::string& msg);
    std::string robotName(int i) const { return "R" + std::to_string(i); }

    // 场景参数
    int T_ = 0;
    int batteryMax_ = 0, chargeRate_ = 0, lowThreshold_ = 0, safetyMargin_ = 0;
    int reportEvery_ = 1, hotRadius_ = 0;

    Grid grid_;
    std::vector<Robot> robots_;
    std::vector<Order> orders_;
    std::unordered_map<std::string, int> orderIndex_;
    std::vector<int> pending_;                 // PENDING 订单的下标（集合语义，排序后使用）
    std::vector<std::vector<Tokens>> byTick_;  // 按 tick 分桶的事件，保持输入顺序

    std::vector<int> occ_;        // 每格存活机器人数
    std::vector<int> chargeUse_;  // 每个充电桩被占用的次数（充电中或前往充电）
    std::vector<int> prefix_;     // 热点计算用的二维前缀和

    // 统计
    long long delivered_ = 0, lost_ = 0, rejected_ = 0, cancelled_ = 0;
    long long latencySum_ = 0, latencyMax_ = 0;

    std::string out_;
};

#endif
