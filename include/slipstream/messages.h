//
// Created by lukeypookster on 10/17/26.
//

#ifndef SLIPSTREAMEXECUTIONENGINE_MESSAGES_H
#define SLIPSTREAMEXECUTIONENGINE_MESSAGES_H
#include <cstdint>
#include <variant>

struct FrameHeader {
    uint16_t    body_len;
    uint8_t     msg_type;
    uint8_t     version;
};

enum class MsgType : uint8_t {
    Quote =             1,
    Trade =             2,
    Heartbeat =         3,
    SessionControl =    4,
    NewOrder =          10,
    ExecReport =        11,
};

struct Quote {
    char symbol[12];
    uint64_t ts_ns;
    uint32_t bid_qty;
    int64_t bid_px;
    uint32_t ask_qty;
    int64_t ask_px;

    bool operator==(const Quote&) const = default;
};

struct Trade {
    char symbol[12];
    uint64_t ts_ns;
    uint32_t qty;
    int64_t px;
    char aggressor;
    int64_t id;

    bool operator==(const Trade&) const = default;
};

struct Heartbeat {
    uint64_t ts_ns;

    bool operator==(const Heartbeat&) const = default;
};

struct SessionControl {
    uint64_t ts_ns;
    uint8_t state;

    bool operator==(const SessionControl&) const = default;
};

struct NewOrder {
    uint64_t client_order_id;
    char symbol[12];
    char status;
    uint64_t ts_ns;
    int64_t trade_id;
    char side;
    uint32_t qty;
    int64_t limit_px;

    bool operator==(const NewOrder&) const = default;
};

struct ExecReport {
    uint64_t client_order_id;
    uint64_t ts_ns;
    uint8_t status;
    uint32_t filled_qty;
    int64_t avg_px;
    uint8_t reason_code;

    bool operator==(const ExecReport&) const = default;
};

using MdMessage = std::variant<Quote>;
using OeMessage = std::variant<Trade, Heartbeat, SessionControl, NewOrder, ExecReport>;


#endif //SLIPSTREAMEXECUTIONENGINE_MESSAGES_H
