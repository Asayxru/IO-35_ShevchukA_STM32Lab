#include "bmp180.h"

#include <string.h>

#define BMP180_REG_CALIBRATION 0xaau
#define BMP180_REG_ID 0xd0u
#define BMP180_REG_CONTROL 0xf4u
#define BMP180_REG_DATA 0xf6u

#define BMP180_CMD_TEMPERATURE 0x2eu
#define BMP180_CMD_PRESSURE 0x34u
#define BMP180_OSS 0u

static Bmp180Status bus_result(int result)
{
    if (result == BMP180_BUS_OK)
    {
        return BMP180_STATUS_OK;
    }
    if (result == BMP180_BUS_NACK)
    {
        return BMP180_STATUS_NACK;
    }
    if (result == BMP180_BUS_TIMEOUT)
    {
        return BMP180_STATUS_TIMEOUT;
    }
    return BMP180_STATUS_BUS_ERROR;
}

static int16_t read_s16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static uint16_t read_u16_be(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static Bmp180Status read_registers(
    const Bmp180Bus *bus,
    uint8_t reg,
    uint8_t *data,
    size_t length
)
{
    if ((bus == NULL) || (bus->read == NULL) || (data == NULL) || (length == 0u))
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }

    return bus_result(
        bus->read(bus->context, BMP180_I2C_ADDRESS, reg, data, length)
    );
}

static Bmp180Status write_register(
    const Bmp180Bus *bus,
    uint8_t reg,
    uint8_t value
)
{
    if ((bus == NULL) || (bus->write == NULL))
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }

    return bus_result(
        bus->write(bus->context, BMP180_I2C_ADDRESS, reg, &value, 1u)
    );
}

Bmp180Status Bmp180_ReadChipId(const Bmp180Bus *bus, uint8_t *chip_id)
{
    if (chip_id == NULL)
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }
    return read_registers(bus, BMP180_REG_ID, chip_id, 1u);
}

static bool calibration_is_valid(const uint8_t *raw, size_t length)
{
    size_t index;
    bool all_zero = true;
    bool all_ff = true;

    for (index = 0u; index < length; ++index)
    {
        all_zero = all_zero && (raw[index] == 0u);
        all_ff = all_ff && (raw[index] == 0xffu);
    }
    return !all_zero && !all_ff;
}

static Bmp180Status read_calibration(Bmp180 *device)
{
    uint8_t raw[22];
    Bmp180Status status;

    status = read_registers(&device->bus, BMP180_REG_CALIBRATION, raw, sizeof(raw));
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }
    if (!calibration_is_valid(raw, sizeof(raw)))
    {
        return BMP180_STATUS_CALIBRATION;
    }

    device->calibration.ac1 = read_s16_be(&raw[0]);
    device->calibration.ac2 = read_s16_be(&raw[2]);
    device->calibration.ac3 = read_s16_be(&raw[4]);
    device->calibration.ac4 = read_u16_be(&raw[6]);
    device->calibration.ac5 = read_u16_be(&raw[8]);
    device->calibration.ac6 = read_u16_be(&raw[10]);
    device->calibration.b1 = read_s16_be(&raw[12]);
    device->calibration.b2 = read_s16_be(&raw[14]);
    device->calibration.mb = read_s16_be(&raw[16]);
    device->calibration.mc = read_s16_be(&raw[18]);
    device->calibration.md = read_s16_be(&raw[20]);
    return BMP180_STATUS_OK;
}

Bmp180Status Bmp180_Init(Bmp180 *device, const Bmp180Bus *bus)
{
    Bmp180Status status;

    if ((device == NULL) || (bus == NULL) || (bus->read == NULL) ||
        (bus->write == NULL) || (bus->delay_ms == NULL))
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(device, 0, sizeof(*device));
    device->bus = *bus;

    status = Bmp180_ReadChipId(&device->bus, &device->chip_id);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }
    if (device->chip_id != BMP180_EXPECTED_CHIP_ID)
    {
        return BMP180_STATUS_BAD_ID;
    }

    status = read_calibration(device);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    device->initialized = true;
    return BMP180_STATUS_OK;
}

static Bmp180Status read_uncompensated_temperature(Bmp180 *device, int32_t *value)
{
    uint8_t raw[2];
    Bmp180Status status;

    status = write_register(&device->bus, BMP180_REG_CONTROL, BMP180_CMD_TEMPERATURE);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    device->bus.delay_ms(5u);
    status = read_registers(&device->bus, BMP180_REG_DATA, raw, sizeof(raw));
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    *value = ((int32_t)raw[0] << 8) | (int32_t)raw[1];
    return BMP180_STATUS_OK;
}

static Bmp180Status read_uncompensated_pressure(Bmp180 *device, int32_t *value)
{
    uint8_t raw[3];
    uint8_t command = (uint8_t)(BMP180_CMD_PRESSURE | (BMP180_OSS << 6));
    uint32_t combined;
    Bmp180Status status;

    status = write_register(&device->bus, BMP180_REG_CONTROL, command);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    device->bus.delay_ms(8u);
    status = read_registers(&device->bus, BMP180_REG_DATA, raw, sizeof(raw));
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    combined = ((uint32_t)raw[0] << 16) |
               ((uint32_t)raw[1] << 8) |
               (uint32_t)raw[2];
    *value = (int32_t)(combined >> (8u - BMP180_OSS));
    return BMP180_STATUS_OK;
}

Bmp180Status Bmp180_Compensate(
    const Bmp180Calibration *calibration,
    int32_t uncompensated_temperature,
    int32_t uncompensated_pressure,
    uint8_t oversampling,
    int32_t *temperature_x10,
    int32_t *pressure_pa
)
{
    int32_t x1;
    int32_t x2;
    int32_t x3;
    int32_t b3;
    int32_t b5;
    int32_t b6;
    uint32_t b4;
    uint32_t b7;
    int32_t pressure;

    if ((calibration == NULL) || (temperature_x10 == NULL) ||
        (pressure_pa == NULL) || (oversampling > 3u))
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }

    x1 = ((uncompensated_temperature - (int32_t)calibration->ac6) *
          (int32_t)calibration->ac5) >> 15;
    if ((x1 + (int32_t)calibration->md) == 0)
    {
        return BMP180_STATUS_COMPENSATION;
    }

    x2 = ((int32_t)calibration->mc * 2048) /
         (x1 + (int32_t)calibration->md);
    b5 = x1 + x2;
    *temperature_x10 = (b5 + 8) >> 4;

    b6 = b5 - 4000;
    x1 = ((int32_t)calibration->b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = ((int32_t)calibration->ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t)calibration->ac1 * 4) + x3) *
           (int32_t)(1u << oversampling)) + 2) >> 2;

    x1 = ((int32_t)calibration->ac3 * b6) >> 13;
    x2 = ((int32_t)calibration->b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = (x1 + x2 + 2) >> 2;
    b4 = ((uint32_t)calibration->ac4 * (uint32_t)(x3 + 32768)) >> 15;
    if (b4 == 0u)
    {
        return BMP180_STATUS_COMPENSATION;
    }

    b7 = (uint32_t)(uncompensated_pressure - b3) *
         (uint32_t)(50000u >> oversampling);
    if (b7 < 0x80000000u)
    {
        pressure = (int32_t)((b7 * 2u) / b4);
    }
    else
    {
        pressure = (int32_t)((b7 / b4) * 2u);
    }

    x1 = (pressure >> 8) * (pressure >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * pressure) >> 16;
    pressure += (x1 + x2 + 3791) >> 4;
    *pressure_pa = pressure;
    return BMP180_STATUS_OK;
}

Bmp180Status Bmp180_ReadMeasurement(
    Bmp180 *device,
    int32_t *temperature_x10,
    int32_t *pressure_pa
)
{
    int32_t uncompensated_temperature;
    int32_t uncompensated_pressure;
    Bmp180Status status;

    if ((device == NULL) || (temperature_x10 == NULL) || (pressure_pa == NULL))
    {
        return BMP180_STATUS_INVALID_ARGUMENT;
    }
    if (!device->initialized)
    {
        return BMP180_STATUS_NOT_INITIALIZED;
    }

    status = read_uncompensated_temperature(device, &uncompensated_temperature);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }
    status = read_uncompensated_pressure(device, &uncompensated_pressure);
    if (status != BMP180_STATUS_OK)
    {
        return status;
    }

    return Bmp180_Compensate(
        &device->calibration,
        uncompensated_temperature,
        uncompensated_pressure,
        BMP180_OSS,
        temperature_x10,
        pressure_pa
    );
}

const char *Bmp180_StatusString(Bmp180Status status)
{
    switch (status)
    {
        case BMP180_STATUS_OK:
            return "OK";
        case BMP180_STATUS_NACK:
            return "NACK";
        case BMP180_STATUS_TIMEOUT:
            return "TIMEOUT";
        case BMP180_STATUS_BAD_ID:
            return "BAD_ID";
        case BMP180_STATUS_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case BMP180_STATUS_CALIBRATION:
            return "CALIBRATION";
        case BMP180_STATUS_COMPENSATION:
            return "COMPENSATION";
        case BMP180_STATUS_NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        default:
            return "BUS_ERROR";
    }
}
