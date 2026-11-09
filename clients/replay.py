import csv
import time
from datetime import datetime

def read_rows(path, row_type, symbol):
    with open(path, newline="") as f:
        lines = [line for line in f if not line.startswith('#')]

    base = None
    for row in csv.DictReader(lines):
        ts = datetime.strptime(row["Timestamp"], "%H:%M:%S.%f")
        if base is None:
            base = ts
        if row["Type"] == row_type and row["Symbol"] == symbol:
            yield (ts - base).total_seconds(), row

def replay(rows, speed=1.0):
    """
    generator that yields each row at its scheduled moment, relative to the first call
    :param speed:
    :param rows:
    :return:
    """
    start = time.monotonic()
    for offset_s, row in rows:
        due_time = start + (offset_s / speed)
        delay = due_time - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        yield row
