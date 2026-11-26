//
// Created by lukeypookster on 11/13/26.
//

#include <gtest/gtest.h>

#include "slipstream/vwap.h"

namespace {
    constexpr uint64_t kWindowMs = 1000;
    constexpr uint64_t kWindowNs = kWindowMs * 1'000'000;

    Quote make_quote(const uint64_t ts_ns,
                     const int64_t bid_px, const uint32_t bid_qty,
                     const int64_t ask_px, const uint32_t ask_qty) {
        Quote q{};
        q.ts_ns   = ts_ns;
        q.bid_px  = bid_px;
        q.bid_qty = bid_qty;
        q.ask_px  = ask_px;
        q.ask_qty = ask_qty;
        return q;
    }
}

TEST(RollingVwap, EmptyHasNoValue) {
    const RollingVwap v{kWindowMs};
    int64_t out = -1;
    EXPECT_FALSE(v.value(out));
    EXPECT_FALSE(v.has_full_window());
    EXPECT_EQ(v.quotes_seen(), 0u);
    EXPECT_EQ(v.size(), 0u);
}

TEST(RollingVwap, SingleQuoteIsWeightedAcrossItsTwoSides) {
    RollingVwap v{kWindowMs};
    // (200*200'000 + 300*210'000) / 500 == 206'000 => 20.60.
    v.on_quote(make_quote(0, 200'000, 200, 210'000, 300));

    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 206'000);
}

TEST(RollingVwap, ReproducesTheBriefsWorkedExample) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 200'000, 200, 210'000, 300));  // 20.60
    v.on_quote(make_quote(1, 220'000, 250, 250'000, 400));  // 23.8461

    // (206'000 + 238'461) / 2 == 222'230 => 22.22.
    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 222'230);
}

TEST(RollingVwap, PooledVolumeWouldGiveADifferentAnswer) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 200'000, 200, 210'000, 300));
    v.on_quote(make_quote(1, 220'000, 250, 250'000, 400));

    // Guard against drifting back to sum(px*qty)/sum(qty)
    // 258'000'000 / 1150 == 224'347 here.
    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_NE(out, 224'347);
}

TEST(RollingVwap, OneSidedQuoteUsesOnlyTheLiveSide) {
    RollingVwap v{kWindowMs};
    // Zero bid qty is a cancel of the bid
    v.on_quote(make_quote(0, 200'000, 0, 210'000, 300));

    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 210'000);
}

TEST(RollingVwap, FullyCancelledQuoteCountsForTheGateButNotTheBenchmark) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 0, 0, 0, 0));

    int64_t out = -1;
    EXPECT_FALSE(v.value(out));
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.quotes_seen(), 1u);
}

TEST(RollingVwap, QuoteExactlyOneWindowOldIsEvicted) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 200'000, 200, 210'000, 300));

    v.advance(kWindowNs - 1);
    EXPECT_EQ(v.size(), 1u);

    v.advance(kWindowNs);
    EXPECT_EQ(v.size(), 0u);

    int64_t out = -1;
    EXPECT_FALSE(v.value(out));
}

TEST(RollingVwap, EvictionLeavesOnlyTheQuotesInsideTheWindow) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 100'000, 100, 100'000, 100));              // leaves
    v.on_quote(make_quote(kWindowNs / 2, 200'000, 100, 200'000, 100));  // stays
    v.on_quote(make_quote(kWindowNs, 300'000, 100, 300'000, 100));

    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 250'000);   // (200'000 + 300'000) / 2
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.quotes_seen(), 3u);
}

TEST(RollingVwap, FullWindowGateNeedsAWindowOfElapsedTime) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(5'000, 200'000, 200, 210'000, 300));
    EXPECT_FALSE(v.has_full_window());

    v.advance(5'000 + kWindowNs - 1);
    EXPECT_FALSE(v.has_full_window());

    v.advance(5'000 + kWindowNs);
    EXPECT_TRUE(v.has_full_window());
}

TEST(RollingVwap, GateStaysSetAfterTheWindowDrains) {
    RollingVwap v{kWindowMs};
    v.on_quote(make_quote(0, 200'000, 200, 210'000, 300));
    v.advance(10 * kWindowNs);

    EXPECT_TRUE(v.has_full_window());
    EXPECT_EQ(v.size(), 0u);
}