#!/usr/bin/env python3

import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=0)
    args = parser.parse_args()

    deadline = time.monotonic() + args.seconds if args.seconds > 0 else None
    with serial.serial_for_url(args.url, baudrate=args.baud, timeout=0.5) as port:
        while True:
            if deadline is not None and time.monotonic() >= deadline:
                return 0
            data = port.read(512)
            if not data:
                continue
            sys.stdout.write(data.decode("utf-8", errors="replace"))
            sys.stdout.flush()


if __name__ == "__main__":
    raise SystemExit(main())
