#!/usr/bin/env python3
"""Serieller Mitschnitt ohne Auto-Reset.

Oeffnet den Port mit DTR=False/RTS=False (wie monitor_dtr=0/monitor_rts=0),
damit der ESP beim Verbinden NICHT resettet wird. Laeuft bis Abbruch.

Aufruf:  ~/.platformio/penv/bin/python tools/serial_capture.py [port] [baud]
"""
import sys
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

s = serial.Serial()
s.port = port
s.baudrate = baud
s.dtr = False   # kein Auto-Reset beim Oeffnen
s.rts = False
s.timeout = 1
s.open()

print(f"[capture] {port} @ {baud} (dtr/rts=0)", flush=True)
try:
    while True:
        line = s.readline()
        if line:
            sys.stdout.write(line.decode("utf-8", "replace"))
            sys.stdout.flush()
except KeyboardInterrupt:
    pass
finally:
    s.close()
