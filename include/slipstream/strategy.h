//
// Created by lukeypookster on 11/25/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_STRATEGY_H
#define SLIPSTREAMEXECUTIONENGINE_STRATEGY_H

#include <cstdint>
#include "book.h"
#include "config.h"
#include "messages.h"
#include "vwap.h"

// Suppress means stay silent
enum class Verdict : uint8_t { Suppress, Accept, Reject };

enum class RejectReason : uint8_t {
    None,
    UnknownAggressor,       // aggressor was neither 'B' or 'S'
    NoBook,                 // no quote has arrived
    ZeroQty,                //
    OverMaxQuantity,        // larger than --max-quantity
    QuoteOutsideBand,       // the quote is not cheap or rich enough against V
    TradePriceMissesQuote,  // we were not willing to pay the ask, or accept the bid
    ParticipationCap,       // would push us past cap times the observed vol
};

inline constexpr size_t kRejectReasonCount = 8;

[[nodiscard]] const char* to_string(RejectReason r);

struct Decision {
    Verdict verdict = Verdict::Suppress;
    RejectReason reason = RejectReason::None;
    char side = '?';
    uint32_t qty = 0;
    int64_t limit_px = 0;
};

class Strategy {
public:
    explicit Strategy(const Config& cfg);

    void on_quote(const Quote& q);

    [[nodiscard]] Decision judge(const Trade& t, const L1& book, const RollingVwap& vwap);

    [[nodiscard]] uint64_t bought_qty() const { return bought_qty_; }
    [[nodiscard]] uint64_t sold_qty() const { return sold_qty_; }
    [[nodiscard]] uint64_t observed_ask_qty() const { return observed_ask_qty_; }
    [[nodiscard]] uint64_t observed_bid_qty() const { return observed_bid_qty_; }

    [[nodiscard]] uint64_t buy_participation_ppm() const;
    [[nodiscard]] uint64_t sell_participation_ppm() const;

private:
    [[nodiscard]] bool warm(const RollingVwap& vwap) const;

    [[nodiscard]] bool within_cap(uint64_t already, uint32_t extra, uint64_t observed) const;

    uint32_t max_quantity_;
    uint32_t participation_cap_ppm_;
    uint32_t band_bps_x100_;

    uint64_t observed_bid_qty_ = 0;
    uint64_t observed_ask_qty_ = 0;
    uint64_t sold_qty_ = 0;
    uint64_t bought_qty_ = 0;
};






#endif //SLIPSTREAMEXECUTIONENGINE_STRATEGY_H
