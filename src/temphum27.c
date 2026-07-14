/**
 * @file temphum27.c
 * @brief Temp&Hum 27 Click reset, CRC validation and fixed-point conversion.
 */

#include "temphum27.h"
#include "app_config.h"
#include "iic0_bus.h"
#include "ra4m2_baremetal.h"

#define TEMPHUM27_EN_MASK (1U << 5) /* P105 */
#define TEMPHUM27_ALR_MASK (1U << 1) /* P101 */

static void temphum27_delay_ms(uint32_t milliseconds)
{
    while (milliseconds-- != 0U)
    {
        Delay_1ms();
    }
}

/* -------------------------------------------------------------------------- */
/* Temp&Hum 27 Click access                                                    */
/* -------------------------------------------------------------------------- */

static void temphum27_configure_pins(void)
{
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;

    /* P105: active-high sensor enable. P101: alert input (currently unused). */
    R_PFS->PORT[1].PIN[5].PmnPFS = PFS_PDR_OUTPUT;
    R_PFS->PORT[1].PIN[1].PmnPFS = 0U;

    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;
}


static void temphum27_enable(void)
{
    R_PORT1->PODR |= TEMPHUM27_EN_MASK;
}


static void temphum27_disable(void)
{
    R_PORT1->PODR &= (uint16_t)~TEMPHUM27_EN_MASK;
}


static void temphum27_reset(void)
{
    /* Match the reset sequence from the supplied Click driver. */
    temphum27_disable();
    temphum27_delay_ms(1000U);
    temphum27_enable();
    temphum27_delay_ms(1000U);
}


static uint8_t temphum27_calculate_crc(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFFU;
    uint8_t byte_index;
    uint8_t bit_index;

    for (byte_index = 0U; byte_index < length; byte_index++)
    {
        crc ^= data[byte_index];

        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x1DU);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}


void temphum27_init(void)
{
    temphum27_configure_pins();
    temphum27_reset();
}


int temphum27_read_data(temphum27_data_t *environment)
{
    uint8_t command = TEMPHUM27_CMD_HOLD_TEMP_HUM;
    uint8_t raw[5];
    uint16_t raw_humidity;
    uint16_t raw_temperature;
    uint32_t humidity_tenths;
    int32_t temperature_tenths;
    int result;

    if (environment == 0)
    {
        return RTC14_ERROR_ARGUMENT;
    }

    environment->valid = 0U;
    Delay_ms(10);

    result = iic0_bus_write(TEMPHUM27_I2C_ADDRESS,
                        &command, 1U, 1U);
    if (result != RTC14_OK)
    {
        return result;
    }

    Delay_ms(10);

    result = iic0_bus_read_after_restart(TEMPHUM27_I2C_ADDRESS,
                                     raw, sizeof(raw));
    if (result != RTC14_OK)
    {
        return result;
    }

    Delay_ms(10);

    if (temphum27_calculate_crc(raw, 4U) != raw[4])
    {
        return TEMPHUM27_ERROR_CRC;
    }

    /*
     * The Click driver returns humidity first, then temperature:
     * RH = raw / 0x3FFF * 100
     * T  = raw / 0x3FFF * 165 - 40
     * Keep one decimal place without requiring floating-point formatting.
     */
    raw_humidity = (uint16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    raw_temperature = (uint16_t)(((uint16_t)raw[2] << 8) | raw[3]);

    humidity_tenths =
        (uint32_t)((((uint64_t)raw_humidity * TEMPHUM27_HUM_SPAN_TENTHS) +
                    (TEMPHUM27_DATA_RESOLUTION / 2UL)) /
                   TEMPHUM27_DATA_RESOLUTION);

    temperature_tenths =
        (int32_t)((((uint64_t)raw_temperature *
                    (uint32_t)TEMPHUM27_TEMP_SPAN_TENTHS) +
                   (TEMPHUM27_DATA_RESOLUTION / 2UL)) /
                  TEMPHUM27_DATA_RESOLUTION) -
        TEMPHUM27_TEMP_OFFSET_TENTHS;

    if (humidity_tenths > 1000UL)
    {
        humidity_tenths = 1000UL;
    }

    environment->temperature_tenths_c = (int16_t)temperature_tenths;
    environment->humidity_tenths_percent = (uint16_t)humidity_tenths;
    environment->valid = 1U;

    return RTC14_OK;
}


void temphum27_print_data(const temphum27_data_t *environment)
{
    int32_t temperature = environment->temperature_tenths_c;
    const char *sign = "";
    uint32_t magnitude;

    if (temperature < 0)
    {
        sign = "-";
        magnitude = (uint32_t)(-temperature);
    }
    else
    {
        magnitude = (uint32_t)temperature;
    }

    printf_me("TempHum27: %s%lu.%lu C, %lu.%lu %%RH\r\n",
              sign,
              (unsigned long)(magnitude / 10UL),
              (unsigned long)(magnitude % 10UL),
              (unsigned long)(environment->humidity_tenths_percent / 10U),
              (unsigned long)(environment->humidity_tenths_percent % 10U));
}
