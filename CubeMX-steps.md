# CubeMX settings for the starter

Target: NUCLEO-C031C6 (Board Selector)
Toolchain: Makefile
Project name: lab1

Configure:
- USART2, asynchronous, 115200, 8 data bits, no parity, 1 stop bit; PA2=TX, PA3=RX.
- I2C1, 100 kHz; PB8=SCL, PB9=SDA.
- ADC1: PA0 / ADC_IN0, single conversion, software trigger.
- GPIO input: PB0, Pull-up, label USER_BUTTON (optional).
- Keep SysTick/HAL defaults.

Generate code into a folder named `firmware` at repository root.

Then copy:
- firmware_overlay/Core/Inc/lab_app.h -> firmware/Core/Inc/lab_app.h
- firmware_overlay/Core/Src/lab_app.c -> firmware/Core/Src/lab_app.c

Edit firmware/Core/Src/main.c only inside USER CODE sections:

1) USER CODE BEGIN Includes:
#include "lab_app.h"

2) After all MX_*_Init() calls, in USER CODE BEGIN 2:
LAB_AppInit();

3) Inside while(1), in USER CODE BEGIN WHILE or USER CODE BEGIN 3:
LAB_AppLoop();

Regenerate safely later: CubeMX preserves USER CODE sections and separate lab_app files.
