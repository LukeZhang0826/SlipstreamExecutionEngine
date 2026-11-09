from argparse import ArgumentParser
import socket
import time

import wire
from replay import read_rows, replay

def parse_args():
    p = ArgumentParser(description="Replay CSV quotes to slipstream.")
    p.add_argument("--csv", required=True)
    p.add_argument("--symbol", required=True)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=14200) # how do we know?
    p.add_argument("--speed", type=float, default=1.0, help="replay rate multiplier; 1.0 is real time")

    return p.parse_args()

def main():
    args = parse_args()
    rows = read_rows(args.csv, "Q", args.symbol)

    with socket.create_connection((args.host, args.port)) as sock:
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        sent = 0
        for row in replay(rows, args.speed):
            frame = wire.quote_frame(
                args.symbol,
                time.time_ns(),
                int(row["BidQty"]),
                wire.to_fixed(row["BidPrice"]),
                int(row["AskQty"]),
                wire.to_fixed(row["AskPrice"]),
            )
            sock.sendall(frame)
            sent += 1

        print(f"sent {sent} quotes")


if __name__ == "__main__":
    main()