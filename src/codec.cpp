//
// Created by lukeypookster on 10/17/26.
//
#include <cstddef>

#include "slipstream/bytes.h"
#include "slipstream/messages.h"
#include "slipstream/codec.h"

size_t encode_body(const Quote& q, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(q.symbol,     sizeof(q.symbol));
    w.put(&q.ts_ns,     sizeof(q.ts_ns));
    w.put(&q.bid_qty,   sizeof(q.bid_qty));
    w.put(&q.bid_px,    sizeof(q.bid_px));
    w.put(&q.ask_qty,   sizeof(q.ask_qty));
    w.put(&q.ask_px,    sizeof(q.ask_px));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, Quote& q) {
    ByteReader r{in, len};
    r.get(q.symbol,     sizeof(q.symbol));
    r.get(&q.ts_ns,     sizeof(q.ts_ns));
    r.get(&q.bid_qty,   sizeof(q.bid_qty));
    r.get(&q.bid_px,    sizeof(q.bid_px));
    r.get(&q.ask_qty,   sizeof(q.ask_qty));
    r.get(&q.ask_px,    sizeof(q.ask_px));
    return r.ok() ? r.read() : 0;
}

size_t encode_body(const Trade& t, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(t.symbol,     sizeof(t.symbol));
    w.put(&t.ts_ns,     sizeof(t.ts_ns));
    w.put(&t.qty,       sizeof(t.qty));
    w.put(&t.px,        sizeof(t.px));
    w.put(&t.aggressor, sizeof(t.aggressor));
    w.put(&t.id,        sizeof(t.id));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, Trade& t) {
    ByteReader r{in, len};
    r.get(t.symbol,     sizeof(t.symbol));
    r.get(&t.ts_ns,     sizeof(t.ts_ns));
    r.get(&t.qty,       sizeof(t.qty));
    r.get(&t.px,        sizeof(t.px));
    r.get(&t.aggressor, sizeof(t.aggressor));
    r.get(&t.id,        sizeof(t.id));
    return r.ok() ? r.read() : 0;
}

size_t encode_body(const Heartbeat& h, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(&h.ts_ns,     sizeof(h.ts_ns));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, Heartbeat& h) {
    ByteReader r{in, len};
    r.get(&h.ts_ns,     sizeof(h.ts_ns));
    return r.ok() ? r.read() : 0;
}

size_t encode_body(const SessionControl& c, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(&c.ts_ns,     sizeof(c.ts_ns));
    w.put(&c.state,     sizeof(c.state));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, SessionControl& c) {
    ByteReader r{in, len};
    r.get(&c.ts_ns,     sizeof(c.ts_ns));
    r.get(&c.state,     sizeof(c.state));
    return r.ok() ? r.read() : 0;
}

size_t encode_body(const NewOrder& c, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(&c.client_order_id,   sizeof(c.client_order_id));
    w.put(&c.symbol,            sizeof(c.symbol));
    w.put(&c.status,            sizeof(c.status));
    w.put(&c.ts_ns,             sizeof(c.ts_ns));
    w.put(&c.trade_id,          sizeof(c.trade_id));
    w.put(&c.side,              sizeof(c.side));
    w.put(&c.qty,               sizeof(c.qty));
    w.put(&c.limit_px,          sizeof(c.limit_px));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, NewOrder& c) {
    ByteReader r{in, len};
    r.get(&c.client_order_id,   sizeof(c.client_order_id));
    r.get(&c.symbol,            sizeof(c.symbol));
    r.get(&c.status,            sizeof(c.status));
    r.get(&c.ts_ns,             sizeof(c.ts_ns));
    r.get(&c.trade_id,          sizeof(c.trade_id));
    r.get(&c.side,              sizeof(c.side));
    r.get(&c.qty,               sizeof(c.qty));
    r.get(&c.limit_px,          sizeof(c.limit_px));
    return r.ok() ? r.read() : 0;
}

size_t encode_body(const ExecReport& c, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(&c.client_order_id,   sizeof(c.client_order_id));
    w.put(&c.ts_ns,             sizeof(c.ts_ns));
    w.put(&c.status,            sizeof(c.status));
    w.put(&c.filled_qty,        sizeof(c.filled_qty));
    w.put(&c.avg_px,            sizeof(c.avg_px));
    w.put(&c.reason_code,       sizeof(c.reason_code));
    return w.ok() ? w.written() : 0;
}

size_t decode_body(const std::byte* in, size_t len, ExecReport& c) {
    ByteReader r{in, len};
    r.get(&c.client_order_id,   sizeof(c.client_order_id));
    r.get(&c.ts_ns,             sizeof(c.ts_ns));
    r.get(&c.status,            sizeof(c.status));
    r.get(&c.filled_qty,        sizeof(c.filled_qty));
    r.get(&c.avg_px,            sizeof(c.avg_px));
    r.get(&c.reason_code,       sizeof(c.reason_code));
    return r.ok() ? r.read() : 0;
}

size_t encode_frame_header(const FrameHeader& fh, std::byte* out, const size_t cap) {
    ByteWriter w{out, cap};
    w.put(&fh.body_len, sizeof(fh.body_len));
    w.put(&fh.msg_type, sizeof(fh.msg_type));
    w.put(&fh.version,  sizeof(fh.version));
    return w.ok() ? w.written() : 0;
}

size_t decode_frame_header(const std::byte* in, size_t len, FrameHeader& fh) {
    ByteReader r{in, len};
    r.get(&fh.body_len, sizeof(fh.body_len));
    r.get(&fh.msg_type, sizeof(fh.msg_type));
    r.get(&fh.version,  sizeof(fh.version));
    return r.ok() ? r.read() : 0;
}

DecodeResult parse_frame(const std::byte* in, size_t len, FrameHeader& fh, size_t& frame_len) {
    frame_len = 0;

    if (len < kFrameHeaderLen) return DecodeResult::Incomplete;
    if (decode_frame_header(in, len, fh) == 0) return DecodeResult::Incomplete;

    if (fh.version != kProtocolVersion) return DecodeResult::Invalid;
    if (fh.body_len > kMaxBodyLen) return DecodeResult::Invalid;

    frame_len = kFrameHeaderLen + fh.body_len;
    if (len < frame_len) return DecodeResult::Incomplete;

    return DecodeResult::Ok;
}

DecodeResult decode_frame(const std::byte* in, const size_t len, MdMessage& out, size_t& consumed) {
    consumed = 0;
    FrameHeader fh{};
    size_t frame_len{0};

    if (DecodeResult parsed_frame_result{parse_frame(in, len, fh, frame_len)}; parsed_frame_result != DecodeResult::Ok) return parsed_frame_result;

    const std::byte* body = in + kFrameHeaderLen;
    const size_t n = fh.body_len;

    switch (static_cast<MsgType>(fh.msg_type)) {
        case MsgType::Quote: {
            if (n != kQuoteBodyLen) return DecodeResult::Invalid;
            Quote q{};
            if (decode_body(body, n, q) == 0) return DecodeResult::Invalid;
            out = q;
            break;
        }
        default:
            consumed = frame_len;
            return DecodeResult::Unknown;
    }

    consumed = frame_len;
    return DecodeResult::Ok;
}

DecodeResult decode_frame(const std::byte* in, const size_t len, OeMessage& out, size_t& consumed) {
    consumed = 0;
    FrameHeader fh{};
    size_t frame_len{0};

    if (DecodeResult parsed_frame_result{parse_frame(in, len, fh, frame_len)}; parsed_frame_result != DecodeResult::Ok) return parsed_frame_result;

    const std::byte* body = in + kFrameHeaderLen;
    const size_t n = fh.body_len;

    switch (static_cast<MsgType>(fh.msg_type)) {
        case MsgType::Trade: {
            if (n != kTradeBodyLen) return DecodeResult::Invalid;
            Trade t{};
            if (decode_body(body, n, t) == 0) return DecodeResult::Invalid;
            out = t;
            break;
        }
        case MsgType::Heartbeat: {
            if (n != kHeartbeatBodyLen) return DecodeResult::Invalid;
            Heartbeat h{};
            if (decode_body(body, n, h) == 0) return DecodeResult::Invalid;
            out = h;
            break;
        }
        case MsgType::SessionControl: {
            if (n != kSessionControlBodyLen) return DecodeResult::Invalid;
            SessionControl c{};
            if (decode_body(body, n, c) == 0) return DecodeResult::Invalid;
            out = c;
            break;
        }
        case MsgType::NewOrder: {
            if (n != kNewOrderBodyLen) return DecodeResult::Invalid;
            NewOrder no{};
            if (decode_body(body, n, no) == 0) return DecodeResult::Invalid;
            out = no;
            break;
        }
        case MsgType::ExecReport: {
            if (n != kExecReportBodyLen) return DecodeResult::Invalid;
            ExecReport er{};
            if (decode_body(body, n, er) == 0) return DecodeResult::Invalid;
            out = er;
            break;
        }
        default:
            consumed = frame_len;
            return DecodeResult::Unknown;
    }

    consumed = frame_len;
    return DecodeResult::Ok;
}