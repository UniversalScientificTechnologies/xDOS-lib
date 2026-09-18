#!/usr/bin/env python3
"""Command line client of the xDOS configuration bridge.

Opens the device's serial port, switches it into config mode and runs the
given bridge commands, e.g. reading the factory serial number of the EEPROM
at 0x5B:

    xdos_bridge.py /dev/ttyACM0 I2CWR,5B,0800,10

Commands are written without the leading '!' and without the checksum; the
tool adds both and leaves config mode with EXIT at the end (unless --stay).
See docs/CONFIG_BRIDGE.md for the protocol.

Opening the port may reset the device (SPACEDOS04: the nRF52840 pulses the
ATmega reset on DTR), so by default the tool first waits for the device to
print something before it sends !CONFIG.
"""

import argparse
import sys
import time

TOKEN = "XDOS"


def checksum(body):
    """NMEA 0183 checksum - XOR of all characters."""
    value = 0
    for ch in body:
        value ^= ord(ch)
    return value


def frame_command(body):
    """"I2CR,5B,10" -> b"!I2CR,5B,10*hh\\r\\n"."""
    return ("!%s*%02X\r\n" % (body, checksum(body))).encode("ascii")


def parse_response(line):
    """Body of a valid "#<body>*hh" response line, or None."""
    line = line.strip()
    if len(line) < 4 or not line.startswith("#") or line[-3] != "*":
        return None
    body = line[1:-3]
    try:
        expected = int(line[-2:], 16)
    except ValueError:
        return None
    if checksum(body) != expected:
        return None
    return body


def is_error(body):
    return body.startswith("CMD_ERR") or body.startswith("I2C_ERR")


class Bridge:
    def __init__(self, port, verbose=False):
        self.port = port
        self.verbose = verbose

    def _readline(self, deadline):
        while time.monotonic() < deadline:
            raw = self.port.readline()
            if raw:
                return raw.decode("ascii", errors="replace").rstrip("\r\n")
        return None

    def _log(self, text):
        if self.verbose:
            print("  " + text)

    def wait_for_device(self, timeout):
        """Waits for any line from the running firmware."""
        deadline = time.monotonic() + timeout
        while True:
            line = self._readline(deadline)
            if line is None:
                return False
            self._log(line)
            if line.startswith("$") or line.startswith("#"):
                return True

    def _send(self, body):
        self.port.write(frame_command(body))
        self.port.flush()

    def _wait_response(self, deadline, prefixes=None, skip=None):
        while True:
            line = self._readline(deadline)
            if line is None:
                return None
            body = parse_response(line)
            if (body is None or (prefixes and not body.startswith(prefixes))
                    or (skip and body.startswith(skip))):
                self._log(line)
                continue
            return body

    def enter(self, timeout, resend=5.0):
        """Sends !CONFIG until the device answers CONFIG_MODE. The firmware
        only reads commands in its report window, so the first answer can
        take a few seconds; sending !CONFIG again is harmless."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self._send("CONFIG," + TOKEN)
            body = self._wait_response(min(deadline, time.monotonic() + resend),
                                       ("CONFIG_MODE", "CMD_ERR"))
            if body is not None:
                return body
        return None

    def command(self, body, timeout=5.0):
        self._send(body)
        # enter() may have sent !CONFIG more than once; the extra CONFIG_MODE
        # answers are still on the way and do not belong to this command.
        skip = None if body.startswith("CONFIG") else "CONFIG_MODE"
        return self._wait_response(time.monotonic() + timeout, skip=skip)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Run commands of the xDOS configuration bridge.")
    parser.add_argument("port", help="serial port, e.g. /dev/ttyACM0")
    parser.add_argument("commands", nargs="*",
                        help='bridge commands without "!" and checksum, '
                             'e.g. I2CWR,5B,0800,10')
    parser.add_argument("-b", "--baud", type=int, default=500000,
                        help="baud rate (default 500000 = SPACEDOS04)")
    parser.add_argument("--no-wait", action="store_true",
                        help="do not wait for the device to print something "
                             "before sending !CONFIG")
    parser.add_argument("--stay", action="store_true",
                        help="do not send EXIT at the end")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="how long to wait for the device and for "
                             "config mode, in seconds (default 30)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="also print the device's other output")
    args = parser.parse_args(argv)

    import serial  # pyserial; imported here so the tests do not need it

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        bridge = Bridge(port, verbose=args.verbose)

        if not args.no_wait and not bridge.wait_for_device(args.timeout):
            print("no output from the device", file=sys.stderr)
            return 2

        answer = bridge.enter(args.timeout)
        print("< " + str(answer))
        if answer is None or is_error(answer):
            print("config mode not entered", file=sys.stderr)
            return 2

        failed = False
        for body in args.commands:
            print("> " + body)
            answer = bridge.command(body)
            print("< " + str(answer))
            if answer is None or is_error(answer):
                failed = True

        if not args.stay:
            print("> EXIT")
            print("< " + str(bridge.command("EXIT")))

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
