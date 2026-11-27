//
// Created by lukeypookster on 11/25/26.
//

#include "slipstream/strategy.h"

namespace {
    constexpr int64_t kBandScale = 1'000'000;
    constexpr size_t kMinQuotes = 10;
    constexpr uint64_t kCapScale = 1'000'000;
}

const char* to_string(const RejectReason r) {
    switch (r) {
        case RejectReason::None:                  return "none";
        case RejectReason::UnknownAggressor:      return "unknown aggressor";
        case RejectReason::NoBook:                return "no book";
        case RejectReason::ZeroQty:               return "zero qty";
        case RejectReason::OverMaxQuantity:       return "over max quantity";
        case RejectReason::QuoteOutsideBand:      return "quote outside band";
        case RejectReason::TradePriceMissesQuote: return "trade price misses quote";
        case RejectReason::ParticipationCap:      return "participation cap";
    }
    return "unknown";
}

Strategy::Strategy(const Config& cfg)
    : max_quantity_{cfg.max_quantity},
      participation_cap_ppm_{cfg.participation_cap_ppm},
      band_bps_x100_{cfg.band_bps_x100} {}

void Strategy::on_quote(const Quote& q) {
    observed_bid_qty_ += q.bid_qty;
    observed_ask_qty_ += q.ask_qty;
}

bool Strategy::warm(const RollingVwap& vwap) const {
    return vwap.has_full_window() && vwap.quotes_seen() >= kMinQuotes;
}

bool Strategy::within_cap(const uint64_t already, const uint32_t extra, const uint64_t observed) const {
    return (already + extra) * kCapScale <= static_cast<uint64_t>(participation_cap_ppm_) * observed;
}

uint64_t Strategy::buy_participation_ppm() const {
    if (observed_ask_qty_ == 0) return 0;
    return bought_qty_ * kCapScale / observed_ask_qty_;
}

uint64_t Strategy::sell_participation_ppm() const {
    if (observed_bid_qty_ == 0) return 0;
    return sold_qty_ * kCapScale / observed_bid_qty_;
}

bool Strategy::avg_buy_px(int64_t& out) const {
    if (bought_qty_ == 0) return false;
    out = bought_notional_ / static_cast<int64_t>(bought_qty_);
    return true;
}

bool Strategy::avg_sell_px(int64_t& out) const {
    if (sold_qty_ == 0) return false;
    out = sold_notional_ / static_cast<int64_t>(sold_qty_);
    return true;
}


Decision Strategy::judge(const Trade& t, const L1& book, const RollingVwap& vwap) {
    Decision d;

    int64_t v = 0;
    if (!warm(vwap) || !vwap.value(v)) return d;

    d.verdict = Verdict::Reject;
    d.side = t.aggressor;

    if (t.aggressor != 'B' && t.aggressor !='S') {
        d.side = '?';
        d.reason = RejectReason::UnknownAggressor;
        return d;
    }

    if (!book.valid) {
        d.reason = RejectReason::NoBook;
        return d;
    }

    const bool buying = t.aggressor == 'B';
    d.limit_px = buying ? book.ask_px : book.bid_px;
    d.qty = t.qty;

    if (t.qty == 0) {
        d.reason = RejectReason::ZeroQty;
        return d;
    }

    if (t.qty > max_quantity_) {
        d.reason = RejectReason::OverMaxQuantity;
        return d;
    }

    const bool in_band = buying
        ? book.ask_px * kBandScale <= v * (kBandScale - band_bps_x100_)
        : book.bid_px * kBandScale >= v * (kBandScale + band_bps_x100_);
    if (!in_band) {
        d.reason = RejectReason::QuoteOutsideBand;
        return d;
    }

    const bool clears_quote = buying ? t.px >= book.ask_px : t.px <= book.bid_px;
    if (!clears_quote) {
        d.reason = RejectReason::TradePriceMissesQuote;
        return d;
    }

    const uint64_t already = buying ? bought_qty_ : sold_qty_;
    const uint64_t observed = buying ? observed_ask_qty_ : observed_bid_qty_;
    if (!within_cap(already, t.qty, observed)) {
        d.reason = RejectReason::ParticipationCap;
        return d;
    }

    const int64_t notional = t.px * static_cast<int64_t>(t.qty);
    if (buying) {
        bought_qty_ += t.qty;
        bought_notional_ += notional;
    } else {
        sold_qty_ += t.qty;
        sold_notional_ += notional;
    }

    d.verdict = Verdict::Accept;
    d.reason = RejectReason::None;
    return d;
}



