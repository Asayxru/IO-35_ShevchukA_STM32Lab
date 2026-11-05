#!/usr/bin/env python3
"""Перевірка, що App і Devices не підключають STM32 HAL напряму."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
LAYERS = (ROOT / "firmware" / "App", ROOT / "firmware" / "Devices")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
FORBIDDEN = (
    "stm32",
    "main.h",
    "adc.h",
    "gpio.h",
    "i2c.h",
    "usart.h",
)


def main() -> int:
    violations: list[str] = []
    for layer in LAYERS:
        for source in sorted(layer.glob("*.[ch]")):
            for number, line in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
                match = INCLUDE.match(line)
                if match and any(token in match.group(1).lower() for token in FORBIDDEN):
                    relative = source.relative_to(ROOT)
                    violations.append(f"{relative}:{number}: forbidden include {match.group(1)}")

    if violations:
        print("Architecture check failed:")
        print("\n".join(violations))
        return 1

    print("Architecture check passed: App and Devices are independent of STM32 HAL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
