# Звіт до лабораторної роботи №2

## Тема

Інтеграція сенсорної підсистеми STM32, перевірка I2C логічним аналізатором та поділ програми на шари.

## Мета роботи

Потрібно було переробити програму з ЛР1 так, щоб окремо знаходились робота з платою, драйвер BMP180 та логіка програми. Також треба було додати self-test, тести на комп'ютері, CI та перевірити I2C через Logic Analyzer.

## Використані компоненти

- NUCLEO-C031C6;
- BMP180 на PB6/PB7, адреса `0x77`;
- потенціометр на PA1;
- кнопка на PB0;
- USART2, PA2/PA3, 115200 бод;
- Logic Analyzer: D0=SCL, D1=SDA.

Докладна таблиця підключень знаходиться у `docs/pinout.md`.

## Що було змінено

У ЛР1 файл `lab_app.c` містив майже всю програму. У ЛР2 його було розділено:

- `Platform/soft_i2c.*` - програмний I2C;
- `Platform/board.*` - UART, ADC, кнопка, час і піни плати;
- `Devices/bmp180.*` - драйвер BMP180 та обчислення;
- `App/app.*` - self-test і періодичне виведення даних.

Hardware I2C у `.ioc` залишився, але після запуску він вимикається. PB6/PB7 далі працюють через software I2C, тому що цей варіант стабільно працює у Wokwi.

Драйвер BMP180 не включає STM32 HAL. Він отримує функції читання, запису та затримки через `Bmp180Bus`. Завдяки цьому той самий драйвер використовується і у firmware, і в host-тестах.

## Self-test

При запуску у Wokwi отримав:

```text
SELFTEST START
UART STATUS=OK
BMP180 ID=0x55 STATUS=OK
BMP180 TEMP=+24.0C PRESSURE=101327Pa STATUS=OK
ADC VALUE=2050 STATUS=OK
BUTTON STATE=RELEASED STATUS=OK
SELFTEST RESULT=PASS
```

Після self-test дані виводилися раз на 1000 мс. Кнопка змінювала стан між `released` і `PRESSED`. Під час зміни потенціометра ADC приймав значення `2050`, `348`, `1349`, `1229`, `1137` і `3350`.

Після зміни параметрів BMP180 тиск змінився з `101327 Pa` до `95971 Pa`, а потім температура і тиск стали `+62.7 °C` та `53500 Pa`. Отже, firmware використовував дані компонента Wokwi, а не наперед задані значення.

## Тести

Створено три файли:

- `test_bmp180_compensation.c`;
- `test_bmp180_errors.c`;
- `test_app.c`.

Тести перевіряють:

- приклад розрахунку з datasheet BMP180;
- реальні UT=`0x8689` та UP=`0x40C2` з Logic Analyzer;
- неправильний CHIP_ID;
- NACK і timeout;
- байти `0x77`, `0xD0`, `0x55`;
- self-test і періодичний вивід App.

Результат запуску:

```text
test_bmp180_compensation: PASS
test_bmp180_errors: PASS
test_app: PASS
```

Coverage для `App` та `Devices`:

- рядки - `78.1%`;
- функції - `95.0%`.

## Збірка та перевірка архітектури

Основні команди:

```sh
make docker
make build
make test
make coverage
make check-architecture
```

Firmware зібрався через ARM GCC. Розмір normal firmware:

```text
text = 16456 bytes
data = 124 bytes
bss  = 1980 bytes
```

`check-architecture` підтвердив, що у `App` та `Devices` немає залежності від STM32 HAL. Для pull request додано `.github/workflows/ci.yml`, де запускаються build, tests та architecture check.

## Logic Analyzer

Було записано три реальні VCD:

1. CHIP_ID: `0x77/W → 0xD0 → 0x77/R → 0x55`.
2. Calibration і measurement: 22 байти коефіцієнтів, UT=`86 89`, UP=`40 C2 00`.
3. NACK: адреса `0x76` отримала NACK, після чого був STOP.

Детальний розбір знаходиться у `docs/captures/analysis.md`.

## Висновок

У цій роботі я розділив програму на Core, Platform, Devices та App. Після переробки BMP180, ADC, кнопка й UART продовжили працювати у Wokwi. Для BMP180 та App додано host-тести, а залежність від HAL перевіряється окремим скриптом. Роботу I2C підтверджено трьома записами Logic Analyzer.
