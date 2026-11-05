#include "bmp180.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    const Bmp180Calibration calibration = {
        .ac1 = 408,
        .ac2 = -72,
        .ac3 = -14383,
        .ac4 = 32741u,
        .ac5 = 32757u,
        .ac6 = 23153u,
        .b1 = 6190,
        .b2 = 4,
        .mb = -32768,
        .mc = -8711,
        .md = 2868
    };
    int32_t temperature_x10 = 0;
    int32_t pressure_pa = 0;
    Bmp180Status status;

    status = Bmp180_Compensate(
        &calibration,
        27898,
        23843,
        0u,
        &temperature_x10,
        &pressure_pa
    );

    assert(status == BMP180_STATUS_OK);
    assert(temperature_x10 == 150);
    assert(pressure_pa == 69964);

    /* Байти з реального запису Logic Analyzer у Wokwi. */
    status = Bmp180_Compensate(
        &calibration,
        0x8689,
        0x40c2,
        0u,
        &temperature_x10,
        &pressure_pa
    );
    assert(status == BMP180_STATUS_OK);
    assert(temperature_x10 == 627);
    assert(pressure_pa == 53500);

    puts("test_bmp180_compensation: PASS");
    return 0;
}
