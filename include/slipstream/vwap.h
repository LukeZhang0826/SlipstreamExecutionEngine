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
        while (!quotes_.empty() && quotes_.front().ts_ns + window_ns_ <= now_ns) {
            sum_quote_vwap_ -= quotes_.front().quote_vwap;
            quotes_.pop_front();
        }
        if (now_ns > now_ns_) now_ns_ = now_ns;
    };

    void on_quote(const Quote& q) {
        if (!seen_any_) {
            first_ts_ns_ = q.ts_ns;
            seen_any_ = true;
        }
        ++quotes_seen_;
        advance(q.ts_ns);

        const int64_t total_qty = static_cast<int64_t>(q.bid_qty) + static_cast<int64_t>(q.ask_qty);
        if (total_qty == 0) return;

        const int64_t numerator = q.bid_px * static_cast<int64_t>(q.bid_qty) + q.ask_px * static_cast<int64_t>(q.ask_qty);
        const int64_t quote_vwap = numerator / total_qty;

        quotes_.push_back(Entry{q.ts_ns, quote_vwap});
        sum_quote_vwap_ += quote_vwap;
    }

    [[nodiscard]] bool value(int64_t& out) const {
        if (quotes_.empty()) return false;
        out = sum_quote_vwap_ / static_cast<int64_t>(quotes_.size());
        return true;
    }

    [[nodiscard]] bool has_full_window() const {
        return seen_any_ && now_ns_ - first_ts_ns_ >= window_ns_;
    }

    [[nodiscard]] size_t quotes_seen() const { return quotes_seen_; }

    [[nodiscard]] size_t size() const { return quotes_.size(); }

private:
    struct Entry {
        uint64_t ts_ns;
        int64_t quote_vwap;
    };

    std::deque<Entry> quotes_;
    uint64_t window_ns_;
    uint64_t now_ns_ = 0;
    uint64_t first_ts_ns_ = 0;
    size_t quotes_seen_ = 0;
    bool seen_any_ = false;
    int64_t sum_quote_vwap_ = 0;
};


#endif //SLIPSTREAMEXECUTIONENGINE_VWAP_H
