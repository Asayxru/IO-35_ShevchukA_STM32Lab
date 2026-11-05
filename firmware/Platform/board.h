#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    BOARD_BUS_OK = 0,
    BOARD_BUS_NACK = -1,
    BOARD_BUS_TIMEOUT = -2,
    BOARD_BUS_ERROR = -3
};

void Board_Init(void);
uint32_t Board_Millis(void);
void Board_DelayMs(uint32_t delay_ms);
void Board_UartWrite(const char *text);
bool Board_AdcRead(uint32_t *value);
bool Board_ButtonPressed(void);

int Board_I2cWrite(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
);

int Board_I2cRead(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
);

#endif
