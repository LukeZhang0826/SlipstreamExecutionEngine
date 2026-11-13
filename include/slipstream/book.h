//
// Created by lukeypookster on 11/13/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_BOOK_H
#define SLIPSTREAMEXECUTIONENGINE_BOOK_H

#include <string_view>
#include <cstring>
#include <algorithm>

#include "messages.h"

class SymbolFilter {
public:
    explicit SymbolFilter(std::string_view symbol) {
        std::memcpy(want_, symbol.data(), std::min(symbol.size(), sizeof(want_)));
    }

    [[nodiscard]] bool matches(const char (&symbol)[12]) const {
        return std::memcmp(symbol, want_, sizeof(want_)) == 0;
    }

private:
    char want_[12]{};
};

struct L1 {
    uint64_t ts_ns   = 0;
    int64_t  bid_px  = 0;
    uint32_t bid_qty = 0;
    int64_t  ask_px  = 0;
    uint32_t ask_qty = 0;
    bool     valid   = false;

    void apply(const Quote& q) {
        ts_ns   = q.ts_ns;
        bid_px  = q.bid_px;
        bid_qty = q.bid_qty;
        ask_px  = q.ask_px;
        ask_qty = q.ask_qty;
        valid   = true;
    }
};

#endif //SLIPSTREAMEXECUTIONENGINE_BOOK_H
