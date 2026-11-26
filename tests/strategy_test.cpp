//
// Created by lukeypookster on 11/25/26.
//

#include <gtest/gtest.h>

#include "slipstream/strategy.h"

namespace {
    constexpr uint64_t kWindowMs = 1000;
    constexpr uint64_t kWindowNs = kWindowMs * 1'000'000;

    // Eleven identical quotes, 100 ms apart, so the window is full and eleven
    // quotes have been seen. Each one is worth
    // (100*1'000'000 + 100*1'010'000) / 200 == 1'005'000, so V is 100.5000.
    constexpr int64_t  kV            = 1'005'000;
    constexpr uint32_t kQuoteSideQty = 100;
    constexpr size_t   kWarmUpQuotes = 11;

    // V * (1 - B) and V * (1 + B) for a 10 bp band, the largest ask we may buy
    // and the smallest bid we may sell into.
    constexpr int64_t kMaxBuyAsk  = 1'003'995;
    constexpr int64_t kMinSellBid = 1'006'005;

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

    Trade make_trade(const char aggressor, const int64_t px, const uint32_t qty) {
        Trade t{};
        t.ts_ns     = kWindowNs;
        t.px        = px;
        t.qty       = qty;
        t.aggressor = aggressor;
        t.id        = 7;
        return t;
    }

    Config warm_config() {
        Config cfg;
        cfg.vwap_window_ms        = kWindowMs;
        cfg.band_bps_x100         = 1000;    // 10.00 bps
        cfg.max_quantity          = 500;
        cfg.participation_cap_ppm = 150'000; // 0.15
        return cfg;
    }

    /// A strategy and a VWAP that have both just passed the warm up gate, plus
    /// a book the caller can point wherever the test needs it.
    struct Fixture {
        Config      cfg  = warm_config();
        Strategy    strategy{cfg};
        RollingVwap vwap{kWindowMs};
        L1          book;

        explicit Fixture(const size_t quotes = kWarmUpQuotes) {
            for (size_t i = 0; i < quotes; ++i) {
                const Quote q = make_quote(i * 100'000'000,
                                           1'000'000, kQuoteSideQty,
                                           1'010'000, kQuoteSideQty);
                vwap.on_quote(q);
                strategy.on_quote(q);
            }
            vwap.advance(kWindowNs);
        }

        void set_book(const int64_t bid_px, const int64_t ask_px) {
            book.apply(make_quote(kWindowNs, bid_px, 500, ask_px, 500));
        }
    };
}

TEST(Strategy, BenchmarkMatchesTheFixtureAssumption) {
    const Fixture f;
    int64_t v = 0;
    ASSERT_TRUE(f.vwap.value(v));
    EXPECT_EQ(v, kV);
    EXPECT_TRUE(f.vwap.has_full_window());
    EXPECT_EQ(f.vwap.quotes_seen(), kWarmUpQuotes);
}

TEST(Strategy, SaysNothingBeforeTheWindowIsFull) {
    Config cfg = warm_config();
    Strategy s{cfg};
    RollingVwap vwap{kWindowMs};
    L1 book;

    for (size_t i = 0; i < kWarmUpQuotes; ++i) {
        const Quote q = make_quote(i, 1'000'000, 100, 1'010'000, 100);
        vwap.on_quote(q);
        s.on_quote(q);
    }
    book.apply(make_quote(0, 1'007'000, 500, 1'003'000, 500));

    const Decision d = s.judge(make_trade('B', 1'003'000, 100), book, vwap);
    EXPECT_EQ(d.verdict, Verdict::Suppress);
}

TEST(Strategy, SaysNothingWithFewerThanTenQuotes) {
    Fixture f{9};
    f.set_book(1'007'000, 1'003'000);

    ASSERT_TRUE(f.vwap.has_full_window());
    ASSERT_LT(f.vwap.quotes_seen(), 10u);

    const Decision d = f.strategy.judge(make_trade('B', 1'003'000, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Suppress);
}

TEST(Strategy, AcceptsABuyWhenTheAskIsCheapAndWePaidIt) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('B', 1'003'500, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Accept);
    EXPECT_EQ(d.reason, RejectReason::None);
    EXPECT_EQ(d.side, 'B');
    EXPECT_EQ(d.qty, 100u);
    EXPECT_EQ(d.limit_px, 1'003'000);   // we would have crossed at the ask
    EXPECT_EQ(f.strategy.bought_qty(), 100u);
}

TEST(Strategy, AcceptsASellWhenTheBidIsRichAndWeTookIt) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('S', 1'006'500, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Accept);
    EXPECT_EQ(d.side, 'S');
    EXPECT_EQ(d.limit_px, 1'007'000);   // we would have crossed at the bid
    EXPECT_EQ(f.strategy.sold_qty(), 100u);
}

TEST(Strategy, BandAppliesToTheQuoteNotTheTradePrice) {
    Fixture f;
    // The trade price is deeply favourable against V, but the ask itself is
    // not cheap enough, so the trade should never have happened.
    f.set_book(1'007'000, 1'004'500);

    const Decision d = f.strategy.judge(make_trade('B', 900'000, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::QuoteOutsideBand);
}

TEST(Strategy, BuyBandBoundaryIsInclusive) {
    Fixture on_the_line;
    on_the_line.set_book(1'007'000, kMaxBuyAsk);
    EXPECT_EQ(on_the_line.strategy.judge(make_trade('B', kMaxBuyAsk, 100),
                                         on_the_line.book, on_the_line.vwap).verdict,
              Verdict::Accept);

    Fixture one_tick_over;
    one_tick_over.set_book(1'007'000, kMaxBuyAsk + 1);
    const Decision d = one_tick_over.strategy.judge(make_trade('B', kMaxBuyAsk + 1, 100),
                                                    one_tick_over.book, one_tick_over.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::QuoteOutsideBand);
}

TEST(Strategy, SellBandBoundaryIsInclusive) {
    Fixture on_the_line;
    on_the_line.set_book(kMinSellBid, 1'003'000);
    EXPECT_EQ(on_the_line.strategy.judge(make_trade('S', kMinSellBid, 100),
                                         on_the_line.book, on_the_line.vwap).verdict,
              Verdict::Accept);

    Fixture one_tick_under;
    one_tick_under.set_book(kMinSellBid - 1, 1'003'000);
    const Decision d = one_tick_under.strategy.judge(make_trade('S', kMinSellBid - 1, 100),
                                                     one_tick_under.book, one_tick_under.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::QuoteOutsideBand);
}

TEST(Strategy, RejectsABuyThatWouldNotPayTheAsk) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('B', 1'002'999, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::TradePriceMissesQuote);
}

TEST(Strategy, RejectsASellThatWouldNotAcceptTheBid) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('S', 1'007'001, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::TradePriceMissesQuote);
}

TEST(Strategy, RejectsAnUnknownAggressor) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('?', 1'003'500, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::UnknownAggressor);
    EXPECT_EQ(d.side, '?');
}

TEST(Strategy, RejectsWithNoBookYet) {
    Fixture f;   // deliberately no set_book

    const Decision d = f.strategy.judge(make_trade('B', 1'003'500, 100), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::NoBook);
}

TEST(Strategy, RejectsZeroQuantity) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('B', 1'003'500, 0), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::ZeroQty);
}

TEST(Strategy, RejectsOverTheMaxQuantityCeiling) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    const Decision d = f.strategy.judge(make_trade('B', 1'003'500, 501), f.book, f.vwap);
    EXPECT_EQ(d.verdict, Verdict::Reject);
    EXPECT_EQ(d.reason, RejectReason::OverMaxQuantity);
}

TEST(Strategy, RejectsWhatWouldBreachTheParticipationCap) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    // Eleven quotes offered 100 a side, so 1100 on the ask. A 15% cap allows
    // 165, which 150 clears and 200 does not.
    ASSERT_EQ(f.strategy.observed_ask_qty(), 1100u);

    EXPECT_EQ(f.strategy.judge(make_trade('B', 1'003'500, 200), f.book, f.vwap).reason,
              RejectReason::ParticipationCap);
    EXPECT_EQ(f.strategy.bought_qty(), 0u);

    EXPECT_EQ(f.strategy.judge(make_trade('B', 1'003'500, 150), f.book, f.vwap).verdict,
              Verdict::Accept);
    EXPECT_EQ(f.strategy.bought_qty(), 150u);
}

TEST(Strategy, ParticipationIsTrackedPerSide) {
    Fixture f;
    f.set_book(1'007'000, 1'003'000);

    ASSERT_EQ(f.strategy.judge(make_trade('B', 1'003'500, 100), f.book, f.vwap).verdict,
              Verdict::Accept);
    ASSERT_EQ(f.strategy.judge(make_trade('S', 1'006'500, 110), f.book, f.vwap).verdict,
              Verdict::Accept);

    EXPECT_EQ(f.strategy.bought_qty(), 100u);
    EXPECT_EQ(f.strategy.sold_qty(), 110u);
    // 100 / 1100 and 110 / 1100 in parts per million.
    EXPECT_EQ(f.strategy.buy_participation_ppm(), 90'909u);
    EXPECT_EQ(f.strategy.sell_participation_ppm(), 100'000u);
}

TEST(Strategy, RejectedTradesDoNotConsumeTheBudget) {
    Fixture f;
    f.set_book(1'007'000, 1'004'500);   // ask outside the band

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(f.strategy.judge(make_trade('B', 1'004'500, 100), f.book, f.vwap).verdict,
                  Verdict::Reject);
    }
    EXPECT_EQ(f.strategy.bought_qty(), 0u);
    EXPECT_EQ(f.strategy.buy_participation_ppm(), 0u);
}