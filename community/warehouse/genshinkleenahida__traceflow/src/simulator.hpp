#ifndef TRACEFLOW_SIMULATOR_HPP
#define TRACEFLOW_SIMULATOR_HPP

#include "grid.hpp"

#include <cstdint>
#include <charconv>
#include <iosfwd>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace traceflow {

enum class RobotState { Idle, ToPickup, Delivering, ToCharger, Charging, Dead };
enum class OrderState { Pending, Assigned, Picked, Done, Cancelled, Lost };
enum class EventKind { Order, Cancel, Block, Unblock };

struct Parameters {
    int batteryMax, chargeRate, lowThreshold, safetyMargin, reportEvery, hotRadius;
};

struct Robot {
    int cell = -1, battery = 0, target = -1, order = -1;
    int consecutiveWait = 0, deadSince = -1;
    std::int64_t steps = 0, waits = 0;
    RobotState state = RobotState::Idle;
    std::string name;
};

struct Order {
    int id, pickup, dropoff, priority, arrival;
    OrderState state = OrderState::Pending;
    int robot = -1;
    // Only topology affects pickup -> dropoff -> nearest-charger cost.
    std::uint64_t routeVersion = std::numeric_limits<std::uint64_t>::max();
    int remainingCost = kInf;
};

struct Event {
    int tick;
    EventKind kind;
    int id = 0, first = -1, second = -1, priority = 0;
};

class Simulator {
public:
    Simulator(Grid grid, int ticks, Parameters parameters,
              std::vector<Robot> robots, std::vector<Event> events);
    void run(std::ostream& output);

private:
    using PendingKey = std::tuple<int, int, int, int>;
    using IdleSnapshot = std::tuple<int, int, int>;
    struct RouteCache {
        std::vector<int> cells;
        std::vector<std::uint16_t> positions;
        std::size_t cursor = 0;
        int source = -1, target = -1;
        std::uint64_t version = 0;
        bool valid = false;
        int failedSource = -1, failedTarget = -1;
        std::uint64_t failedVersion = std::numeric_limits<std::uint64_t>::max();
    };

    Grid grid_;
    int ticks_;
    Parameters parameters_;
    std::vector<Robot> robots_;
    std::vector<RouteCache> routes_;
    std::vector<Event> events_;
    std::size_t nextEvent_ = 0;
    std::vector<Order> orders_;
    std::unordered_map<int, int> orderIndex_;
    std::set<PendingKey> pending_;
    std::uint64_t pendingVersion_ = 1;

    // DEAD robots do not occupy their cells; there is at most one live robot.
    std::vector<int> occupant_;
    std::vector<int> reservationTick_, reservationCount_;
    std::vector<int> heatDifference_;

    std::uint64_t dispatchGridVersion_ = 0, dispatchPendingVersion_ = 0;
    std::vector<IdleSnapshot> dispatchIdle_;

    std::int64_t delivered_ = 0, lost_ = 0, rejected_ = 0, cancelled_ = 0;
    std::int64_t latencySum_ = 0, latencyMax_ = 0;
    std::ostream* output_ = nullptr;
    std::string outputBuffer_;

    void append(std::string_view value) { outputBuffer_.append(value.data(), value.size()); }
    void append(char value) { outputBuffer_.push_back(value); }
    template<class Integer, std::enable_if_t<std::is_integral_v<Integer>, int> = 0>
    void append(Integer value) {
        char digits[32];
        const auto end = std::to_chars(digits, digits + sizeof(digits), value).ptr;
        outputBuffer_.append(digits, end);
    }
    template<class... Parts> void write(const Parts&... parts) {
        (append(parts), ...);
        if (outputBuffer_.size() >= 65536) flush();
    }
    template<class... Parts> void log(int tick, const Parts&... parts) {
        write(tick, ' ', parts..., '\n');
    }
    void flush();

    PendingKey key(int index) const;
    void addPending(int index);
    void removePending(int index);
    void idle(int robot);
    void events(int tick);
    void rescue(int tick);
    void charge(int tick);
    void dispatch(int tick);
    void seekChargers(int tick);
    int routeStep(int robot);
    void move(int tick);
    void arrive(int robot, int tick);
    void die(int robot, int tick);
    void moveTo(int robot, int cell);
    void report(int tick);
    void finish();
};

void simulate(std::istream& input, std::ostream& output);

}  // namespace traceflow

#endif
