#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include "stm32c0xx_hal.h"

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    SOFT_I2C_OK = 0,
    SOFT_I2C_NACK = -1,
    SOFT_I2C_TIMEOUT = -2,
    SOFT_I2C_INVALID_ARGUMENT = -3
} SoftI2C_Status;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t scl_pin;
    uint16_t sda_pin;
    uint32_t timeout_cycles;
} SoftI2C;

void SoftI2C_Init(SoftI2C *bus);

SoftI2C_Status SoftI2C_WriteRegister(
    SoftI2C *bus,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
);

SoftI2C_Status SoftI2C_ReadRegisters(
    SoftI2C *bus,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
);

#endif
