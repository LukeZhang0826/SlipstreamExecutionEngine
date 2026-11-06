//
// Created by lukeypookster on 10/17/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_CODEC_H
#define SLIPSTREAMEXECUTIONENGINE_CODEC_H
#include "messages.h"
#include <cstddef>

enum class DecodeResult { Ok, Incomplete, Invalid, Unknown };

[[nodiscard]] size_t encode_body(const Quote& q, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, Quote& q);

[[nodiscard]] size_t encode_body(const Trade& t, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, Trade& t);

[[nodiscard]] size_t encode_body(const Heartbeat& h, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, Heartbeat& h);

[[nodiscard]] size_t encode_body(const SessionControl& c, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, SessionControl& c);

[[nodiscard]] size_t encode_body(const NewOrder& c, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, NewOrder& c);

[[nodiscard]] size_t encode_body(const ExecReport& c, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_body(const std::byte* in, size_t len, ExecReport& c);

inline MsgType type_of(const Quote&)          { return MsgType::Quote; }
inline MsgType type_of(const Trade&)          { return MsgType::Trade; }
inline MsgType type_of(const Heartbeat&)      { return MsgType::Heartbeat; }
inline MsgType type_of(const SessionControl&) { return MsgType::SessionControl; }
inline MsgType type_of(const NewOrder&)       { return MsgType::NewOrder; }
inline MsgType type_of(const ExecReport&)     { return MsgType::ExecReport; }


inline constexpr size_t  kFrameHeaderLen        = 4;
inline constexpr uint8_t kProtocolVersion       = 1;

inline constexpr size_t  kQuoteBodyLen          = 44;
inline constexpr size_t  kTradeBodyLen          = 41;
inline constexpr size_t  kHeartbeatBodyLen      = 8;
inline constexpr size_t  kSessionControlBodyLen = 9;
inline constexpr size_t  kNewOrderBodyLen       = 50;
inline constexpr size_t  kExecReportBodyLen     = 30;

inline constexpr size_t  kMaxBodyLen            = kNewOrderBodyLen;
inline constexpr size_t  kMaxFrameLen           = kFrameHeaderLen + kMaxBodyLen;

[[nodiscard]] size_t encode_frame_header(const FrameHeader& fh, std::byte* out, size_t cap);
[[nodiscard]] size_t decode_frame_header(const std::byte* in, size_t len, FrameHeader& fh);

template <typename MessageVariant>
[[nodiscard]] size_t encode_frame(const MessageVariant& msg, std::byte* out, size_t cap) {
    if (cap < kFrameHeaderLen) return 0;

    return std::visit([out, cap](const auto& m) -> size_t {
        const size_t body_len = encode_body(m, out + kFrameHeaderLen, cap - kFrameHeaderLen);
        if (body_len == 0) return 0;

        const FrameHeader h{
            .body_len = static_cast<uint16_t>(body_len),
            .msg_type = static_cast<uint8_t>(type_of(m)),
            .version  = kProtocolVersion
        };
        if (encode_frame_header(h, out, kFrameHeaderLen) == 0) return 0;

        return kFrameHeaderLen + body_len;
    }, msg);
};

[[nodiscard]] DecodeResult decode_frame(const std::byte* in, size_t len, MdMessage& out, size_t& consumed);
[[nodiscard]] DecodeResult decode_frame(const std::byte* in, size_t len, OeMessage& out, size_t& consumed);
[[nodiscard]] DecodeResult parse_frame(const std::byte* in, size_t len, FrameHeader& fh, size_t& frame_len);

#endif //SLIPSTREAMEXECUTIONENGINE_CODEC_H
