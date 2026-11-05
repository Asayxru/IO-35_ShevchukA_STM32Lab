#include "app.h"

#include "bmp180.h"
#include "board.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef APP_CAPTURE_MODE
#define APP_CAPTURE_MODE 0
#endif

#define APP_CAPTURE_NORMAL 0
#define APP_CAPTURE_CHIP_ID 1
#define APP_CAPTURE_MEASUREMENT 2
#define APP_CAPTURE_NACK 3

#define APP_TELEMETRY_INTERVAL_MS 1000u
#define APP_BAD_I2C_ADDRESS 0x76u

static Bmp180 bmp180;
static bool bmp180_ready;
static bool capture_only;
static uint32_t last_telemetry_tick;

static void app_log(const char *format, ...)
{
    char buffer[192];
    va_list arguments;

    va_start(arguments, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    Board_UartWrite(buffer);
}

static Bmp180Bus app_bmp180_bus(void)
{
    Bmp180Bus bus = {
        .context = NULL,
        .write = Board_I2cWrite,
        .read = Board_I2cRead,
        .delay_ms = Board_DelayMs
    };
    return bus;
}

static void run_capture_scenario(const Bmp180Bus *bus)
{
    uint8_t id = 0u;
    uint8_t ignored = 0u;
    int32_t temperature_x10 = 0;
    int32_t pressure_pa = 0;
    int result;
    Bmp180Status status;

    capture_only = true;
    app_log("CAPTURE MODE=%d START\r\n", APP_CAPTURE_MODE);

    if (APP_CAPTURE_MODE == APP_CAPTURE_CHIP_ID)
    {
        status = Bmp180_ReadChipId(bus, &id);
        app_log(
            "CAPTURE CHIP_ID=0x%02X STATUS=%s\r\n",
            id,
            Bmp180_StatusString(status)
        );
    }
    else if (APP_CAPTURE_MODE == APP_CAPTURE_MEASUREMENT)
    {
        status = Bmp180_Init(&bmp180, bus);
        if (status == BMP180_STATUS_OK)
        {
            status = Bmp180_ReadMeasurement(
                &bmp180,
                &temperature_x10,
                &pressure_pa
            );
        }
        app_log(
            "CAPTURE MEASUREMENT TEMP_X10=%ld PRESSURE=%ld STATUS=%s\r\n",
            (long)temperature_x10,
            (long)pressure_pa,
            Bmp180_StatusString(status)
        );
    }
    else if (APP_CAPTURE_MODE == APP_CAPTURE_NACK)
    {
        result = Board_I2cRead(
            NULL,
            APP_BAD_I2C_ADDRESS,
            0xd0u,
            &ignored,
            1u
        );
        app_log(
            "CAPTURE NACK ADDRESS=0x%02X STATUS=%s\r\n",
            APP_BAD_I2C_ADDRESS,
            (result == BOARD_BUS_NACK) ? "NACK" : "ERROR"
        );
    }

    app_log("CAPTURE END\r\n");
}

static void run_self_test(const Bmp180Bus *bus)
{
    uint32_t adc_value = 0u;
    int32_t temperature_x10 = 0;
    int32_t pressure_pa = 0;
    int32_t temperature_absolute;
    char temperature_sign;
    bool adc_ok;
    bool button_pressed;
    bool self_test_pass;
    Bmp180Status init_status;
    Bmp180Status measurement_status = BMP180_STATUS_NOT_INITIALIZED;

    app_log("SELFTEST START\r\n");
    app_log("UART STATUS=OK\r\n");

    init_status = Bmp180_Init(&bmp180, bus);
    bmp180_ready = init_status == BMP180_STATUS_OK;
    app_log(
        "BMP180 ID=0x%02X STATUS=%s\r\n",
        bmp180.chip_id,
        Bmp180_StatusString(init_status)
    );

    if (bmp180_ready)
    {
        measurement_status = Bmp180_ReadMeasurement(
            &bmp180,
            &temperature_x10,
            &pressure_pa
        );
    }

    if (measurement_status == BMP180_STATUS_OK)
    {
        temperature_sign = temperature_x10 < 0 ? '-' : '+';
        temperature_absolute = temperature_x10 < 0
            ? -temperature_x10
            : temperature_x10;
        app_log(
            "BMP180 TEMP=%c%ld.%ldC PRESSURE=%ldPa STATUS=OK\r\n",
            temperature_sign,
            (long)(temperature_absolute / 10),
            (long)(temperature_absolute % 10),
            (long)pressure_pa
        );
    }
    else
    {
        app_log(
            "BMP180 TEMP=NA PRESSURE=NA STATUS=%s\r\n",
            Bmp180_StatusString(measurement_status)
        );
    }

    adc_ok = Board_AdcRead(&adc_value);
    app_log(
        "ADC VALUE=%lu STATUS=%s\r\n",
        (unsigned long)adc_value,
        adc_ok ? "OK" : "ERROR"
    );

    button_pressed = Board_ButtonPressed();
    app_log(
        "BUTTON STATE=%s STATUS=OK\r\n",
        button_pressed ? "PRESSED" : "RELEASED"
    );

    self_test_pass = bmp180_ready &&
                     (measurement_status == BMP180_STATUS_OK) &&
                     adc_ok;
    app_log(
        "SELFTEST RESULT=%s\r\n",
        self_test_pass ? "PASS" : "FAIL"
    );
}

void App_Init(void)
{
    Bmp180Bus bus;

    Board_Init();
    Board_DelayMs(100u);
    bus = app_bmp180_bus();

    if (APP_CAPTURE_MODE != APP_CAPTURE_NORMAL)
    {
        run_capture_scenario(&bus);
        return;
    }

    run_self_test(&bus);
    last_telemetry_tick = Board_Millis();
}

void App_Loop(void)
{
    uint32_t now;
    uint32_t adc_value = 0u;
    int32_t temperature_x10 = 0;
    int32_t pressure_pa = 0;
    int32_t temperature_absolute;
    bool adc_ok;
    bool button_pressed;
    Bmp180Status measurement_status = BMP180_STATUS_NOT_INITIALIZED;
    char temperature_sign = '+';

    if (capture_only)
    {
        return;
    }

    now = Board_Millis();
    if ((uint32_t)(now - last_telemetry_tick) < APP_TELEMETRY_INTERVAL_MS)
    {
        return;
    }
    last_telemetry_tick = now;

    adc_ok = Board_AdcRead(&adc_value);
    button_pressed = Board_ButtonPressed();
    if (bmp180_ready)
    {
        measurement_status = Bmp180_ReadMeasurement(
            &bmp180,
            &temperature_x10,
            &pressure_pa
        );
    }

    if (adc_ok && (measurement_status == BMP180_STATUS_OK))
    {
        temperature_absolute = temperature_x10;
        if (temperature_x10 < 0)
        {
            temperature_sign = '-';
            temperature_absolute = -temperature_x10;
        }

        app_log(
            "tick=%lu adc=%lu button=%s temp=%c%ld.%ldC pressure=%ldPa\r\n",
            (unsigned long)now,
            (unsigned long)adc_value,
            button_pressed ? "PRESSED" : "released",
            temperature_sign,
            (long)(temperature_absolute / 10),
            (long)(temperature_absolute % 10),
            (long)pressure_pa
        );
    }
    else
    {
        app_log(
            "tick=%lu adc=%lu button=%s BMP180=%s ADC=%s\r\n",
            (unsigned long)now,
            (unsigned long)adc_value,
            button_pressed ? "PRESSED" : "released",
            Bmp180_StatusString(measurement_status),
            adc_ok ? "OK" : "ERROR"
        );
    }
}
