"""GCMD/1 and GCOE/1 wire format. Mirrors include/slipstream/codec.h."""
import struct
from decimal import Decimal

from enum import IntEnum


SCALE = 10_000
PROTOCOL_VERSION = 1
class MsgType(IntEnum):
    QUOTE = 1
    TRADE = 2
    HEARTBEAT = 3
    SESSION_CONTROL = 4
    NEW_ORDER = 10
    EXEC_REPORT = 11

FRAME_HEADER = struct.Struct("<HBB") # body_len, msg_type, version
QUOTE_BODY = struct.Struct("<12sQIqIq") # symbol, ts_ns, bid_qty, bid_px, ask_qty, ask_px
TRADE_BODY = struct.Struct("<12sQIqcq") # symbol, ts_ns, qty, px, aggressor, id

def to_fixed(price_str):
    return int(Decimal(price_str) * SCALE)

def frame(msg_type, body):
    return FRAME_HEADER.pack(len(body), msg_type, PROTOCOL_VERSION) + body

def quote_frame(symbol, ts_ns, bid_qty, bid_px, ask_qty, ask_px):
    body = QUOTE_BODY.pack(symbol.encode(), ts_ns, bid_qty, bid_px, ask_qty, ask_px)
    return frame(MsgType.QUOTE, body)

def trade_frame(symbol, ts_ns, qty, px, aggressor, trade_id):
    body = TRADE_BODY.pack(symbol.encode(), ts_ns, qty, px, aggressor.encode(), trade_id)
    return frame(MsgType.TRADE, body)


if __name__ == "__main__":
    print("header", FRAME_HEADER.size, "quote", QUOTE_BODY.size, "trade", TRADE_BODY.size)
    print(quote_frame("Quote", 254, 2, 3, 4, 5).hex(" "))






# if __name__ == "__main__":
#