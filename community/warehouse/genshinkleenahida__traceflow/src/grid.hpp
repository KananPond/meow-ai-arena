#ifndef TRACEFLOW_GRID_HPP
#define TRACEFLOW_GRID_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace traceflow {

constexpr int kInf = 1000000000;

// Static walls and dynamic closures form the route graph. Robot occupancy is
// deliberately absent: the baseline only checks it when executing a move.
class Grid {
public:
    explicit Grid(std::vector<std::string> cells);

    int width() const { return width_; }
    int height() const { return height_; }
    int size() const { return size_; }
    int cell(int x, int y) const;
    int x(int id) const;
    int y(int id) const;
    bool wall(int id) const;
    bool passable(int id) const;

    // The order is part of the simulation semantics: up, right, down, left.
    const std::array<int, 4>& neighbors(int id) const;
    const std::vector<int>& chargers() const { return chargers_; }

    // Invalid coordinates, walls, and repeated identical operations are no-ops.
    // Only an actual topology change invalidates cached distances.
    bool block(int id, bool blocked);
    std::uint64_t version() const { return version_; }

    // References may be invalidated by the next cache query or topology change.
    // A blocked target has no reverse-distance table and returns all kInf.
    const std::vector<int>& distancesTo(int target);
    // Baseline BFS seeds its source even when that source is blocked. It may
    // leave that cell, but may only enter passable cells after the first step.
    const std::vector<int>& distancesFrom(int source);

    int fromDistances(int source, int target, const std::vector<int>& d) const;
    int distance(int from, int to);
    int chargerDistance(int from);
    int nextStep(int source, int target);
    // Build the static graph's canonical shortest path, excluding its source.
    // It is usable only when every subsequent cell is currently open. Since
    // the canonical choice ignores closures, UNBLOCK cannot supersede it.
    bool canonicalRoute(int source, int target, std::vector<int>& route);
    bool validateRoute(const std::vector<int>& route, std::size_t cursor,
                       const std::vector<std::uint16_t>& positions,
                       std::uint64_t previousVersion) const;

private:
    struct StaticEntry {
        int source = -1;
        std::uint64_t age = 0;
        std::vector<std::uint16_t> distance;
        std::uint64_t version = 0;
        std::vector<unsigned char> reachable;
        std::uint64_t detourVersion = std::numeric_limits<std::uint64_t>::max();
        unsigned char detourQueries = 0;
    };
    StaticEntry& staticTo(int target, bool repair = true);
    StaticEntry& staticChargers();
    StaticEntry staticChargers_;
    bool staticChargersReady_ = false;
    bool shortestOpen(StaticEntry& entry, int source);
    // -2 means that bounded local work was exhausted; retain the exact-field
    // fallback. Otherwise return the canonical first step, or -1 if unreachable.
    int detourStep(int source, const StaticEntry& entry);
    void repairCertificate(StaticEntry& entry);
    void closeCertificate(StaticEntry& entry, const std::vector<int>& closed);
    std::vector<int> activeBlocked_, blockedSlot_;
    std::vector<std::uint16_t> initialDistance_;
    void rebuildCertificate(StaticEntry& entry);
    int staticCapacity_ = 1;
    std::uint64_t staticClock_ = 0;
    std::vector<int> staticSlot_;
    std::vector<StaticEntry> staticCache_;
    struct CacheEntry {
        int source = -1;
        int previous = -1;
        int next = -1;
        std::uint64_t version = 0;
        std::vector<int> distances;
    };

    struct Change { int cell; bool blocked; };
    void repairDistances(int source, std::vector<int>& distances, std::uint64_t oldVersion);
    bool valid(int id) const { return id >= 0 && id < size_; }
    void detach(int slot);
    void makeNewest(int slot);
    void fillDistances(int source, std::vector<int>& distances);
    void refreshChargerDistances();

    int width_ = 0;
    int height_ = 0;
    int size_ = 0;
    std::vector<unsigned char> walls_;
    std::vector<unsigned char> blocked_;
    std::vector<int> dynamicInitial_, wallCells_;
    std::vector<std::array<int, 4>> neighbors_;
    std::vector<int> chargers_;
    std::uint64_t version_ = 0;

    // Each cell directly identifies its cached BFS slot. LRU links avoid a
    // linear eviction scan; allocated tables are reused after map changes.
    int cacheCapacity_ = 1;
    int usedSlots_ = 0;
    int oldest_ = -1;
    int newest_ = -1;
    std::vector<int> slotForSource_;
    std::vector<CacheEntry> cache_;
    std::vector<int> queue_;
    std::vector<int> unreachable_;
    std::vector<int> chargerDistances_;
    std::uint64_t chargerVersion_ = std::numeric_limits<std::uint64_t>::max();
    // A tree catches up only when queried. Changes are bounded by the input's
    // 200,000 events; scratch buffers are shared, never allocated per tree.
    std::vector<Change> changes_;
    std::vector<std::uint64_t> changedStamp_;
    std::vector<unsigned char> oldBlocked_;
    std::uint64_t repairStamp_ = 0;
    std::vector<int> changedCells_;
    std::vector<std::pair<int, int>> invalidated_;
    std::vector<int> repairQueue_;
    std::vector<std::uint64_t> enqueuedStamp_;
    // One shared, stamped workspace avoids clearing an entire map per point
    // query. Eight cyclic buckets hold the integer key 4*detours + firstRank.
    std::vector<int> detourKey_;
    std::vector<std::uint32_t> detourSeen_;
    std::uint32_t detourStamp_ = 0;
    std::array<std::vector<std::pair<int, int>>, 8> detourBuckets_;
};

} // namespace traceflow

#endif
