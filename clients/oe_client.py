from argparse import ArgumentParser
import socket
import threading
import time

import wire
from replay import read_rows, replay


def parse_args():
    p = ArgumentParser(description="Replay CSV trades to slipstream.")
    p.add_argument("--csv", required=True)
    p.add_argument("--symbol", required=True)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=14300)
    p.add_argument("--speed", type=float, default=1.0, help="replay rate multiplier; 1.0 is real time")

    return p.parse_args()

def handle(msg_type, body, stop):
    if msg_type is wire.MsgType.NEW_ORDER:
        coid, sym, status, ts_ns, trade_id, side, qty, limit_px = wire.NEW_ORDER_BODY.unpack(body)
        print(f"NewOrder #{coid} {status.decode()} trade={trade_id} {side.decode()} {qty} @ {limit_px / wire.SCALE:.4f}")

    elif msg_type is wire.MsgType.HEARTBEAT:
        ts_ns, = wire.HEARTBEAT_BODY.unpack(body)
        print(f"Heartbeat {ts_ns}")

    elif msg_type is wire.MsgType.SESSION_CONTROL:
        ts_ns, state = wire.SESSION_CONTROL_BODY.unpack(body)
        name = {0: "OPEN", 1: "HALT", 2: "CLOSE"}.get(state, str(state))
        print(f"SessionControl {name}")
        if state == 2:
            stop.set()

def read_loop(sock, stop):
    reader = wire.FrameReader()
    while True:
        data = sock.recv(4096)
        if not data:
            break

        reader.feed(data)
        for msg_type, body in reader:
            handle(msg_type, body, stop)

    stop.set()

def main():
    args = parse_args()
    rows = read_rows(args.csv, "T", args.symbol)
    stop = threading.Event()

    with socket.create_connection((args.host, args.port)) as sock:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        t = threading.Thread(target=read_loop, args=(sock, stop), daemon=True)
        t.start()

        sent = 0
        for row in replay(rows, args.speed):
            if stop.is_set():
                break

            sent += 1
            frame = wire.trade_frame(
                args.symbol,
                time.time_ns(),
                int(row["Qty"]),
                wire.to_fixed(row["Price"]),
                row["Aggressor"],
                sent,
            )
            sock.sendall(frame)

        sock.shutdown(socket.SHUT_WR)
        t.join(timeout=5.0)

    print(f"sent {sent} trades")

if __name__ == "__main__":
    main()