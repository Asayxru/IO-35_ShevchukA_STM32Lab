# Hardware Components

## 1. STM32C031C6

У роботі використовується мікроконтролер STM32C031C6 на ядрі Arm Cortex-M0+.

| Параметр | Значення | Джерело |
|---|---|---|
| Ядро | Arm Cortex-M0+ | STM32C031x4/x6 Datasheet, p. 1 |
| Максимальна частота | 48 MHz | STM32C031x4/x6 Datasheet, p. 1 |
| Flash | до 32 KB | STM32C031x4/x6 Datasheet, p. 1 |
| SRAM | 12 KB | STM32C031x4/x6 Datasheet, p. 1 |
| Живлення | 2.0-3.6 V | STM32C031x4/x6 Datasheet, p. 1 |
| ADC | 12-bit | STM32C031x4/x6 Datasheet, p. 1 |
| I2C1 SCL | PB6, AF6 | STM32C031x4/x6 Datasheet, Table 15 |
| I2C1 SDA | PB7, AF6 | STM32C031x4/x6 Datasheet, Table 15 |

Datasheet:  
https://www.st.com/resource/en/datasheet/stm32c031c6.pdf

### Піни, які використовуються у роботі

| Пін | Функція |
|---|---|
| PA1 | ADC1_IN1 - потенціометр |
| PA2 | USART2_TX |
| PA3 | USART2_RX |
| PB0 | GPIO Input - кнопка |
| PB6 | I2C SCL |
| PB7 | I2C SDA |

У CubeMX частота SYSCLK налаштована на 48 MHz.

## 2. BMP180

BMP180 - цифровий датчик температури і атмосферного тиску.

| Параметр | Значення | Джерело |
|---|---|---|
| Інтерфейс | I2C | BMP180 Datasheet |
| I2C address | `0x77` | BMP180 Datasheet / Wokwi |
| Діапазон тиску | 300-1100 hPa | BMP180 Datasheet, p. 2 |
| Живлення | 1.8-3.6 V | BMP180 Datasheet, p. 2 |
| Calibration coefficients | 11 значень по 16 біт | BMP180 Datasheet, p. 13 |
| Команда температури | `0x2E` -> `0xF4` | BMP180 Datasheet, p. 21 |
| Команда тиску | `0x34` -> `0xF4` | BMP180 Datasheet, p. 21 |

### Регістри, які використовуються

| Register | Для чого |
|---|---|
| `0xAA-0xBF` | calibration coefficients |
| `0xD0` | CHIP_ID |
| `0xF4` | запуск вимірювання |
| `0xF6-0xF8` | дані вимірювання |

Під час роботи ми перевірили CHIP_ID. Logic Analyzer показав:

- адреса BMP180 - `0x77`;
- регістр - `0xD0`;
- відповідь сенсора - `0x55`.

Джерела:

- Bosch BMP180 Datasheet:  
  https://datacapturecontrol.com/docs/datasheets/bmp180-datasheet.pdf
- Wokwi BMP180:  
  https://docs.wokwi.com/parts/board-bmp180

## 3. Potentiometer

Потенціометр використовується як просте джерело аналогового сигналу.

| Пін | Підключення |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SIG | PA1 / ADC1_IN1 |

STM32 читає це значення через 12-bit ADC. Під час тесту значення змінювалося приблизно в діапазоні 0-4095.

Wokwi reference:  
https://docs.wokwi.com/parts/wokwi-potentiometer

## 4. Pushbutton

Кнопка підключена до PB0.

| Параметр | Значення |
|---|---|
| MCU pin | PB0 |
| Mode | GPIO Input |
| Pull | Pull-Up |
| Active state | LOW |

Коли кнопка не натиснута, у програмі отримуємо HIGH. Коли натиснута - LOW.

## 5. UART

Для виводу логів використовується USART2:

- PA2 - TX;
- PA3 - RX;
- 115200 baud;
- 8 data bits;
- 1 stop bit;
- без parity.

Для `printf()` використовується функція `_write()`, яка викликає `HAL_UART_Transmit()`.

## 6. Logic Analyzer

Logic Analyzer у Wokwi підключений так:

- D0 -> PB6 / SCL;
- D1 -> PB7 / SDA.

Після зупинки симуляції Wokwi створює VCD-файл. Його можна відкрити в PulseView і додати I2C decoder.

У нашому випадку PulseView показав читання CHIP_ID:

`0x77 -> 0xD0 -> 0x77 -> 0x55`
