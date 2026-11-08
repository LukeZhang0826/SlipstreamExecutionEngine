#include <gtest/gtest.h>
#include "slipstream/frame_reader.h"

namespace {
    constexpr Quote kQuote{.symbol="SYNTH1", .ts_ns=1, .bid_qty=2,
                           .bid_px=3, .ask_qty=4, .ask_px=5};
    constexpr Trade kTrade{.symbol="SYNTH1", .ts_ns=11, .qty=13,
                       .px=1012500, .aggressor='B', .id=17};
    constexpr Heartbeat kHeartbeat{.ts_ns=123456789};
    constexpr SessionControl kSessionControl{.ts_ns=123456789, .state=1};
    constexpr NewOrder kNewOrder{.client_order_id=7, .symbol="SYNTH1", .status='A',
                          .ts_ns=11, .trade_id=13, .side='B',
                          .qty=17, .limit_px=1012500};
    constexpr ExecReport kExecReport{.client_order_id=7, .ts_ns=11, .status=1,
                                .filled_qty=17, .avg_px=1012500, .reason_code=2};

    template <typename Variant, typename Msg>
    void expect_round_trip(const Msg& in, size_t body_len) {
        std::byte wire[kMaxFrameLen];
        const size_t n = encode_frame(Variant{in}, wire, sizeof(wire));
        ASSERT_EQ(n, kFrameHeaderLen + body_len);

        Variant out;
        size_t consumed{0};
        ASSERT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Ok);
        EXPECT_EQ(consumed, n);

        const Msg* got = std::get_if<Msg>(&out);
        ASSERT_NE(got, nullptr);
        EXPECT_EQ(*got, in);
    }
}

TEST(Codec, QuoteRoundTrip) {
    expect_round_trip<MdMessage>(kQuote, kQuoteBodyLen);
}

TEST(Codec, TradeRoundTrip) {
    expect_round_trip<OeMessage>(kTrade, kTradeBodyLen);
}

TEST(Codec, HeartbeatRoundTrip) {
    expect_round_trip<OeMessage>(kHeartbeat, kHeartbeatBodyLen);
}

TEST(Codec, SessionControlRoundTrip) {
    expect_round_trip<OeMessage>(kSessionControl, kSessionControlBodyLen);
}

TEST(Codec, NewOrderRoundTrip) {
    expect_round_trip<OeMessage>(kNewOrder, kNewOrderBodyLen);
}

TEST(Codec, ExecReportRoundTrip) {
    expect_round_trip<OeMessage>(kExecReport, kExecReportBodyLen);
}

TEST(FrameReader, OneByteAtATime) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));

    FrameReader<MdMessage> fr;
    MdMessage out;
    bool complete = false;
    for (size_t i{0}; i < n; ++i) {
        ASSERT_TRUE(fr.feed(wire + i, 1));
        if (fr.next(out) == DecodeResult::Ok) complete = true;
    }
    EXPECT_EQ(complete, true);
    EXPECT_EQ(std::get<Quote>(out), kQuote);
}

TEST(FrameReader, SplitAtEveryBoundary) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));

    for (size_t cut{0}; cut <= n; ++cut) {
        FrameReader<MdMessage> fr;
        MdMessage out;

        ASSERT_TRUE(fr.feed(wire, cut)) << "cut=" << cut;
        if (cut < n) {
            ASSERT_EQ(fr.next(out), DecodeResult::Incomplete) << "cut=" << cut;
        }
        ASSERT_TRUE(fr.feed(wire + cut, n - cut)) << "cut=" << cut;
        ASSERT_EQ(fr.next(out), DecodeResult::Ok) << "cut=" << cut;
        EXPECT_EQ(std::get<Quote>(out), kQuote) << "cut=" << cut;
    }
}

TEST(FrameReader, TwoFramesInOneFeed) {
    constexpr Quote second{.symbol="SYNTH1", .ts_ns=99, .bid_qty=7,
                           .bid_px=8, .ask_qty=9, .ask_px=10};

    std::byte wire[kMaxFrameLen * 2];
    const size_t n1 = encode_frame(MdMessage{kQuote},  wire,      sizeof(wire));
    const size_t n2 = encode_frame(MdMessage{second}, wire + n1, sizeof(wire) - n1);
    ASSERT_GT(n1, 0uz);
    ASSERT_GT(n2, 0uz);

    FrameReader<MdMessage> fr;
    ASSERT_TRUE(fr.feed(wire, n1 + n2));

    MdMessage out;
    ASSERT_EQ(fr.next(out), DecodeResult::Ok);
    EXPECT_EQ(std::get<Quote>(out), kQuote);
    ASSERT_EQ(fr.next(out), DecodeResult::Ok);
    EXPECT_EQ(std::get<Quote>(out), second);
    EXPECT_EQ(fr.next(out), DecodeResult::Incomplete);
}

TEST(CodecErrors, WrongVersionIsInvalid) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));
    wire[3] = std::byte{kProtocolVersion + 1};

    MdMessage out;
    size_t consumed{1};
    EXPECT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Invalid);
    EXPECT_EQ(consumed, 0uz);
}

TEST(CodecErrors, BodyLenAboveMaxIsInvalid) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));
    const uint16_t bad_len = kMaxBodyLen + 1;
    std::memcpy(wire, &bad_len, sizeof(bad_len));

    MdMessage out;
    size_t consumed{1};
    EXPECT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Invalid);
    EXPECT_EQ(consumed, 0uz);
}

TEST(CodecErrors, BodyLenWrongForTypeIsInvalid) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));
    const uint16_t short_len = kQuoteBodyLen - 1;
    std::memcpy(wire, &short_len, sizeof(short_len));

    MdMessage out;
    size_t consumed{1};
    EXPECT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Invalid);
    EXPECT_EQ(consumed, 0uz);
}

TEST(CodecErrors, ShortHeaderIsIncomplete) {
    std::byte wire[kMaxFrameLen];
    (void)encode_frame(MdMessage{kQuote}, wire, sizeof(wire));

    MdMessage out;
    size_t consumed{1};
    EXPECT_EQ(decode_frame(wire, kFrameHeaderLen - 1, out, consumed),
              DecodeResult::Incomplete);
    EXPECT_EQ(consumed, 0uz);
}

TEST(CodecErrors, TruncatedBodyIsIncomplete) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));

    MdMessage out;
    size_t consumed{1};
    EXPECT_EQ(decode_frame(wire, n - 1, out, consumed), DecodeResult::Incomplete);
    EXPECT_EQ(consumed, 0uz);
}

TEST(CodecErrors, UnknownTypeIsSkippedNotFatal) {
    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));
    wire[2] = std::byte{99};

    MdMessage out;
    size_t consumed{0};
    EXPECT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Unknown);
    EXPECT_EQ(consumed, n) << "must skip the frame so the stream stays in sync";
}

TEST(CodecErrors, EncodeIntoTooSmallBufferWritesNothing) {
    std::byte wire[kFrameHeaderLen + kQuoteBodyLen - 1];
    EXPECT_EQ(encode_frame(MdMessage{kQuote}, wire, sizeof(wire)), 0uz);
}

TEST(FrameReaderErrors, UnknownTypeThenValidFrameStillDecodes) {
    std::byte wire[kMaxFrameLen * 2];
    const size_t n1 = encode_frame(MdMessage{kQuote}, wire, sizeof(wire));
    wire[2] = std::byte{99};
    const size_t n2 = encode_frame(MdMessage{kQuote}, wire + n1, sizeof(wire) - n1);

    FrameReader<MdMessage> fr;
    ASSERT_TRUE(fr.feed(wire, n1 + n2));

    MdMessage out;
    ASSERT_EQ(fr.next(out), DecodeResult::Unknown);
    ASSERT_EQ(fr.next(out), DecodeResult::Ok);
    EXPECT_EQ(std::get<Quote>(out), kQuote);
}