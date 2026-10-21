#ifndef STEADYFLOW_MONOTONE_H
#define STEADYFLOW_MONOTONE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

using MonotoneRow = std::array<std::uint64_t, 4>;

// Reverse reachability inside the original source/target rectangle. It stays
// exact while the target is unchanged and no cell in that rectangle changes.
class MonotoneProof {
public:
    bool validFor(int x, int y, int tx, int ty) const {
        return valid_ && targetX_ == tx && targetY_ == ty && contains(x, y);
    }
    bool reachable(int x, int y) const {
        return valid_ && contains(x, y) &&
               (reach_[y - minY_][x >> 6] & (std::uint64_t(1) << (x & 63)));
    }
    void invalidateIfContains(int x, int y) {
        if (valid_ && contains(x, y)) valid_ = false;
    }

private:
    friend class MonotonePaths;
    bool contains(int x, int y) const {
        return x >= minX_ && x <= maxX_ && y >= minY_ && y <= maxY_;
    }
    bool valid_ = false;
    int targetX_ = -1, targetY_ = -1;
    int minX_ = 0, maxX_ = -1, minY_ = 0, maxY_ = -1;
    std::vector<MonotoneRow> reach_;
};

// Exact existence test for a passable path whose every step approaches the
// target. Such a path attains the Manhattan lower bound on any shortest path.
// A false result says nothing about longer paths; callers must then use BFS.
class MonotonePaths {
public:
    void reset(int height) { rows_.assign(height, {}); }

    void setPassable(int x, int y, bool passable) {
        std::uint64_t bit = std::uint64_t(1) << (x & 63);
        if (passable) rows_[y][x >> 6] |= bit;
        else rows_[y][x >> 6] &= ~bit;
    }

    void buildProof(int sx, int sy, int tx, int ty, MonotoneProof& proof) const {
        proof.targetX_ = tx;
        proof.targetY_ = ty;
        proof.minX_ = std::min(sx, tx);
        proof.maxX_ = std::max(sx, tx);
        proof.minY_ = std::min(sy, ty);
        proof.maxY_ = std::max(sy, ty);
        proof.reach_.assign(proof.maxY_ - proof.minY_ + 1, {});
        proof.valid_ = true;
        MonotoneRow mask = rectangleMask(sx, tx);
        MonotoneRow reach{};
        reach[tx >> 6] = std::uint64_t(1) << (tx & 63);
        int lo = proof.minX_ >> 6, hi = proof.maxX_ >> 6;
        int stepY = ty <= sy ? 1 : -1;
        for (int y = ty;; y += stepY) {
            bool any = advanceRow(reach, mask, y, lo, hi, tx <= sx);
            proof.reach_[y - proof.minY_] = reach;
            if (!any || y == sy) return;
        }
    }

    bool hasPath(int sx, int sy, int tx, int ty) const {
        const std::uint64_t sourceBit = std::uint64_t(1) << (sx & 63);
        const std::uint64_t targetBit = std::uint64_t(1) << (tx & 63);
        if (!(rows_[sy][sx >> 6] & sourceBit) || !(rows_[ty][tx >> 6] & targetBit)) return false;
        int lo = std::min(sx, tx) >> 6, hi = std::max(sx, tx) >> 6;
        MonotoneRow mask = rectangleMask(sx, tx);
        if (sy == ty) {
            for (int word = lo; word <= hi; ++word)
                if ((rows_[sy][word] & mask[word]) != mask[word]) return false;
            return true;
        }
        const int stepY = sy < ty ? 1 : -1;
        if (sx == tx) {
            for (int y = sy;; y += stepY) {
                if (!(rows_[y][sx >> 6] & sourceBit)) return false;
                if (y == ty) return true;
            }
        }
        MonotoneRow reach{};
        reach[sx >> 6] = sourceBit;
        const bool right = sx < tx;
        for (int y = sy;; y += stepY) {
            if (!advanceRow(reach, mask, y, lo, hi, right)) return false;
            if (y == ty) return (reach[tx >> 6] & targetBit) != 0;
        }
    }

private:
    static MonotoneRow rectangleMask(int sx, int tx) {
        int lo = std::min(sx, tx) >> 6, hi = std::max(sx, tx) >> 6;
        MonotoneRow mask{};
        for (int word = lo; word <= hi; ++word) mask[word] = ~std::uint64_t(0);
        mask[lo] &= ~std::uint64_t(0) << (std::min(sx, tx) & 63);
        mask[hi] &= ~std::uint64_t(0) >> (63 - (std::max(sx, tx) & 63));
        return mask;
    }

    bool advanceRow(MonotoneRow& reach, const MonotoneRow& mask, int y, int lo, int hi, bool right) const {
        std::uint64_t any = 0, carry = 0;
        if (right) {
            for (int word = lo; word <= hi; ++word) {
                std::uint64_t open = rows_[y][word] & mask[word];
                std::uint64_t r = (reach[word] | carry) & open;
                r |= (r << 1) & open; open &= open << 1;
                r |= (r << 2) & open; open &= open << 2;
                r |= (r << 4) & open; open &= open << 4;
                r |= (r << 8) & open; open &= open << 8;
                r |= (r << 16) & open; open &= open << 16;
                r |= (r << 32) & open;
                carry = r >> 63;
                reach[word] = r;
                any |= r;
            }
        } else {
            for (int word = hi; word >= lo; --word) {
                std::uint64_t open = rows_[y][word] & mask[word];
                std::uint64_t r = (reach[word] | (carry << 63)) & open;
                r |= (r >> 1) & open; open &= open >> 1;
                r |= (r >> 2) & open; open &= open >> 2;
                r |= (r >> 4) & open; open &= open >> 4;
                r |= (r >> 8) & open; open &= open >> 8;
                r |= (r >> 16) & open; open &= open >> 16;
                r |= (r >> 32) & open;
                carry = r & 1;
                reach[word] = r;
                any |= r;
            }
        }
        return any != 0;
    }

    // Official W <= 200: four 64-bit words cover every row.
    std::vector<MonotoneRow> rows_;
};

#endif
