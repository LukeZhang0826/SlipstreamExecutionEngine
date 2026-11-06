//
// Created by lukeypookster on 11/5/26.
//

#include <gtest/gtest.h>
#include "slipstream/frame_reader.h"

TEST(Codec, QuoteRoundTrip) {
    const Quote in{.symbol="SYNTH1", .ts_ns=1, .bid_qty=2, .bid_px=3, .ask_qty=4, .ask_px=5};

    std::byte wire[kMaxFrameLen];
    const size_t n = encode_frame(MdMessage{in}, wire, sizeof(wire));
    ASSERT_EQ(n, kFrameHeaderLen + kQuoteBodyLen);

    MdMessage out;
    size_t consumed{0};
    ASSERT_EQ(decode_frame(wire, n, out, consumed), DecodeResult::Ok);
    EXPECT_EQ(consumed, n);

    const Quote* got = std::get_if<Quote>(&out); // checks if the variant is a quote
    ASSERT_NE(got, nullptr); // if not its a nullptr
    EXPECT_EQ(*got, in);
}