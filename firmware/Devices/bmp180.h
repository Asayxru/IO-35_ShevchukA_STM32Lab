#ifndef BMP180_H
#define BMP180_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BMP180_I2C_ADDRESS 0x77u
#define BMP180_EXPECTED_CHIP_ID 0x55u

enum
{
    BMP180_BUS_OK = 0,
    BMP180_BUS_NACK = -1,
    BMP180_BUS_TIMEOUT = -2
};

typedef int (*Bmp180BusWrite)(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
);

typedef int (*Bmp180BusRead)(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
);

typedef void (*Bmp180DelayMs)(uint32_t delay_ms);

typedef struct
{
    void *context;
    Bmp180BusWrite write;
    Bmp180BusRead read;
    Bmp180DelayMs delay_ms;
} Bmp180Bus;

typedef struct
{
    int16_t ac1;
    int16_t ac2;
    int16_t ac3;
    uint16_t ac4;
    uint16_t ac5;
    uint16_t ac6;
    int16_t b1;
    int16_t b2;
    int16_t mb;
    int16_t mc;
    int16_t md;
} Bmp180Calibration;

typedef enum
{
    BMP180_STATUS_OK = 0,
    BMP180_STATUS_NACK,
    BMP180_STATUS_TIMEOUT,
    BMP180_STATUS_BAD_ID,
    BMP180_STATUS_INVALID_ARGUMENT,
    BMP180_STATUS_CALIBRATION,
    BMP180_STATUS_COMPENSATION,
    BMP180_STATUS_NOT_INITIALIZED,
    BMP180_STATUS_BUS_ERROR
} Bmp180Status;

typedef struct
{
    Bmp180Bus bus;
    Bmp180Calibration calibration;
    uint8_t chip_id;
    bool initialized;
} Bmp180;

Bmp180Status Bmp180_ReadChipId(const Bmp180Bus *bus, uint8_t *chip_id);
Bmp180Status Bmp180_Init(Bmp180 *device, const Bmp180Bus *bus);
Bmp180Status Bmp180_ReadMeasurement(
    Bmp180 *device,
    int32_t *temperature_x10,
    int32_t *pressure_pa
);

Bmp180Status Bmp180_Compensate(
    const Bmp180Calibration *calibration,
    int32_t uncompensated_temperature,
    int32_t uncompensated_pressure,
    uint8_t oversampling,
    int32_t *temperature_x10,
    int32_t *pressure_pa
);

const char *Bmp180_StatusString(Bmp180Status status);

#endif
