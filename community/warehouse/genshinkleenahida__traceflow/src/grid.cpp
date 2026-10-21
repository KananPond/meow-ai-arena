#include "grid.hpp"

#include <algorithm>

namespace traceflow {

Grid::Grid(std::vector<std::string> cells)
    : width_(cells.empty() ? 0 : static_cast<int>(cells.front().size())),
      height_(static_cast<int>(cells.size())),
      size_(width_ * height_),
      walls_(size_), blocked_(size_, 0), neighbors_(size_),
      slotForSource_(size_, -1), queue_(size_),
      unreachable_(size_, kInf), chargerDistances_(size_, kInf),
      changedStamp_(size_, 0), oldBlocked_(size_, 0), enqueuedStamp_(size_, 0) {
    for (int cy = 0; cy < height_; ++cy) {
        for (int cx = 0; cx < width_; ++cx) {
            const int id = cell(cx, cy);
            walls_[id] = cells[cy][cx] == '#';
            neighbors_[id] = {cell(cx, cy - 1), cell(cx + 1, cy),
                              cell(cx, cy + 1), cell(cx - 1, cy)};
            if (cells[cy][cx] == 'C') chargers_.push_back(id);
        }
    }

    // Each static root stores a uint16 distance and one reachability byte per
    // cell. All finite paths fit below 65534 on a legal 200 x 200 grid.
    staticCapacity_ = std::max(1, std::min(512,
        static_cast<int>((64u * 1024u * 1024u) / (3u * size_))));
    initialDistance_.resize(size_);
    dynamicInitial_.resize(size_);
    for (int i = 0; i < size_; ++i) {
        dynamicInitial_[i] = walls_[i] ? -1 : kInf;
        if (walls_[i]) wallCells_.push_back(i);
    }
    for (int i = 0; i < size_; ++i) initialDistance_[i] = walls_[i] ? 65534 : 65535;
    staticSlot_.assign(size_, -1);
    blockedSlot_.assign(size_, -1);
    staticCache_.reserve(staticCapacity_);

    // Include both permanent distance tables in the 96 MiB distance budget.
    // The legal 200 x 200 grid leaves room for hundreds of cached BFS roots.
    constexpr std::size_t distanceBudget = 96u * 1024u * 1024u;
    if (size_ > 0) {
        const std::size_t tableBytes = sizeof(int) * static_cast<std::size_t>(size_);
        const std::size_t tableCount = distanceBudget / tableBytes - 2;
        cacheCapacity_ = static_cast<int>(std::min(tableCount,
                                                 static_cast<std::size_t>(size_)));
    }
}

int Grid::cell(int cx, int cy) const {
    if (cx < 0 || cy < 0 || cx >= width_ || cy >= height_) return -1;
    return cy * width_ + cx;
}

int Grid::x(int id) const { return valid(id) ? id % width_ : -1; }
int Grid::y(int id) const { return valid(id) ? id / width_ : -1; }

bool Grid::wall(int id) const { return !valid(id) || walls_[id] != 0; }

bool Grid::passable(int id) const {
    return valid(id) && walls_[id] == 0 && blocked_[id] == 0;
}

const std::array<int, 4>& Grid::neighbors(int id) const {
    static const std::array<int, 4> missing = {-1, -1, -1, -1};
    return valid(id) ? neighbors_[id] : missing;
}

bool Grid::block(int id, bool blocked) {
    if (wall(id) || (blocked_[id] != 0) == blocked) return false;
    if (blocked) {
        blockedSlot_[id] = static_cast<int>(activeBlocked_.size());
        activeBlocked_.push_back(id);
    } else {
        const int slot = blockedSlot_[id], last = activeBlocked_.back();
        activeBlocked_[slot] = last;
        blockedSlot_[last] = slot;
        activeBlocked_.pop_back();
        blockedSlot_[id] = -1;
    }
    blocked_[id] = static_cast<unsigned char>(blocked);
    dynamicInitial_[id] = blocked ? -1 : kInf;
    ++version_;

    changes_.push_back({id, blocked});
    return true;
}

void Grid::detach(int slot) {
    CacheEntry& entry = cache_[slot];
    if (entry.previous >= 0) cache_[entry.previous].next = entry.next;
    else oldest_ = entry.next;
    if (entry.next >= 0) cache_[entry.next].previous = entry.previous;
    else newest_ = entry.previous;
    entry.previous = entry.next = -1;
}

void Grid::makeNewest(int slot) {
    CacheEntry& entry = cache_[slot];
    entry.previous = newest_;
    entry.next = -1;
    if (newest_ >= 0) cache_[newest_].next = slot;
    else oldest_ = slot;
    newest_ = slot;
}

void Grid::fillDistances(int source, std::vector<int>& distances) {
    // -1 temporarily distinguishes walls/closures from unvisited floor. This
    // removes repeated passability checks in BFS; callers always receive kInf
    // for unavailable cells, except the baseline's explicitly seeded source.
    distances = dynamicInitial_;
    std::size_t head = 0, tail = 0;
    distances[source] = 0;
    queue_[tail++] = source;
    while (head < tail) {
        const int current = queue_[head++];

        const int nextDistance = distances[current] + 1;
        for (int neighbor : neighbors_[current]) {
            if (neighbor < 0 || distances[neighbor] != kInf) continue;
            distances[neighbor] = nextDistance;
            queue_[tail++] = neighbor;
        }
    }
    for (int cell : wallCells_) if (cell != source) distances[cell] = kInf;
    for (int cell : activeBlocked_) if (cell != source) distances[cell] = kInf;
}

void Grid::repairDistances(int source, std::vector<int>& distances,
                           std::uint64_t oldVersion) {
    // Catching up a cold tree over many graph edits is cheaper with one BFS.
    // This threshold changes work only, never the resulting distances.
    if (version_ - oldVersion > 256) {
        fillDistances(source, distances);
        return;
    }
    ++repairStamp_;
    changedCells_.clear();
    for (std::uint64_t i = oldVersion; i < version_; ++i) {
        const auto& change = changes_[i];
        if (changedStamp_[change.cell] == repairStamp_) continue;
        changedStamp_[change.cell] = repairStamp_;
        oldBlocked_[change.cell] = static_cast<unsigned char>(!change.blocked);
        changedCells_.push_back(change.cell);
    }

    // Remove closed vertices from the old shortest-path DAG. A reachable
    // vertex becomes invalid only after its last distance-1 predecessor goes.
    // This avoids count-to-infinity on components disconnected by a closure.
    invalidated_.clear();
    for (int cell : changedCells_) {
        if (oldBlocked_[cell] == blocked_[cell] || !blocked_[cell] ||
            cell == source || distances[cell] == kInf) continue;
        invalidated_.emplace_back(cell, distances[cell]);
        distances[cell] = kInf;
    }
    const std::size_t repairLimit = static_cast<std::size_t>(std::max(32, size_ / 8));
    for (std::size_t head = 0; head < invalidated_.size(); ++head) {
        if (invalidated_.size() > repairLimit) {
            fillDistances(source, distances);
            return;
        }
        const auto [cell, oldDistance] = invalidated_[head];
        for (int next : neighbors_[cell]) {
            if (!passable(next) || distances[next] != oldDistance + 1) continue;
            bool supported = false;
            for (int predecessor : neighbors_[next]) {
                if (valid(predecessor) &&
                    (predecessor == source || passable(predecessor)) &&
                    distances[predecessor] == distances[next] - 1) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                invalidated_.emplace_back(next, distances[next]);
                distances[next] = kInf;
            }
        }
    }

    // All finite remaining labels describe valid routes in the new graph.
    // Seed the invalidated region's boundary and newly opened vertices, then
    // correct labels with a bounded FIFO. Every finite label is a valid route;
    // relaxations only decrease it. Large repairs revert to one full BFS.
    repairQueue_.clear();
    const auto enqueue = [this](int cell) {
        if (enqueuedStamp_[cell] == repairStamp_) return;
        enqueuedStamp_[cell] = repairStamp_;
        repairQueue_.push_back(cell);
    };
    const auto addSeed = [this, source, &distances, &enqueue](int cell) {
        if (cell == source || !passable(cell)) return;
        int best = distances[cell];
        for (int predecessor : neighbors_[cell]) {
            if (valid(predecessor) &&
                (predecessor == source || passable(predecessor)) &&
                distances[predecessor] < best)
                best = std::min(best, distances[predecessor] + 1);
        }
        if (best >= distances[cell]) return;
        distances[cell] = best;
        enqueue(cell);
    };
    for (const auto& [cell, oldDistance] : invalidated_) {
        (void)oldDistance;
        addSeed(cell);
    }
    for (int cell : changedCells_) {
        if (oldBlocked_[cell] != blocked_[cell] && !blocked_[cell]) addSeed(cell);
    }

    std::size_t relaxations = 0;
    for (std::size_t head = 0; head < repairQueue_.size(); ++head) {
        const int cell = repairQueue_[head];
        enqueuedStamp_[cell] = 0;
        const int distance = distances[cell];
        for (int next : neighbors_[cell]) {
            if (!passable(next) || distances[next] <= distance + 1) continue;
            distances[next] = distance + 1;
            enqueue(next);
            if (++relaxations > repairLimit) {
                fillDistances(source, distances);
                return;
            }
        }
    }
}

const std::vector<int>& Grid::distancesFrom(int source) {

    if (!valid(source)) return unreachable_;
    int slot = slotForSource_[source];
    if (slot >= 0) {

        if (slot != newest_) {
            detach(slot);
            makeNewest(slot);
        }
        CacheEntry& entry = cache_[slot];
        if (entry.version != version_) {
            repairDistances(source, entry.distances, entry.version);
            entry.version = version_;
        }
        return entry.distances;
    }

    if (usedSlots_ < cacheCapacity_) {
        slot = usedSlots_++;
        if (slot == static_cast<int>(cache_.size())) cache_.emplace_back();
    } else {

        slot = oldest_;
        slotForSource_[cache_[slot].source] = -1;
        detach(slot);
    }
    CacheEntry& entry = cache_[slot];
    entry.source = source;
    entry.version = version_;
    slotForSource_[source] = slot;
    makeNewest(slot);
    fillDistances(source, entry.distances);
    return entry.distances;
}

const std::vector<int>& Grid::distancesTo(int target) {
    if (!passable(target)) return unreachable_;
    // On the passable subgraph every edge is undirected. Only a blocked
    // *source* needs the asymmetric first-step treatment below.
    return distancesFrom(target);
}

int Grid::fromDistances(int source, int target, const std::vector<int>& d) const {
    if (!valid(source) || !valid(target)) return kInf;
    if (source == target) return 0;
    if (!passable(target)) return kInf;
    if (passable(source)) return d[source];

    int best = kInf;
    for (int neighbor : neighbors_[source]) {
        if (passable(neighbor) && d[neighbor] < best) {
            best = std::min(best, d[neighbor] + 1);
        }
    }
    return best;
}

int Grid::distance(int from, int to) {
    if (!valid(from) || !valid(to)) return kInf;
    if (from == to) return 0;
    if (!passable(to)) return kInf;
    auto& e = staticTo(to);
    if (passable(from)) {
        if (e.distance[from] == 65535) return kInf;
        if (shortestOpen(e, from)) return e.distance[from];
    } else {
        int lower = 65535;
        for (int n : neighbors_[from]) {
            if (passable(n)) lower = std::min(lower, int(e.distance[n]));
        }
        if (lower == 65535) return kInf;
        for (int n : neighbors_[from])
            if (passable(n) && e.distance[n] == lower && shortestOpen(e, n)) return lower + 1;
    }
    return fromDistances(from, to, distancesTo(to));
}

void Grid::refreshChargerDistances() {
    if (chargerVersion_ == version_) return;

    chargerDistances_ = dynamicInitial_;
    std::size_t head = 0, tail = 0;
    for (int charger : chargers_) {
        if (!passable(charger)) continue;
        chargerDistances_[charger] = 0;
        queue_[tail++] = charger;
    }
    while (head < tail) {
        const int current = queue_[head++];

        const int nextDistance = chargerDistances_[current] + 1;
        for (int neighbor : neighbors_[current]) {
            if (chargerDistances_[neighbor] != kInf) continue;
            chargerDistances_[neighbor] = nextDistance;
            queue_[tail++] = neighbor;
        }
    }
    for (int cell : wallCells_) chargerDistances_[cell] = kInf;
    for (int cell : activeBlocked_) chargerDistances_[cell] = kInf;
    chargerVersion_ = version_;
}

int Grid::chargerDistance(int from) {
    if (!valid(from)) return kInf;
    auto& entry = staticChargers();
    if (passable(from)) {
        if (entry.distance[from] == 65535) return kInf;
        if (shortestOpen(entry, from)) return entry.distance[from];
    } else {
        int lower = 65535;
        for (int next : neighbors_[from]) {
            if (passable(next)) lower = std::min(lower, int(entry.distance[next]));
        }
        if (lower == 65535) return kInf;
        for (int next : neighbors_[from]) {
            if (passable(next) && entry.distance[next] == lower && shortestOpen(entry, next))
                return lower + 1;
        }
    }
    refreshChargerDistances();
    if (passable(from)) return chargerDistances_[from];
    int best = kInf;
    for (int neighbor : neighbors_[from]) {
        if (passable(neighbor) && chargerDistances_[neighbor] < best)
            best = std::min(best, chargerDistances_[neighbor] + 1);
    }
    return best;
}

Grid::StaticEntry& Grid::staticTo(int target, bool repair) {
    int slot = staticSlot_[target];
    if (slot >= 0) {
        auto& entry = staticCache_[slot];
        entry.age = ++staticClock_;
        if (repair) repairCertificate(entry);
        return entry;
    }
    if (int(staticCache_.size()) < staticCapacity_) {
        slot = static_cast<int>(staticCache_.size());
        staticCache_.emplace_back();
    } else {
        slot = 0;
        for (int i = 1; i < staticCapacity_; ++i) {
            if (staticCache_[i].age < staticCache_[slot].age) slot = i;
        }
        staticSlot_[staticCache_[slot].source] = -1;
    }
    auto& entry = staticCache_[slot];
    entry.source = target;
    entry.age = ++staticClock_;
    entry.distance = initialDistance_;
    entry.reachable.assign(size_, 0);
    staticSlot_[target] = slot;
    int head = 0, tail = 0;
    entry.distance[target] = 0;
    entry.reachable[target] = 1;
    queue_[tail++] = target;

    // Legal maps have a solid wall border: all queued floor is interior, and
    // the number of queued cells is strictly smaller than size_. Thus each
    // neighbor and the unconditional spare-slot queue write are in range.
    auto* const distance = entry.distance.data();
    auto* const queue = queue_.data();
    while (head < tail) {
        const int cell = queue[head++];
        const std::uint16_t nextDistance = distance[cell] + 1;
        const auto visit = [&](int next) {
            const std::uint16_t old = distance[next];
            const bool unseen = old == 65535;
            distance[next] = unseen ? nextDistance : old;
            queue[tail] = next;
            tail += unseen;
        };
        visit(cell - width_);
        visit(cell + 1);
        visit(cell + width_);
        visit(cell - 1);
    }
    for (int i = 0; i < size_; ++i) entry.reachable[i] = distance[i] < 65534;
    closeCertificate(entry, activeBlocked_);
    entry.version = version_;
    return entry;
}

void Grid::rebuildCertificate(StaticEntry& entry) {
    for (int i = 0; i < size_; ++i) entry.reachable[i] = entry.distance[i] < 65534;
    closeCertificate(entry, activeBlocked_);
    entry.version = version_;
}

void Grid::repairCertificate(StaticEntry& entry) {
    if (entry.version == version_) return;
    if (version_ - entry.version > 64) {
        rebuildCertificate(entry);
        return;
    }
    ++repairStamp_;
    changedCells_.clear();
    for (auto i = entry.version; i < version_; ++i) {
        const int cell = changes_[i].cell;
        if (changedStamp_[cell] != repairStamp_) {
            changedStamp_[cell] = repairStamp_;
            changedCells_.push_back(cell);
        }
    }
    closeCertificate(entry, changedCells_);

    // A certificate means that a path descending the immutable static
    // distance at every step still exists. Openings propagate support along
    // increasing-distance edges; closures remove only unsupported vertices.
    int head = 0, tail = 0;
    for (int cell : changedCells_) {
        if (!passable(cell) || entry.reachable[cell]) continue;
        bool supported = entry.distance[cell] == 0;
        if (!supported) {
            for (int predecessor : neighbors_[cell]) {
                if (passable(predecessor) && entry.reachable[predecessor] &&
                    entry.distance[predecessor] + 1 == entry.distance[cell]) {
                    supported = true;
                    break;
                }
            }
        }
        if (supported) {
            entry.reachable[cell] = 1;
            queue_[tail++] = cell;
        }
    }
    while (head < tail) {
        const int cell = queue_[head++];
        for (int next : neighbors_[cell]) {
            if (passable(next) && !entry.reachable[next] &&
                entry.distance[next] == entry.distance[cell] + 1) {
                entry.reachable[next] = 1;
                queue_[tail++] = next;
            }
        }
    }
    entry.version = version_;
}

bool Grid::shortestOpen(StaticEntry& entry, int source) {
    return entry.reachable[source];
}

bool Grid::canonicalRoute(int source, int target, std::vector<int>& route) {
    route.clear();
    if (wall(source) || !passable(target)) return false;
    if (source == target) return true;
    // Only static distances are needed: an existing tree need not repair its
    // whole shortest-path DAG when one concrete path supplies the witness.
    const auto& entry = staticTo(target, false);
    if (entry.distance[source] >= 65534) return false;
    int current = source;
    while (current != target) {
        int next = -1;
        for (int neighbor : neighbors_[current]) {
            if (valid(neighbor) && entry.distance[neighbor] + 1 == entry.distance[current]) {
                next = neighbor;
                break;
            }
        }
        // Do not skip a closed canonical choice to select a later neighbor:
        // reopening that earlier choice would change baseline's direction tie.
        if (next < 0 || !passable(next)) {
            route.clear();
            return false;
        }
        route.push_back(next);
        current = next;
    }
    return true;
}

bool Grid::validateRoute(const std::vector<int>& route, std::size_t cursor,
                         const std::vector<std::uint16_t>& positions,
                         std::uint64_t previousVersion) const {
    for (std::uint64_t i = previousVersion; i < version_; ++i) {
        const Change& change = changes_[i];
        if (!change.blocked || !blocked_[change.cell]) continue;
        const std::size_t position = positions[change.cell];
        // Positions need not be cleared when replacing a route: the cell
        // identity rejects stale marks. The current source is already consumed.
        if (position >= cursor && position < route.size() && route[position] == change.cell)
            return false;
    }
    return true;
}

int Grid::nextStep(int source, int target) {
    if (!passable(target)) return -1;
    auto& entry = staticTo(target);
    int lower = 65535;
    for (int next : neighbors_[source]) {
        if (passable(next)) lower = std::min(lower, int(entry.distance[next]));
    }
    if (lower == 65535) return -1;
    for (int next : neighbors_[source]) {
        if (passable(next) && entry.distance[next] == lower && entry.reachable[next])
            return next;
    }

    // If no smallest-static-distance neighbor has a certificate, a detour
    // might change both the minimum and its tie. Compare exact dynamic values.
    const auto& distances = distancesTo(target);
    int next = -1, best = kInf;
    for (int neighbor : neighbors_[source]) {
        if (passable(neighbor) && distances[neighbor] < best) {
            next = neighbor;
            best = distances[neighbor];
        }
    }
    return next;
}

Grid::StaticEntry& Grid::staticChargers() {
    if (staticChargersReady_) {
        repairCertificate(staticChargers_);
        return staticChargers_;
    }
    auto& entry = staticChargers_;
    entry.source = -2;
    entry.distance.resize(size_);
    entry.reachable.assign(size_, 0);
    for (int i = 0; i < size_; ++i) entry.distance[i] = walls_[i] ? 65534 : 65535;
    int head = 0, tail = 0;
    for (int charger : chargers_) {
        entry.distance[charger] = 0;
        entry.reachable[charger] = 1;
        queue_[tail++] = charger;
    }
    while (head < tail) {
        const int cell = queue_[head++];
        const int nextDistance = entry.distance[cell] + 1;
        for (int next : neighbors_[cell]) {
            if (entry.distance[next] == 65535) {
                entry.distance[next] = nextDistance;
                entry.reachable[next] = 1;
                queue_[tail++] = next;
            }
        }
    }
    closeCertificate(entry, activeBlocked_);
    entry.version = version_;
    staticChargersReady_ = true;
    return entry;
}

void Grid::closeCertificate(StaticEntry& entry, const std::vector<int>& closed) {
    int head = 0, tail = 0;
    for (int cell : closed) {
        if (blocked_[cell] && entry.reachable[cell]) {
            entry.reachable[cell] = 0;
            queue_[tail++] = cell;
        }
    }
    while (head < tail) {
        const int cell = queue_[head++];
        for (int next : neighbors_[cell]) {
            if (!passable(next) || !entry.reachable[next] ||
                entry.distance[next] != entry.distance[cell] + 1) continue;
            bool supported = false;
            for (int predecessor : neighbors_[next]) {
                if (passable(predecessor) && entry.reachable[predecessor] &&
                    entry.distance[predecessor] + 1 == entry.distance[next]) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                entry.reachable[next] = 0;
                queue_[tail++] = next;
            }
        }
    }
}

} // namespace traceflow
