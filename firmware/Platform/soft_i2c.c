#include "soft_i2c.h"

#define SOFT_I2C_DELAY_CYCLES 500u

static void soft_i2c_delay(void)
{
    volatile uint32_t i;

    for (i = 0; i < SOFT_I2C_DELAY_CYCLES; ++i)
    {
        __NOP();
    }
}

static void gpio_drive_low(const SoftI2C *bus, uint16_t pin)
{
    GPIO_InitTypeDef config = {0};

    HAL_GPIO_WritePin(bus->port, pin, GPIO_PIN_RESET);

    config.Pin = pin;
    config.Mode = GPIO_MODE_OUTPUT_OD;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(bus->port, &config);

    HAL_GPIO_WritePin(bus->port, pin, GPIO_PIN_RESET);
}

static void gpio_release(const SoftI2C *bus, uint16_t pin)
{
    GPIO_InitTypeDef config = {0};

    config.Pin = pin;
    config.Mode = GPIO_MODE_INPUT;
    config.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(bus->port, &config);
}

static void sda_low(const SoftI2C *bus)
{
    gpio_drive_low(bus, bus->sda_pin);
    soft_i2c_delay();
}

static void sda_release(const SoftI2C *bus)
{
    gpio_release(bus, bus->sda_pin);
    soft_i2c_delay();
}

static void scl_low(const SoftI2C *bus)
{
    gpio_drive_low(bus, bus->scl_pin);
    soft_i2c_delay();
}

static SoftI2C_Status scl_release(const SoftI2C *bus)
{
    uint32_t remaining = bus->timeout_cycles;

    gpio_release(bus, bus->scl_pin);
    soft_i2c_delay();

    while (HAL_GPIO_ReadPin(bus->port, bus->scl_pin) == GPIO_PIN_RESET)
    {
        if (remaining == 0u)
        {
            return SOFT_I2C_TIMEOUT;
        }
        --remaining;
    }

    return SOFT_I2C_OK;
}

static uint8_t read_sda(const SoftI2C *bus)
{
    return (HAL_GPIO_ReadPin(bus->port, bus->sda_pin) == GPIO_PIN_SET) ? 1u : 0u;
}

void SoftI2C_Init(SoftI2C *bus)
{
    if (bus == NULL)
    {
        return;
    }

    gpio_release(bus, bus->scl_pin);
    gpio_release(bus, bus->sda_pin);
}

static SoftI2C_Status start_condition(const SoftI2C *bus)
{
    SoftI2C_Status status;

    sda_release(bus);
    status = scl_release(bus);
    if (status != SOFT_I2C_OK)
    {
        return status;
    }

    sda_low(bus);
    scl_low(bus);
    return SOFT_I2C_OK;
}

static SoftI2C_Status stop_condition(const SoftI2C *bus)
{
    SoftI2C_Status status;

    sda_low(bus);
    scl_low(bus);
    status = scl_release(bus);
    sda_release(bus);
    return status;
}

static SoftI2C_Status write_byte(const SoftI2C *bus, uint8_t value)
{
    uint8_t bit;
    SoftI2C_Status status;

    for (bit = 0u; bit < 8u; ++bit)
    {
        scl_low(bus);
        if ((value & 0x80u) != 0u)
        {
            sda_release(bus);
        }
        else
        {
            sda_low(bus);
        }

        status = scl_release(bus);
        if (status != SOFT_I2C_OK)
        {
            return status;
        }
        scl_low(bus);
        value <<= 1;
    }

    sda_release(bus);
    status = scl_release(bus);
    if (status != SOFT_I2C_OK)
    {
        return status;
    }

    status = (read_sda(bus) == 0u) ? SOFT_I2C_OK : SOFT_I2C_NACK;
    scl_low(bus);
    return status;
}

static SoftI2C_Status read_byte(const SoftI2C *bus, uint8_t send_ack, uint8_t *value)
{
    uint8_t bit;
    uint8_t result = 0u;
    SoftI2C_Status status;

    if (value == NULL)
    {
        return SOFT_I2C_INVALID_ARGUMENT;
    }

    sda_release(bus);
    for (bit = 0u; bit < 8u; ++bit)
    {
        result <<= 1;
        scl_low(bus);
        status = scl_release(bus);
        if (status != SOFT_I2C_OK)
        {
            return status;
        }
        if (read_sda(bus) != 0u)
        {
            result |= 1u;
        }
        scl_low(bus);
    }

    if (send_ack != 0u)
    {
        sda_low(bus);
    }
    else
    {
        sda_release(bus);
    }

    status = scl_release(bus);
    if (status != SOFT_I2C_OK)
    {
        return status;
    }
    scl_low(bus);
    sda_release(bus);

    *value = result;
    return SOFT_I2C_OK;
}

static SoftI2C_Status write_checked(const SoftI2C *bus, uint8_t value)
{
    return write_byte(bus, value);
}

SoftI2C_Status SoftI2C_WriteRegister(
    SoftI2C *bus,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
)
{
    size_t index;
    SoftI2C_Status status;
    SoftI2C_Status stop_status;

    if ((bus == NULL) || ((data == NULL) && (length != 0u)) || (address > 0x7fu))
    {
        return SOFT_I2C_INVALID_ARGUMENT;
    }

    status = start_condition(bus);
    if (status == SOFT_I2C_OK)
    {
        status = write_checked(bus, (uint8_t)(address << 1));
    }
    if (status == SOFT_I2C_OK)
    {
        status = write_checked(bus, reg);
    }

    for (index = 0u; (index < length) && (status == SOFT_I2C_OK); ++index)
    {
        status = write_checked(bus, data[index]);
    }

    stop_status = stop_condition(bus);
    if (status == SOFT_I2C_OK)
    {
        status = stop_status;
    }
    return status;
}

SoftI2C_Status SoftI2C_ReadRegisters(
    SoftI2C *bus,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
)
{
    size_t index;
    SoftI2C_Status status;
    SoftI2C_Status stop_status;

    if ((bus == NULL) || (data == NULL) || (length == 0u) || (address > 0x7fu))
    {
        return SOFT_I2C_INVALID_ARGUMENT;
    }

    status = start_condition(bus);
    if (status == SOFT_I2C_OK)
    {
        status = write_checked(bus, (uint8_t)(address << 1));
    }
    if (status == SOFT_I2C_OK)
    {
        status = write_checked(bus, reg);
    }
    if (status == SOFT_I2C_OK)
    {
        status = start_condition(bus);
    }
    if (status == SOFT_I2C_OK)
    {
        status = write_checked(bus, (uint8_t)((address << 1) | 1u));
    }

    for (index = 0u; (index < length) && (status == SOFT_I2C_OK); ++index)
    {
        status = read_byte(bus, (index + 1u < length) ? 1u : 0u, &data[index]);
    }

    stop_status = stop_condition(bus);
    if (status == SOFT_I2C_OK)
    {
        status = stop_status;
    }
    return status;
}
