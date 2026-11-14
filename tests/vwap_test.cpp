//
// Created by lukeypookster on 11/13/26.
//

#include <gtest/gtest.h>

#include "slipstream/vwap.h"

namespace {
    constexpr uint64_t kWindowMs = 1000;
    constexpr uint64_t kWindowNs = kWindowMs * 1000000;

    Trade make_trade(const uint64_t ts_ns, const int64_t px, const uint32_t qty) {
        Trade t{};
        t.ts_ns = ts_ns;
        t.px = px;
        t.qty = qty;
        t.aggressor = 'B';
        t.id = 1;
        return t;
    }
}

TEST(RollingVwap, EmptyHasNoValue) {
    const RollingVwap v{kWindowMs};
    int64_t out = -1;
    EXPECT_FALSE(v.value(out));
    EXPECT_FALSE(v.has_full_window());
    EXPECT_EQ(v.window_qty(), 0);
    EXPECT_EQ(v.size(), 0u);
}

TEST(RollingVwap, SingleTradeIsItsOwnVwap) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 100));

    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 1'000'000);
    EXPECT_EQ(v.window_qty(), 100);
}

TEST(RollingVwap, WeightsByQuantityNotCount) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 100));
    v.on_trade(make_trade(1, 1'010'000, 300));

    // (1'000'000*100 + 1'010'000*300) / 400 == 1'007'500, so 100.75.
    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 1'007'500);
    EXPECT_EQ(v.window_qty(), 400);
}

TEST(RollingVwap, DivisionTruncates) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 1));
    v.on_trade(make_trade(1, 1'000'001, 2));

    // 3'000'002 / 3 is 1'000'000.67, which truncates down.
    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 1'000'000);
}

TEST(RollingVwap, TradeExactlyOneWindowOldIsEvicted) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 100));

    v.advance(kWindowNs - 1);
    EXPECT_EQ(v.size(), 1u);

    v.advance(kWindowNs);
    EXPECT_EQ(v.size(), 0u);
    EXPECT_EQ(v.window_qty(), 0);

    int64_t out = -1;
    EXPECT_FALSE(v.value(out));
}

TEST(RollingVwap, EvictionLeavesOnlyTheTradesInsideTheWindow) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 100));
    v.on_trade(make_trade(kWindowNs / 2, 2'000'000, 100));

    v.on_trade(make_trade(kWindowNs, 3'000'000, 100));

    int64_t out = 0;
    ASSERT_TRUE(v.value(out));
    EXPECT_EQ(out, 2'500'000);
    EXPECT_EQ(v.size(), 2u);
}

TEST(RollingVwap, FullWindowGateNeedsAWindowOfElapsedTime) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(5'000, 1'000'000, 10));
    EXPECT_FALSE(v.has_full_window());

    v.advance(5'000 + kWindowNs - 1);
    EXPECT_FALSE(v.has_full_window());

    v.advance(5'000 + kWindowNs);
    EXPECT_TRUE(v.has_full_window());
}

TEST(RollingVwap, GateStaysSetAfterTheWindowDrains) {
    RollingVwap v{kWindowMs};
    v.on_trade(make_trade(0, 1'000'000, 10));
    v.advance(10 * kWindowNs);

    EXPECT_TRUE(v.has_full_window());
    EXPECT_EQ(v.size(), 0u);
}