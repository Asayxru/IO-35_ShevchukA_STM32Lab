#include "bmp180.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    int result;
    uint8_t chip_id;
    uint8_t observed_address;
    uint8_t observed_register;
    size_t observed_length;
} FakeBus;

static int fake_write(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length
)
{
    FakeBus *fake = context;
    (void)data;
    fake->observed_address = address;
    fake->observed_register = reg;
    fake->observed_length = length;
    return fake->result;
}

static int fake_read(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
)
{
    FakeBus *fake = context;
    fake->observed_address = address;
    fake->observed_register = reg;
    fake->observed_length = length;
    if ((fake->result == BMP180_BUS_OK) && (length > 0u))
    {
        (void)memset(data, 0, length);
        data[0] = fake->chip_id;
    }
    return fake->result;
}

static void fake_delay(uint32_t delay_ms)
{
    (void)delay_ms;
}

static Bmp180Bus make_bus(FakeBus *fake)
{
    Bmp180Bus bus = {
        .context = fake,
        .write = fake_write,
        .read = fake_read,
        .delay_ms = fake_delay
    };
    return bus;
}

int main(void)
{
    FakeBus fake = { .result = BMP180_BUS_OK, .chip_id = 0x54u };
    Bmp180Bus bus = make_bus(&fake);
    Bmp180 device;
    uint8_t id = 0u;

    assert(Bmp180_Init(&device, &bus) == BMP180_STATUS_BAD_ID);

    fake.result = BMP180_BUS_NACK;
    assert(Bmp180_ReadChipId(&bus, &id) == BMP180_STATUS_NACK);

    fake.result = BMP180_BUS_TIMEOUT;
    assert(Bmp180_ReadChipId(&bus, &id) == BMP180_STATUS_TIMEOUT);

    fake.result = BMP180_BUS_OK;
    fake.chip_id = BMP180_EXPECTED_CHIP_ID;
    assert(Bmp180_ReadChipId(&bus, &id) == BMP180_STATUS_OK);
    assert(id == 0x55u);
    assert(fake.observed_address == 0x77u);
    assert(fake.observed_register == 0xd0u);
    assert(fake.observed_length == 1u);

    puts("test_bmp180_errors: PASS");
    return 0;
}
