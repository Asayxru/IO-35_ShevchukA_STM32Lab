#include "lab_app.h"

#include "adc.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>


/* =========================================================
 * BMP180
 * ========================================================= */

#define BMP180_ADDR            0x77u

#define BMP180_REG_ID          0xD0u
#define BMP180_CHIP_ID         0x55u

#define BMP180_REG_CALIB       0xAAu
#define BMP180_REG_CONTROL     0xF4u
#define BMP180_REG_DATA        0xF6u

#define BMP180_CMD_TEMP        0x2Eu
#define BMP180_CMD_PRESSURE    0x34u

#define BMP180_OSS             0


/* =========================================================
 * SOFTWARE I2C
 *
 * PB6 = SCL
 * PB7 = SDA
 * ========================================================= */

#define SOFT_I2C_PORT          GPIOB
#define SOFT_I2C_SCL_PIN       GPIO_PIN_6
#define SOFT_I2C_SDA_PIN       GPIO_PIN_7


typedef struct
{
    int16_t AC1;
    int16_t AC2;
    int16_t AC3;

    uint16_t AC4;
    uint16_t AC5;
    uint16_t AC6;

    int16_t B1;
    int16_t B2;

    int16_t MB;
    int16_t MC;
    int16_t MD;

} BMP180_Calib_t;


static BMP180_Calib_t bmp_calib;
static uint8_t bmp_ok = 0;


/* =========================================================
 * UART
 * ========================================================= */

static void uart_write(const char *text)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        HAL_MAX_DELAY
    );
}


int _write(int fd, char *ptr, int len)
{
    (void)fd;

    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)ptr,
        (uint16_t)len,
        HAL_MAX_DELAY
    );

    return len;
}


/* =========================================================
 * SOFTWARE I2C DELAY
 * ========================================================= */

static void soft_i2c_delay(void)
{
    volatile uint32_t i;

    /*
     * Навмисно повільний I2C для стабільної
     * роботи в Wokwi.
     */
    for (i = 0; i < 500; i++)
    {
        __NOP();
    }
}


/* =========================================================
 * SOFTWARE I2C GPIO HELPERS
 *
 * LOW:
 *   GPIO output open-drain + RESET
 *
 * RELEASE:
 *   GPIO input + pull-up
 * ========================================================= */

static void gpio_drive_low(uint16_t pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(
        SOFT_I2C_PORT,
        pin,
        GPIO_PIN_RESET
    );

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        SOFT_I2C_PORT,
        &GPIO_InitStruct
    );

    HAL_GPIO_WritePin(
        SOFT_I2C_PORT,
        pin,
        GPIO_PIN_RESET
    );
}


static void gpio_release(uint16_t pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(
        SOFT_I2C_PORT,
        &GPIO_InitStruct
    );
}


/* =========================================================
 * SDA / SCL CONTROL
 * ========================================================= */

static void soft_i2c_sda_low(void)
{
    gpio_drive_low(SOFT_I2C_SDA_PIN);
    soft_i2c_delay();
}


static void soft_i2c_sda_release(void)
{
    gpio_release(SOFT_I2C_SDA_PIN);
    soft_i2c_delay();
}


static void soft_i2c_scl_low(void)
{
    gpio_drive_low(SOFT_I2C_SCL_PIN);
    soft_i2c_delay();
}


static void soft_i2c_scl_release(void)
{
    gpio_release(SOFT_I2C_SCL_PIN);
    soft_i2c_delay();
}


static uint8_t soft_i2c_read_sda(void)
{
    return (
        HAL_GPIO_ReadPin(
            SOFT_I2C_PORT,
            SOFT_I2C_SDA_PIN
        ) == GPIO_PIN_SET
    );
}


/* =========================================================
 * SOFTWARE I2C INIT
 * ========================================================= */

static void soft_i2c_init(void)
{
    /*
     * Hardware I2C1 більше не повинен
     * керувати PB6/PB7.
     */
    HAL_I2C_DeInit(&hi2c1);

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*
     * Idle:
     * SCL = HIGH
     * SDA = HIGH
     */
    gpio_release(SOFT_I2C_SCL_PIN);
    gpio_release(SOFT_I2C_SDA_PIN);

    HAL_Delay(10);
}


/* =========================================================
 * SOFTWARE I2C START
 * ========================================================= */

static void soft_i2c_start(void)
{
    /*
     * SDA HIGH
     * SCL HIGH
     */
    soft_i2c_sda_release();
    soft_i2c_scl_release();

    /*
     * START condition:
     * SDA HIGH -> LOW while SCL HIGH
     */
    soft_i2c_sda_low();

    /*
     * Prepare for data transfer.
     */
    soft_i2c_scl_low();
}


/* =========================================================
 * SOFTWARE I2C STOP
 * ========================================================= */

static void soft_i2c_stop(void)
{
    soft_i2c_sda_low();
    soft_i2c_scl_low();

    /*
     * SCL HIGH
     */
    soft_i2c_scl_release();

    /*
     * STOP condition:
     * SDA LOW -> HIGH while SCL HIGH
     */
    soft_i2c_sda_release();
}


/* =========================================================
 * SOFTWARE I2C WRITE BYTE
 *
 * return:
 * 1 = ACK
 * 0 = NACK
 * ========================================================= */

static uint8_t soft_i2c_write_byte(uint8_t value)
{
    uint8_t bit;
    uint8_t ack;

    for (bit = 0; bit < 8; bit++)
    {
        soft_i2c_scl_low();

        if (value & 0x80u)
        {
            /*
             * Logic 1:
             * release SDA.
             */
            soft_i2c_sda_release();
        }
        else
        {
            /*
             * Logic 0:
             * drive SDA low.
             */
            soft_i2c_sda_low();
        }

        /*
         * Slave samples SDA here.
         */
        soft_i2c_scl_release();

        /*
         * Finish this clock pulse.
         */
        soft_i2c_scl_low();

        value <<= 1;
    }

    /*
     * ACK clock.
     * Master releases SDA.
     */
    soft_i2c_sda_release();

    soft_i2c_scl_release();

    /*
     * ACK = SDA LOW.
     */
    ack =
        (soft_i2c_read_sda() == 0u);

    soft_i2c_scl_low();

    return ack;
}


/* =========================================================
 * SOFTWARE I2C READ BYTE
 *
 * send_ack:
 * 1 -> send ACK
 * 0 -> send NACK
 * ========================================================= */

static uint8_t soft_i2c_read_byte(uint8_t send_ack)
{
    uint8_t value = 0;
    uint8_t bit;

    /*
     * Release SDA so slave can drive it.
     */
    soft_i2c_sda_release();

    for (bit = 0; bit < 8; bit++)
    {
        value <<= 1;

        soft_i2c_scl_low();

        /*
         * Slave puts next bit on SDA.
         */
        soft_i2c_scl_release();

        /*
         * Read while SCL is HIGH.
         */
        if (soft_i2c_read_sda())
        {
            value |= 1u;
        }

        soft_i2c_scl_low();
    }

    /*
     * Master ACK/NACK.
     */
    if (send_ack)
    {
        soft_i2c_sda_low();
    }
    else
    {
        soft_i2c_sda_release();
    }

    soft_i2c_scl_release();
    soft_i2c_scl_low();

    /*
     * Release bus.
     */
    soft_i2c_sda_release();

    return value;
}


/* =========================================================
 * BMP180 REGISTER WRITE
 * ========================================================= */

static uint8_t bmp_write(
    uint8_t reg,
    uint8_t value
)
{
    uint8_t ok = 1;

    soft_i2c_start();

    /*
     * Address + WRITE:
     * 0x77 << 1 = 0xEE
     */
    if (!soft_i2c_write_byte(
        (uint8_t)(BMP180_ADDR << 1)
    ))
    {
        printf(
            "[I2C] BMP180 WRITE address NACK\r\n"
        );

        ok = 0;
    }

    if (
        ok &&
        !soft_i2c_write_byte(reg)
    )
    {
        printf(
            "[I2C] BMP180 register NACK reg=0x%02X\r\n",
            reg
        );

        ok = 0;
    }

    if (
        ok &&
        !soft_i2c_write_byte(value)
    )
    {
        printf(
            "[I2C] BMP180 data NACK value=0x%02X\r\n",
            value
        );

        ok = 0;
    }

    soft_i2c_stop();

    return ok;
}


/* =========================================================
 * BMP180 REGISTER READ
 * ========================================================= */

static uint8_t bmp_read(
    uint8_t reg,
    uint8_t *data,
    uint16_t length
)
{
    uint16_t i;

    soft_i2c_start();

    /*
     * Address + WRITE.
     */
    if (!soft_i2c_write_byte(
        (uint8_t)(BMP180_ADDR << 1)
    ))
    {
        printf(
            "[I2C] address WRITE NACK\r\n"
        );

        soft_i2c_stop();

        return 0;
    }

    /*
     * Register address.
     */
    if (!soft_i2c_write_byte(reg))
    {
        printf(
            "[I2C] register NACK reg=0x%02X\r\n",
            reg
        );

        soft_i2c_stop();

        return 0;
    }

    /*
     * Repeated START.
     */
    soft_i2c_start();

    /*
     * Address + READ.
     */
    if (!soft_i2c_write_byte(
        (uint8_t)((BMP180_ADDR << 1) | 1u)
    ))
    {
        printf(
            "[I2C] address READ NACK\r\n"
        );

        soft_i2c_stop();

        return 0;
    }

    /*
     * Read requested bytes.
     */
    for (i = 0; i < length; i++)
    {
        data[i] =
            soft_i2c_read_byte(
                i < (length - 1)
            );
    }

    soft_i2c_stop();

    return 1;
}


/* =========================================================
 * BYTE HELPERS
 * ========================================================= */

static int16_t read_s16_be(
    const uint8_t *data
)
{
    return (int16_t)(
        ((uint16_t)data[0] << 8) |
        data[1]
    );
}


static uint16_t read_u16_be(
    const uint8_t *data
)
{
    return (uint16_t)(
        ((uint16_t)data[0] << 8) |
        data[1]
    );
}


/* =========================================================
 * BMP180 CALIBRATION
 * ========================================================= */

static uint8_t bmp_read_calibration(void)
{
    uint8_t raw[22];

    if (!bmp_read(
        BMP180_REG_CALIB,
        raw,
        sizeof(raw)
    ))
    {
        printf(
            "[BMP] calibration read failed\r\n"
        );

        return 0;
    }

    bmp_calib.AC1 =
        read_s16_be(&raw[0]);

    bmp_calib.AC2 =
        read_s16_be(&raw[2]);

    bmp_calib.AC3 =
        read_s16_be(&raw[4]);

    bmp_calib.AC4 =
        read_u16_be(&raw[6]);

    bmp_calib.AC5 =
        read_u16_be(&raw[8]);

    bmp_calib.AC6 =
        read_u16_be(&raw[10]);

    bmp_calib.B1 =
        read_s16_be(&raw[12]);

    bmp_calib.B2 =
        read_s16_be(&raw[14]);

    bmp_calib.MB =
        read_s16_be(&raw[16]);

    bmp_calib.MC =
        read_s16_be(&raw[18]);

    bmp_calib.MD =
        read_s16_be(&raw[20]);

    return 1;
}


/* =========================================================
 * BMP180 RAW TEMPERATURE
 * ========================================================= */

static uint8_t bmp_read_ut(
    int32_t *ut
)
{
    uint8_t raw[2];

    /*
     * Start temperature conversion:
     * 0xF4 <- 0x2E
     */
    if (!bmp_write(
        BMP180_REG_CONTROL,
        BMP180_CMD_TEMP
    ))
    {
        printf(
            "[BMP] temp command write failed\r\n"
        );

        return 0;
    }

    HAL_Delay(5);

    /*
     * Read UT:
     * 0xF6 + 0xF7
     */
    if (!bmp_read(
        BMP180_REG_DATA,
        raw,
        2
    ))
    {
        printf(
            "[BMP] temp data read failed\r\n"
        );

        return 0;
    }

    *ut =
        ((int32_t)raw[0] << 8) |
        raw[1];

    return 1;
}


/* =========================================================
 * BMP180 RAW PRESSURE
 * ========================================================= */

static uint8_t bmp_read_up(
    int32_t *up
)
{
    uint8_t raw[3];

    uint8_t command =
        BMP180_CMD_PRESSURE |
        (BMP180_OSS << 6);

    if (!bmp_write(
        BMP180_REG_CONTROL,
        command
    ))
    {
        printf(
            "[BMP] pressure command write failed\r\n"
        );

        return 0;
    }

    HAL_Delay(8);

    if (!bmp_read(
        BMP180_REG_DATA,
        raw,
        3
    ))
    {
        printf(
            "[BMP] pressure data read failed\r\n"
        );

        return 0;
    }

    *up =
        (
            (
                ((uint32_t)raw[0] << 16) |
                ((uint32_t)raw[1] << 8) |
                raw[2]
            )
            >>
            (8 - BMP180_OSS)
        );

    return 1;
}


/* =========================================================
 * BMP180 TEMPERATURE + PRESSURE
 * ========================================================= */

static uint8_t bmp_read_temperature_pressure(
    int32_t *temperature_x10,
    int32_t *pressure_pa
)
{
    int32_t UT;
    int32_t UP;

    int32_t X1;
    int32_t X2;
    int32_t X3;

    int32_t B3;
    int32_t B5;
    int32_t B6;

    uint32_t B4;
    uint32_t B7;

    int32_t p;


    /* -----------------------------------------------------
     * Temperature
     * ----------------------------------------------------- */

    if (!bmp_read_ut(&UT))
    {
        return 0;
    }

    printf(
        "[BMP] UT=%ld\r\n",
        (long)UT
    );

    X1 =
        (
            (UT - (int32_t)bmp_calib.AC6)
            *
            (int32_t)bmp_calib.AC5
        )
        >> 15;

    if (
        (X1 + bmp_calib.MD)
        == 0
    )
    {
        printf(
            "[BMP] temperature calculation error\r\n"
        );

        return 0;
    }

    X2 =
        (
            ((int32_t)bmp_calib.MC << 11)
            /
            (X1 + bmp_calib.MD)
        );

    B5 =
        X1 + X2;

    *temperature_x10 =
        (B5 + 8) >> 4;


    /* -----------------------------------------------------
     * Pressure
     * ----------------------------------------------------- */

    if (!bmp_read_up(&UP))
    {
        return 0;
    }

    printf(
        "[BMP] UP=%ld\r\n",
        (long)UP
    );

    B6 =
        B5 - 4000;

    X1 =
        (
            (int32_t)bmp_calib.B2
            *
            ((B6 * B6) >> 12)
        )
        >> 11;

    X2 =
        (
            (int32_t)bmp_calib.AC2
            *
            B6
        )
        >> 11;

    X3 =
        X1 + X2;

    B3 =
        (
            (
                (
                    ((int32_t)bmp_calib.AC1 * 4 + X3)
                    << BMP180_OSS
                )
                + 2
            )
            >> 2
        );

    X1 =
        (
            (int32_t)bmp_calib.AC3
            *
            B6
        )
        >> 13;

    X2 =
        (
            (int32_t)bmp_calib.B1
            *
            ((B6 * B6) >> 12)
        )
        >> 16;

    X3 =
        (X1 + X2 + 2)
        >> 2;

    B4 =
        (
            (uint32_t)bmp_calib.AC4
            *
            (uint32_t)(X3 + 32768)
        )
        >> 15;

    if (B4 == 0)
    {
        printf(
            "[BMP] pressure calculation error\r\n"
        );

        return 0;
    }

    B7 =
        (
            (uint32_t)(UP - B3)
            *
            (uint32_t)(50000 >> BMP180_OSS)
        );

    if (
        B7 < 0x80000000u
    )
    {
        p =
            (int32_t)(
                (B7 << 1)
                /
                B4
            );
    }
    else
    {
        p =
            (int32_t)(
                (B7 / B4)
                << 1
            );
    }

    X1 =
        (p >> 8)
        *
        (p >> 8);

    X1 =
        (X1 * 3038)
        >> 16;

    X2 =
        (-7357 * p)
        >> 16;

    p =
        p +
        (
            (X1 + X2 + 3791)
            >> 4
        );

    *pressure_pa =
        p;

    return 1;
}


/* =========================================================
 * BMP180 INIT
 * ========================================================= */

static uint8_t bmp_init(void)
{
    uint8_t id = 0;

    printf(
        "[I2C] probing BMP180 address 0x77...\r\n"
    );

    if (!bmp_read(
        BMP180_REG_ID,
        &id,
        1
    ))
    {
        printf(
            "[I2C] BMP180 ID read failed\r\n"
        );

        return 0;
    }

    printf(
        "[I2C] BMP180 chip ID = 0x%02X\r\n",
        id
    );

    if (
        id != BMP180_CHIP_ID
    )
    {
        printf(
            "[I2C] Unexpected BMP180 ID\r\n"
        );

        return 0;
    }

    if (!bmp_read_calibration())
    {
        return 0;
    }

    printf(
        "[I2C] BMP180 calibration loaded\r\n"
    );

    printf(
        "[I2C] BMP180 detected\r\n"
    );

    return 1;
}


/* =========================================================
 * ADC
 * ========================================================= */

static uint32_t read_adc(void)
{
    uint32_t value = 0;

    if (
        HAL_ADC_Start(&hadc1)
        == HAL_OK
    )
    {
        if (
            HAL_ADC_PollForConversion(
                &hadc1,
                100
            )
            == HAL_OK
        )
        {
            value =
                HAL_ADC_GetValue(
                    &hadc1
                );
        }

        HAL_ADC_Stop(
            &hadc1
        );
    }

    return value;
}


/* =========================================================
 * APPLICATION INIT
 * ========================================================= */

void LAB_AppInit(void)
{
    uart_write(
        "\r\n"
        "=== STM32 LAB1 START ===\r\n"
    );

    printf(
        "HAL tick: %lu ms\r\n",
        HAL_GetTick()
    );

    /*
     * BMP180 power-up time.
     */
    HAL_Delay(100);

    /*
     * Hardware I2C -> Software I2C.
     */
    soft_i2c_init();

    printf(
        "[I2C] Software I2C initialized "
        "PB6=SCL PB7=SDA\r\n"
    );

    bmp_ok =
        bmp_init();

    printf(
        "BMP180: %s\r\n",
        bmp_ok
            ? "OK"
            : "ERROR"
    );

    printf(
        "Controls: change potentiometer, "
        "button, temperature and pressure\r\n\r\n"
    );
}


/* =========================================================
 * APPLICATION LOOP
 * ========================================================= */

void LAB_AppLoop(void)
{
    static uint32_t last_tick = 0;

    uint32_t adc;

    GPIO_PinState button;

    int32_t temperature_x10 = 0;
    int32_t pressure_pa = 0;

    uint8_t measurement_ok = 0;


    if (
        HAL_GetTick() - last_tick
        <
        1000
    )
    {
        return;
    }

    last_tick =
        HAL_GetTick();


    adc =
        read_adc();


    button =
        HAL_GPIO_ReadPin(
            GPIOB,
            GPIO_PIN_0
        );


    if (bmp_ok)
    {
        measurement_ok =
            bmp_read_temperature_pressure(
                &temperature_x10,
                &pressure_pa
            );
    }


    if (
        bmp_ok &&
        measurement_ok
    )
    {
        int32_t temp_abs;
        char temp_sign = '+';


        if (
            temperature_x10 < 0
        )
        {
            temp_sign = '-';

            temp_abs =
                -temperature_x10;
        }
        else
        {
            temp_abs =
                temperature_x10;
        }


        printf(
            "tick=%lu "
            "adc=%lu "
            "button=%s "
            "temp=%c%ld.%ldC "
            "pressure=%ldPa\r\n",

            HAL_GetTick(),

            adc,

            button == GPIO_PIN_RESET
                ? "PRESSED"
                : "released",

            temp_sign,

            (long)(
                temp_abs / 10
            ),

            (long)(
                temp_abs % 10
            ),

            (long)pressure_pa
        );
    }
    else
    {
        printf(
            "tick=%lu "
            "adc=%lu "
            "button=%s "
            "BMP180=ERROR\r\n",

            HAL_GetTick(),

            adc,

            button == GPIO_PIN_RESET
                ? "PRESSED"
                : "released"
        );
    }
}