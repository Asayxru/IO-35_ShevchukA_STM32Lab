#include "app.h"
#include "board.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static char uart_output[4096];
static size_t uart_length;
static uint32_t fake_tick;
static uint8_t last_command;

static const uint8_t calibration_bytes[22] = {
    0x01, 0x98, 0xff, 0xb8, 0xc7, 0xd1, 0x7f, 0xe5,
    0x7f, 0xf5, 0x5a, 0x71, 0x18, 0x2e, 0x00, 0x04,
    0x80, 0x00, 0xdd, 0xf9, 0x0b, 0x34
};

void Board_Init(void)
{
}

uint32_t Board_Millis(void)
{
    return fake_tick;
}

void Board_DelayMs(uint32_t delay_ms)
{
    fake_tick += delay_ms;
}

void Board_UartWrite(const char *text)
{
    size_t available = sizeof(uart_output) - uart_length - 1u;
    size_t length = strlen(text);
    if (length > available)
    {
        length = available;
    }
    (void)memcpy(&uart_output[uart_length], text, length);
    uart_length += length;
    uart_output[uart_length] = '\0';
}

bool Board_AdcRead(uint32_t *value)
{
    *value = 2048u;
    return true;
}

bool Board_ButtonPressed(void)
{
    return false;
}

int Board_I2cWrite(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
)
{
    (void)context;
    assert(address == 0x77u);
    if ((reg == 0xf4u) && (length == 1u))
    {
        last_command = data[0];
    }
    return BOARD_BUS_OK;
}

int Board_I2cRead(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
)
{
    (void)context;
    assert(address == 0x77u);

    if ((reg == 0xd0u) && (length == 1u))
    {
        data[0] = 0x55u;
    }
    else if ((reg == 0xaau) && (length == sizeof(calibration_bytes)))
    {
        (void)memcpy(data, calibration_bytes, length);
    }
    else if ((reg == 0xf6u) && (length == 2u) && (last_command == 0x2eu))
    {
        data[0] = 0x6cu;
        data[1] = 0xfau;
    }
    else if ((reg == 0xf6u) && (length == 3u) && (last_command == 0x34u))
    {
        data[0] = 0x5du;
        data[1] = 0x23u;
        data[2] = 0x00u;
    }
    else
    {
        return BOARD_BUS_ERROR;
    }
    return BOARD_BUS_OK;
}

int main(void)
{
    App_Init();
    assert(strstr(uart_output, "BMP180 ID=0x55 STATUS=OK") != NULL);
    assert(strstr(uart_output, "SELFTEST RESULT=PASS") != NULL);

    fake_tick += 1000u;
    App_Loop();
    assert(strstr(uart_output, "adc=2048") != NULL);
    assert(strstr(uart_output, "pressure=69964Pa") != NULL);

    puts("test_app: PASS");
    return 0;
}
