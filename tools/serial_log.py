#!/usr/bin/env python3
"""Print the board's serial output for N seconds (a non-interactive idf.py monitor).

Usage: serial_log.py PORT [SECONDS] [-t]
-t prefixes each line with the host time (HH:MM:SS.mmm), to time deep sleep against the RTC. The port
vanishes while the chip deep-sleeps (USB goes down with it); the logger waits for it and says so."""
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


def stamp():
    t = time.time()
    return time.strftime("%H:%M:%S", time.localtime(t)) + ".%03d " % int((t % 1) * 1000)


def main():
    stamps = "-t" in sys.argv[1:]
    args = [a for a in sys.argv[1:] if a != "-t"]
    port = args[0]
    seconds = float(args[1]) if len(args) > 1 else 10.0
    end = time.time() + seconds
    s = open_port(port, time.time() + 5)
    at_line_start = True
    try:
        while time.time() < end:
            try:
                data = s.read(4096)
            except serial.SerialException:
                # Deep sleep takes USB down with the chip: wait for the port to come back.
                s.close()
                sys.stdout.write("\n[port gone at %s]\n" % stamp().strip())
                sys.stdout.flush()
                try:
                    s = open_port(port, end)
                except serial.SerialException:
                    return
                sys.stdout.write("[port back at %s]\n" % stamp().strip())
                at_line_start = True
                continue
            if not data:
                continue
            text = data.decode("utf-8", "replace")
            if stamps:
                pieces = text.split("\n")
                out = []
                for i, piece in enumerate(pieces):
                    if at_line_start and piece:
                        out.append(stamp())
                        at_line_start = False
                    out.append(piece)
                    if i < len(pieces) - 1:
                        out.append("\n")
                        at_line_start = True
                text = "".join(out)
            sys.stdout.write(text)
            sys.stdout.flush()
    finally:
        s.close()


if __name__ == "__main__":
    main()
