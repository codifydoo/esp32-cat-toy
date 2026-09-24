#!/usr/bin/env python3
"""Print the board's serial output for N seconds (a non-interactive idf.py monitor)."""
import sys
import time

import serial


def open_port(port, deadline):
    while True:
        try:
            s = serial.Serial()
            s.port = port
            s.baudrate = 115200
            s.timeout = 0.2
            # Opening the port must not reset the board: a second reset within ~1 s of the flasher's
            # can hang early boot (docs/device-checks.md). The chip is held in reset while RTS is high
            # and DTR low. The OS raises both on open, and pyserial then clears DTR before RTS, which
            # passes through that state. So skip pyserial's DTR update on open (on POSIX dsrdtr only
            # does that), let it drop RTS, and drop DTR afterwards.
            s.dsrdtr = True
            s.rts = False
            s.open()
            s.dtr = False
            return s
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
