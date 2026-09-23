#!/usr/bin/env python3
"""Print the board's serial output for N seconds (a non-interactive idf.py monitor)."""
import sys
import time

import serial


def open_port(port, deadline):
    while True:
        try:
            return serial.Serial(port, 115200, timeout=0.2)
        except serial.SerialException:
            if time.time() > deadline:
                raise
            time.sleep(0.2)


def main():
    port = sys.argv[1]
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
    end = time.time() + seconds
    with open_port(port, time.time() + 5) as s:
        while time.time() < end:
            data = s.read(4096)
            if data:
                sys.stdout.write(data.decode("utf-8", "replace"))
                sys.stdout.flush()


if __name__ == "__main__":
    main()
