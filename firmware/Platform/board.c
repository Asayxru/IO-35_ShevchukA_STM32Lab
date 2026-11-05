#include "board.h"

#include "adc.h"
#include "i2c.h"
#include "soft_i2c.h"
#include "usart.h"

#include <string.h>

#define BOARD_SOFT_I2C_TIMEOUT_CYCLES 10000u

static SoftI2C board_i2c = {
    .port = GPIOB,
    .scl_pin = GPIO_PIN_6,
    .sda_pin = GPIO_PIN_7,
    .timeout_cycles = BOARD_SOFT_I2C_TIMEOUT_CYCLES
};

void Board_Init(void)
{
    (void)HAL_I2C_DeInit(&hi2c1);
    __HAL_RCC_GPIOB_CLK_ENABLE();
    SoftI2C_Init(&board_i2c);
    HAL_Delay(10u);
}

uint32_t Board_Millis(void)
{
    return HAL_GetTick();
}

void Board_DelayMs(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
}

void Board_UartWrite(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    (void)HAL_UART_Transmit(
        &huart2,
        (const uint8_t *)text,
        (uint16_t)strlen(text),
        HAL_MAX_DELAY
    );
}

int _write(int fd, char *ptr, int len)
{
    (void)fd;

    if ((ptr == NULL) || (len <= 0))
    {
        return 0;
    }

    (void)HAL_UART_Transmit(
        &huart2,
        (const uint8_t *)ptr,
        (uint16_t)len,
        HAL_MAX_DELAY
    );
    return len;
}

bool Board_AdcRead(uint32_t *value)
{
    bool ok = false;

    if (value == NULL)
    {
        return false;
    }

    if (HAL_ADC_Start(&hadc1) == HAL_OK)
    {
        if (HAL_ADC_PollForConversion(&hadc1, 100u) == HAL_OK)
        {
            *value = HAL_ADC_GetValue(&hadc1);
            ok = true;
        }
        (void)HAL_ADC_Stop(&hadc1);
    }

    return ok;
}

bool Board_ButtonPressed(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET;
}

static int board_bus_status(SoftI2C_Status status)
{
    switch (status)
    {
        case SOFT_I2C_OK:
            return BOARD_BUS_OK;
        case SOFT_I2C_NACK:
            return BOARD_BUS_NACK;
        case SOFT_I2C_TIMEOUT:
            return BOARD_BUS_TIMEOUT;
        default:
            return BOARD_BUS_ERROR;
    }
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
    return board_bus_status(
        SoftI2C_WriteRegister(&board_i2c, address, reg, data, length)
    );
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
    return board_bus_status(
        SoftI2C_ReadRegisters(&board_i2c, address, reg, data, length)
    );
}
