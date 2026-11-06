#include <iostream>

#include "slipstream/messages.h"
#include "slipstream/codec.h"

int main() {
    Quote quote {
        .symbol = "Quote",
        .ts_ns = 254,
        .bid_qty = 2,
        .bid_px = 3,
        .ask_qty = 4,

        .ask_px = 5,
    };

    std::byte buf[kFrameHeaderLen + kQuoteBodyLen];
    const size_t n = encode_frame(MdMessage{quote}, buf, sizeof(buf));
    for (size_t i = 0; i < n; ++i)
        std::cout << std::format("{:02x} ", std::to_integer<unsigned>(buf[i]));
    std::cout << "\n";

    MdMessage new_quote{};
    size_t consumed = 0;
    const DecodeResult decode_result = decode_frame(buf, sizeof(buf), new_quote, consumed);
    if (decode_result == DecodeResult::Ok) {
        if (const Quote* q = std::get_if<Quote>(&new_quote)) {
            std::cout << q->ts_ns << " " << q->bid_px << "\n";
        }
    }
    // std::byte dest[44];
    // encode_quote(quote, dest);
    //
    // for (size_t i = 0; i < 44; ++i)
    //     std::cout <<   Quote quote {
    //        .symbol = "Quote",
    //        .ts_ns = 254,
    //        .bid_qty = 2,
    //        .bid_px = 3,
    //        .ask_qty = 4,
    //        .ask_px = 5,
    //    }; std::format("{:02x} ", std::to_integer<unsigned>(dest[i]));

    return 0;
}