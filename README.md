# Slipstream

A volume-aware execution engine for a single instrument. `slipstream` consumes a
binary quote feed and a stream of historical trades over TCP, maintains a rolling
VWAP benchmark, and decides for each trade whether taking it was the right call.

Two Python clients replay `data/QandT.csv` on a wall-clock-relative schedule: the
market data client sends quotes, the order entry client sends trades and receives
the verdicts back.

## Build

cmake -S . -B build
cmake --build build

GCC 16, C++23, `-Wall -Wextra -Wpedantic` clean. GoogleTest is fetched at configure
time, so the first configure needs network access.

## Test

ctest --test-dir build --output-on-failure

17 tests covering round trips for all six message types, frame reassembly split at
every byte boundary, and malformed input (bad version, oversized body length, wrong
body length for the type, truncated frames, unknown message types).

## Clients

python3 clients/md_client.py --csv data/QandT.csv --symbol SYNTH1 --port 14200
python3 clients/oe_client.py --csv data/QandT.csv --symbol SYNTH1 --port 14300

A faithful replay of the file takes 59 minutes. `--speed 60` compresses it to one
minute for development. The default is real time.