#!/usr/bin/env python3
"""Просте декодування SCL/SDA із VCD, записаного у Wokwi."""

from pathlib import Path
import argparse
import statistics


def parse_vcd(path: Path):
    symbols: dict[str, str] = {}
    events: list[tuple[int, str, int]] = []
    timestamp = 0

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("$var"):
            fields = line.split()
            symbols[fields[4].upper()] = fields[3]
        elif line.startswith("#"):
            timestamp = int(line[1:])
        elif len(line) >= 2 and line[0] in "01":
            events.append((timestamp, line[1:], int(line[0])))

    try:
        scl_symbol = symbols["SCL"]
        sda_symbol = symbols["SDA"]
    except KeyError as error:
        raise ValueError("VCD must contain signals named SCL and SDA") from error

    return events, scl_symbol, sda_symbol


def decode(path: Path):
    events, scl_symbol, sda_symbol = parse_vcd(path)
    scl = 1
    sda = 1
    active = False
    bits: list[int] = []
    byte_index = 0
    decoded: list[str] = []
    rising_edges: list[int] = []

    event_index = 0
    while event_index < len(events):
        timestamp = events[event_index][0]
        previous_scl = scl
        previous_sda = sda

        while event_index < len(events) and events[event_index][0] == timestamp:
            _, symbol, value = events[event_index]
            if symbol == scl_symbol:
                scl = value
            elif symbol == sda_symbol:
                sda = value
            event_index += 1

        if previous_sda != sda and previous_scl == 1 and scl == 1:
            if previous_sda == 1 and sda == 0:
                active = True
                bits.clear()
                byte_index = 0
                decoded.append(f"{timestamp / 1_000_000:.3f} ms START")
            elif previous_sda == 0 and sda == 1 and active:
                active = False
                bits.clear()
                decoded.append(f"{timestamp / 1_000_000:.3f} ms STOP")

        if active and previous_scl == 0 and scl == 1:
            rising_edges.append(timestamp)
            bits.append(sda)
            if len(bits) == 9:
                value_byte = 0
                for bit in bits[:8]:
                    value_byte = (value_byte << 1) | bit
                acknowledgment = "ACK" if bits[8] == 0 else "NACK"
                if byte_index == 0:
                    address = value_byte >> 1
                    direction = "READ" if value_byte & 1 else "WRITE"
                    decoded.append(
                        f"{timestamp / 1_000_000:.3f} ms "
                        f"ADDRESS 0x{address:02X} {direction} {acknowledgment}"
                    )
                else:
                    decoded.append(
                        f"{timestamp / 1_000_000:.3f} ms "
                        f"DATA 0x{value_byte:02X} {acknowledgment}"
                    )
                byte_index += 1
                bits.clear()

    periods = [
        current - previous
        for previous, current in zip(rising_edges, rising_edges[1:])
        if 0 < current - previous < 100_000_000
    ]
    frequency = None
    if periods:
        frequency = 1_000_000_000 / statistics.median(periods)
    return decoded, frequency


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("vcd", type=Path)
    arguments = parser.parse_args()
    decoded, frequency = decode(arguments.vcd)
    for line in decoded:
        print(line)
    if frequency is not None:
        print(f"Median SCL frequency: {frequency:.1f} Hz")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
