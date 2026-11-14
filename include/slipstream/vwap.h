//
// Created by lukeypookster on 11/13/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_VWAP_H
#define SLIPSTREAMEXECUTIONENGINE_VWAP_H
#include <cstdint>
#include <deque>

#include "messages.h"

class RollingVwap {
public:
    explicit RollingVwap(const uint64_t window_ms)
        : window_ns_(window_ms * 1000000) {}

    void advance(const uint64_t now_ns) {
        while (!trades_.empty() && trades_.front().ts_ns + window_ns_ <= now_ns) {
            const Entry& old = trades_.front();
            sum_pxqty_ -= old.px * static_cast<int64_t>(old.qty);
            sum_qty_ -= static_cast<int64_t>(old.qty);
            trades_.pop_front();
        }
        if (now_ns > now_ns_) now_ns_ = now_ns;
    };

    void on_trade(const Trade& t) {
        if (!seen_any_) {
            first_ts_ns_ = t.ts_ns;
            seen_any_ = true;
        }
        advance(t.ts_ns);
        trades_.push_back(Entry{t.ts_ns, t.px, t.qty});
        sum_pxqty_ += t.px * static_cast<int64_t>(t.qty);
        sum_qty_ += static_cast<int64_t>(t.qty);
    }

    [[nodiscard]] bool value(int64_t& out) const {
        if (sum_qty_ <= 0) return false;
        out = sum_pxqty_ / sum_qty_;
        return true;
    }

    [[nodiscard]] bool has_full_window() const {
        return seen_any_ && now_ns_ - first_ts_ns_ >= window_ns_;
    }

    [[nodiscard]] int64_t window_qty() const { return sum_qty_; }

    [[nodiscard]] size_t size() const { return trades_.size(); }

private:
    struct Entry {
        uint64_t ts_ns;
        int64_t px;
        uint32_t qty;
    };

    std::deque<Entry> trades_;
    uint64_t window_ns_;
    uint64_t now_ns_ = 0;
    uint64_t first_ts_ns_ = 0;
    bool seen_any_ = false;
    int64_t sum_pxqty_ = 0;
    int64_t sum_qty_ = 0;
};


#endif //SLIPSTREAMEXECUTIONENGINE_VWAP_H
