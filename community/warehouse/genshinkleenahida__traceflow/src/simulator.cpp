#include "simulator.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <utility>

namespace traceflow {
namespace {
constexpr std::array<const char*, 6> stateNames = {
    "IDLE", "TO_PICKUP", "DELIVERING", "TO_CHARGER", "CHARGING", "DEAD"
};
bool moving(RobotState state) {
    return state == RobotState::ToPickup || state == RobotState::Delivering ||
           state == RobotState::ToCharger;
}

// The official input checker specifies ASCII tokens and bounded decimal
// integers. Read only the supplied stream, then parse without locale work.
class InputBuffer {
public:
    explicit InputBuffer(std::istream& input) {
        std::array<char, 65536> chunk;
        while (input.read(chunk.data(), static_cast<std::streamsize>(chunk.size())) || input.gcount()) {
            data_.append(chunk.data(), static_cast<std::size_t>(input.gcount()));
        }
        data_.push_back('\0');
    }

    bool empty() const { return data_.size() == 1; }

    std::string_view word() {
        skipWhitespace();
        const std::size_t first = cursor_;
        while (data_[cursor_] > ' ') ++cursor_;
        return std::string_view(data_.data() + first, cursor_ - first);
    }

    int integer() {
        skipWhitespace();
        const bool negative = data_[cursor_] == '-';
        cursor_ += negative;
        int value = 0;
        while (data_[cursor_] >= '0' && data_[cursor_] <= '9') {
            value = value * 10 + (data_[cursor_] - '0');
            ++cursor_;
        }
        return negative ? -value : value;
    }

private:
    void skipWhitespace() {
        while (data_[cursor_] != '\0' && data_[cursor_] <= ' ') ++cursor_;
    }
    std::string data_;
    std::size_t cursor_ = 0;
};
}  // namespace

Simulator::Simulator(Grid grid, int ticks, Parameters parameters,
                     std::vector<Robot> robots, std::vector<Event> events)
    : grid_(std::move(grid)), ticks_(ticks), parameters_(parameters),
      robots_(std::move(robots)), routes_(robots_.size()), events_(std::move(events)),
      occupant_(grid_.size(), -1), reservationTick_(grid_.size(), -1),
      reservationCount_(grid_.size(), 0),
      heatDifference_(grid_.height() * (grid_.width() + 1), 0) {
    orders_.reserve(events_.size());
    orderIndex_.reserve(events_.size());
    for (int i = 0; i < static_cast<int>(robots_.size()); ++i)
        occupant_[robots_[i].cell] = i;
}

Simulator::PendingKey Simulator::key(int index) const {
    const auto& order = orders_[index];
    return {-order.priority, order.arrival, order.id, index};
}

void Simulator::addPending(int index) {
    pending_.insert(key(index));
    ++pendingVersion_;
}

void Simulator::removePending(int index) {
    pending_.erase(key(index));
    ++pendingVersion_;
}

void Simulator::flush() {
    output_->write(outputBuffer_.data(), static_cast<std::streamsize>(outputBuffer_.size()));
    outputBuffer_.clear();
}

void Simulator::idle(int index) {
    auto& robot = robots_[index];
    robot.state = RobotState::Idle;
    robot.order = -1;
    robot.target = -1;
}

void Simulator::events(int tick) {
    // The input checker guarantees nondecreasing event ticks. Preserve all
    // same-tick events in input order, including CANCEL before ORDER.
    while (nextEvent_ < events_.size() && events_[nextEvent_].tick == tick) {
        const auto& event = events_[nextEvent_++];
        if (event.kind == EventKind::Order) {
            if (grid_.wall(event.first) || grid_.wall(event.second)) {
                ++rejected_;
                log(tick, "REJECT ", event.id);
                continue;
            }
            const int index = static_cast<int>(orders_.size());
            orders_.push_back({event.id, event.first, event.second, event.priority, tick});
            orderIndex_.emplace(event.id, index);
            addPending(index);
        } else if (event.kind == EventKind::Cancel) {
            const auto found = orderIndex_.find(event.id);
            if (found == orderIndex_.end()) {
                log(tick, "CANCEL_FAIL ", event.id);
                continue;
            }
            // Compatibility: even a failed cancellation counts if ORD exists.
            ++cancelled_;
            const int index = found->second;
            auto& order = orders_[index];
            if (order.state == OrderState::Pending) {
                removePending(index);
            } else if (order.state == OrderState::Assigned) {
                idle(order.robot);
                robots_[order.robot].consecutiveWait = 0;
            } else {
                log(tick, "CANCEL_FAIL ", event.id);
                continue;
            }
            order.state = OrderState::Cancelled;
            order.robot = -1;
            log(tick, "CANCEL ", event.id);
        } else {
            grid_.block(event.first, event.kind == EventKind::Block);
        }
    }
}

void Simulator::rescue(int tick) {
    for (int i = 0; i < static_cast<int>(robots_.size()); ++i) {
        auto& robot = robots_[i];
        if (robot.state != RobotState::Dead || tick - robot.deadSince < 100 ||
            occupant_[robot.cell] != -1) continue;
        robot.state = RobotState::Idle;
        robot.battery = parameters_.batteryMax / 2;
        robot.consecutiveWait = 0;
        robot.deadSince = -1;
        occupant_[robot.cell] = i;
        log(tick, "RESCUE ", robot.name);
    }
}

void Simulator::charge(int tick) {
    for (auto& robot : robots_) {
        if (robot.state != RobotState::Charging) continue;
        robot.battery = std::min(parameters_.batteryMax, robot.battery + parameters_.chargeRate);
        // These integer products fit in int under check_input.py's limits.
        if (robot.battery * 10 >= parameters_.batteryMax * 9) {
            robot.state = RobotState::Idle;
            robot.target = -1;
            log(tick, "CHARGED ", robot.name);
        }
    }
}

void Simulator::dispatch(int tick) {
    std::vector<IdleSnapshot> candidates;
    candidates.reserve(robots_.size());
    for (int i = 0; i < static_cast<int>(robots_.size()); ++i) {
        const auto& robot = robots_[i];
        if (robot.state == RobotState::Idle && robot.battery >= parameters_.lowThreshold)
            candidates.emplace_back(i, robot.cell, robot.battery);
    }

    // Failed assignments cannot change until topology, pending orders or the
    // eligible idle fleet changes. Time and other robots' occupancy do not
    // participate in baseline dispatch. Store the *post-dispatch* fleet.
    if (grid_.version() == dispatchGridVersion_ &&
        pendingVersion_ == dispatchPendingVersion_ && candidates == dispatchIdle_) return;

    for (auto it = pending_.begin(); it != pending_.end() && !candidates.empty();) {
        const int index = std::get<3>(*it);
        auto& order = orders_[index];
        const bool pickupOpen = grid_.passable(order.pickup);
        if (!pickupOpen && std::none_of(candidates.begin(), candidates.end(),
            [&order](const auto& candidate) { return std::get<1>(candidate) == order.pickup; })) {
            // DIST(a,p) is infinite for a closed pickup unless a==p. Avoid
            // computing the rest of a route that no candidate can begin.
            ++it;
            continue;
        }
        if (order.routeVersion != grid_.version()) {
            const int charger = grid_.chargerDistance(order.dropoff);
            int delivery = kInf;
            if (charger != kInf) {
                delivery = pickupOpen && grid_.passable(order.dropoff)
                    ? grid_.distance(order.dropoff, order.pickup)
                    : grid_.distance(order.pickup, order.dropoff);
            }
            order.remainingCost = delivery == kInf || charger == kInf
                ? kInf : delivery + charger + parameters_.safetyMargin;
            order.routeVersion = grid_.version();
        }

        int maximumBattery = 0;
        for (const auto& candidate : candidates)
            maximumBattery = std::max(maximumBattery, std::get<2>(candidate));
        if (order.remainingCost > maximumBattery) {
            ++it;
            continue;
        }

        int who = -1, best = kInf;
        if (pickupOpen) {
            for (const auto& [robot, position, battery] : candidates) {
                const int distance = grid_.distance(position, order.pickup);
                // <= is intentional: the original implementation chooses the
                // larger robot index on a tie, contrary to its old comment.
                if (distance < kInf && distance <= best &&
                    distance + order.remainingCost <= battery) {
                    who = robot;
                    best = distance;
                }
            }
        } else {
            // DIST(p,p)==0 even when p is blocked; every other source fails.
            for (const auto& [robot, position, battery] : candidates) {
                if (position == order.pickup && order.remainingCost <= battery) {
                    who = robot;
                    best = 0;
                }
            }
        }
        if (who == -1) {
            ++it;
            continue;
        }

        auto& robot = robots_[who];
        robot.state = RobotState::ToPickup;
        robot.order = index;
        robot.target = order.pickup;
        robot.consecutiveWait = 0;
        order.state = OrderState::Assigned;
        order.robot = who;
        it = pending_.erase(it);
        ++pendingVersion_;
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                         [who](const auto& candidate) { return std::get<0>(candidate) == who; }),
                         candidates.end());
        log(tick, "ASSIGN ", order.id, ' ', robot.name, ' ', best);
    }
    dispatchIdle_ = std::move(candidates);
    dispatchGridVersion_ = grid_.version();
    dispatchPendingVersion_ = pendingVersion_;
}

void Simulator::seekChargers(int tick) {
    if (std::none_of(robots_.begin(), robots_.end(), [this](const Robot& robot) {
            return robot.state == RobotState::Idle && robot.battery < parameters_.lowThreshold;
        })) return;

    const auto reserve = [this, tick](int cell) {
        if (reservationTick_[cell] != tick) {
            reservationTick_[cell] = tick;
            reservationCount_[cell] = 0;
        }
        ++reservationCount_[cell];
    };
    for (const auto& robot : robots_) {
        if (robot.state == RobotState::Charging) reserve(robot.cell);
        else if (robot.state == RobotState::ToCharger) reserve(robot.target);
    }

    for (auto& robot : robots_) {
        if (robot.state != RobotState::Idle || robot.battery >= parameters_.lowThreshold) continue;
        int preferred = -1, fallback = -1, best = kInf, fallbackDistance = kInf;
        for (const int charger : grid_.chargers()) {
            if (!grid_.passable(charger)) continue;
            const int distance = grid_.distance(robot.cell, charger);
            // Row-major traversal with a strict comparison preserves ties.
            if (distance < fallbackDistance) {
                fallbackDistance = distance;
                fallback = charger;
            }
            const bool used = reservationTick_[charger] == tick && reservationCount_[charger] > 0;
            if (!used && distance < best && distance + parameters_.safetyMargin <= robot.battery) {
                best = distance;
                preferred = charger;
            }
        }
        // The fallback intentionally ignores both battery and reservations.
        const int destination = preferred == -1 ? fallback : preferred;
        if (destination == -1) continue;
        robot.state = RobotState::ToCharger;
        robot.target = destination;
        robot.consecutiveWait = 0;
        reserve(destination);
        log(tick, "GO_CHARGE ", robot.name, ' ', grid_.x(destination), ' ', grid_.y(destination));
    }
}

void Simulator::arrive(int index, int tick) {
    auto& robot = robots_[index];
    robot.consecutiveWait = 0;
    if (robot.state == RobotState::ToCharger) {
        robot.state = RobotState::Charging;
        log(tick, "CHARGE ", robot.name);
        return;
    }
    auto& order = orders_[robot.order];
    if (robot.state == RobotState::ToPickup) {
        order.state = OrderState::Picked;
        robot.state = RobotState::Delivering;
        robot.target = order.dropoff;
        log(tick, "PICK ", order.id, ' ', robot.name);
    } else {
        order.state = OrderState::Done;
        const int latency = tick - order.arrival;
        ++delivered_;
        latencySum_ += latency;
        latencyMax_ = std::max(latencyMax_, static_cast<std::int64_t>(latency));
        log(tick, "DELIVER ", order.id, ' ', robot.name, ' ', latency);
        idle(index);
    }
}

void Simulator::die(int index, int tick) {
    auto& robot = robots_[index];
    log(tick, "DEAD ", robot.name);
    if (robot.order != -1) {
        auto& order = orders_[robot.order];
        if (robot.state == RobotState::ToPickup) {
            order.state = OrderState::Pending;
            order.robot = -1;
            addPending(robot.order);
            log(tick, "REQUEUE ", order.id);
        } else if (robot.state == RobotState::Delivering) {
            order.state = OrderState::Lost;
            ++lost_;
            log(tick, "LOST ", order.id);
        }
    }
    occupant_[robot.cell] = -1;
    robot.state = RobotState::Dead;
    robot.deadSince = tick;
    robot.order = -1;
    robot.target = -1;
}

void Simulator::moveTo(int index, int destination) {
    auto& robot = robots_[index];
    occupant_[robot.cell] = -1;
    occupant_[destination] = index;
    robot.cell = destination;
    --robot.battery;
    ++robot.steps;
    robot.consecutiveWait = 0;
}

int Simulator::routeStep(int index) {
    const auto& robot = robots_[index];
    auto& route = routes_[index];
    if (route.valid) {
        if (route.target != robot.target) route.valid = false;
        else {
            // Consume an executed canonical step. Waiting preserves the cursor;
            // a sidestep or any other displacement fails the expected position.
            if (route.cursor < route.cells.size() && route.cells[route.cursor] == robot.cell)
                ++route.cursor;
            const int expected = route.cursor == 0 ? route.source : route.cells[route.cursor - 1];
            if (expected != robot.cell || route.cursor == route.cells.size()) route.valid = false;
            else if (route.version != grid_.version()) {
                route.valid = grid_.validateRoute(route.cells, route.cursor,
                                                   route.positions, route.version);
                route.version = grid_.version();
            }
        }
    }
    if (route.valid) return route.cells[route.cursor];

    if (route.failedSource != robot.cell || route.failedTarget != robot.target ||
        route.failedVersion != grid_.version()) {
        route.source = robot.cell;
        route.target = robot.target;
        route.cursor = 0;
        route.version = grid_.version();
        if (grid_.canonicalRoute(robot.cell, robot.target, route.cells) && !route.cells.empty()) {
            if (route.positions.empty()) route.positions.resize(grid_.size());
            for (std::size_t i = 0; i < route.cells.size(); ++i)
                route.positions[route.cells[i]] = static_cast<std::uint16_t>(i);
            route.valid = true;
            return route.cells.front();
        }
        route.failedSource = robot.cell;
        route.failedTarget = robot.target;
        route.failedVersion = grid_.version();
    }
    return grid_.nextStep(robot.cell, robot.target);
}

void Simulator::move(int tick) {
    for (int i = 0; i < static_cast<int>(robots_.size()); ++i) {
        auto& robot = robots_[i];
        if (!moving(robot.state)) continue;
        // Arrival has precedence over exhaustion and a newly blocked target.
        if (robot.cell == robot.target) {
            arrive(i, tick);
            continue;
        }
        if (robot.battery == 0) {
            die(i, tick);
            continue;
        }
        if (!grid_.passable(robot.target)) {
            ++robot.waits;
            continue;
        }
        const int next = routeStep(i);
        if (next == -1) {
            ++robot.waits;
            continue;
        }
        if (occupant_[next] != -1) {
            int sidestep = -1;
            if (robot.consecutiveWait >= 4 && next != robot.target) {
                for (const int cell : grid_.neighbors(robot.cell)) {
                    if (grid_.passable(cell) && occupant_[cell] == -1) {
                        sidestep = cell;
                        break;
                    }
                }
            }
            if (sidestep == -1) {
                ++robot.waits;
                ++robot.consecutiveWait;
                continue;
            }
            moveTo(i, sidestep);
            log(tick, "SIDESTEP ", robot.name);
        } else {
            moveTo(i, next);
        }
        if (robot.cell == robot.target) arrive(i, tick);
    }
}

void Simulator::report(int tick) {
    std::array<int, 6> count{};
    std::fill(heatDifference_.begin(), heatDifference_.end(), 0);
    const int width = grid_.width(), height = grid_.height(), stride = width + 1;
    const int radius = parameters_.hotRadius;
    for (const auto& robot : robots_) {
        ++count[static_cast<int>(robot.state)];
        if (robot.state == RobotState::Dead) continue;
        const int rx = grid_.x(robot.cell), ry = grid_.y(robot.cell);
        const int firstRow = std::max(0, ry - radius), lastRow = std::min(height - 1, ry + radius);
        for (int y = firstRow; y <= lastRow; ++y) {
            const int reach = radius - std::abs(y - ry);
            const int firstColumn = std::max(0, rx - reach), lastColumn = std::min(width - 1, rx + reach);
            ++heatDifference_[y * stride + firstColumn];
            --heatDifference_[y * stride + lastColumn + 1];
        }
    }
    int hottest = -1, hotCount = -1;
    for (int y = 0; y < height; ++y) {
        int value = 0;
        for (int x = 0; x < width; ++x) {
            value += heatDifference_[y * stride + x];
            const int cell = grid_.cell(x, y);
            // Blocked floor remains a candidate; only walls are excluded.
            if (!grid_.wall(cell) && value > hotCount) {
                hottest = cell;
                hotCount = value;
            }
        }
    }
    log(tick, "REPORT pending=", pending_.size(), " idle=", count[0], " to_pickup=", count[1],
        " delivering=", count[2], " to_charger=", count[3], " charging=", count[4],
        " dead=", count[5], " hot=", grid_.x(hottest), ',', grid_.y(hottest), ',', hotCount);
}

void Simulator::finish() {
    std::int64_t open = 0;
    for (const auto& order : orders_) {
        if (order.state == OrderState::Pending || order.state == OrderState::Assigned ||
            order.state == OrderState::Picked) ++open;
    }
    write("SUMMARY delivered=", delivered_, " lost=", lost_, " rejected=", rejected_,
          " cancelled=", cancelled_, " open=", open, '\n');
    const auto average = delivered_ == 0 ? 0 : (latencySum_ + delivered_ / 2) / delivered_;
    write("LATENCY sum=", latencySum_, " max=", latencyMax_, " avg=", average, '\n');
    for (const auto& robot : robots_)
        write("ROBOT ", robot.name, ' ', grid_.x(robot.cell), ' ', grid_.y(robot.cell),
              ' ', robot.battery, ' ', stateNames[static_cast<int>(robot.state)],
              ' ', robot.steps, ' ', robot.waits, '\n');
}

void Simulator::run(std::ostream& output) {
    output_ = &output;
    outputBuffer_.reserve(65792);
    for (int tick = 0; tick < ticks_; ++tick) {
        events(tick);
        rescue(tick);
        charge(tick);
        dispatch(tick);
        seekChargers(tick);
        move(tick);
        if ((tick + 1) % parameters_.reportEvery == 0) report(tick);
    }
    finish();
    flush();
}

void simulate(std::istream& input, std::ostream& output) {
    InputBuffer scanner(input);
    if (scanner.empty()) return;
    const int width = scanner.integer();
    const int height = scanner.integer();
    const int ticks = scanner.integer();
    std::vector<std::string> map(height);
    for (auto& row : map) {
        const auto token = scanner.word();
        row.assign(token.data(), static_cast<std::size_t>(width));
    }
    Grid grid(std::move(map));
    Parameters parameters{};
    scanner.word(); // PARAMS
    parameters.batteryMax = scanner.integer();
    parameters.chargeRate = scanner.integer();
    parameters.lowThreshold = scanner.integer();
    parameters.safetyMargin = scanner.integer();
    parameters.reportEvery = scanner.integer();
    parameters.hotRadius = scanner.integer();
    scanner.word(); // ROBOTS
    const int robotCount = scanner.integer();
    std::vector<Robot> robots(robotCount);
    for (int i = 0; i < robotCount; ++i) {
        const int x = scanner.integer();
        const int y = scanner.integer();
        robots[i].cell = grid.cell(x, y);
        robots[i].battery = parameters.batteryMax;
        robots[i].name = "R" + std::to_string(i);
    }
    scanner.word(); // EVENTS
    const int eventCount = scanner.integer();
    std::vector<Event> events;
    events.reserve(eventCount);
    for (int i = 0; i < eventCount; ++i) {
        Event event{};
        event.tick = scanner.integer();
        const char kind = scanner.word().front();
        if (kind == 'O') {
            event.kind = EventKind::Order;
            event.id = scanner.integer();
            const int px = scanner.integer();
            const int py = scanner.integer();
            const int dx = scanner.integer();
            const int dy = scanner.integer();
            event.priority = scanner.integer();
            event.first = grid.cell(px, py);
            event.second = grid.cell(dx, dy);
        } else if (kind == 'C') {
            event.kind = EventKind::Cancel;
            event.id = scanner.integer();
        } else {
            const int x = scanner.integer();
            const int y = scanner.integer();
            event.kind = kind == 'B' ? EventKind::Block : EventKind::Unblock;
            event.first = grid.cell(x, y);
        }
        events.push_back(event);
    }
    Simulator(std::move(grid), ticks, parameters, std::move(robots), std::move(events)).run(output);
}

}  // namespace traceflow
