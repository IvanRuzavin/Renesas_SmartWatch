/**
 * @file rtc14.c
 * @brief RTC 14 Click date/time validation, conversion and I2C access.
 */

#include "rtc14.h"
#include "app_config.h"
#include "iic0_bus.h"
#include "project_platform.h"

#define RTC14_REG_SECONDS               (0x00U)
#define RTC14_HOUR_FORMAT_12            (0x00U)
#define RTC14_HOUR_FORMAT_24            (0x80U)
#define RTC14_HOUR_PM                   (0x20U)
#define RTC14_HOUR_24_MASK              (0x3FU)
#define RTC14_HOUR_12_MASK              (0x1FU)

/* -------------------------------------------------------------------------- */
/* Small utility functions                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Convert one packed BCD byte to decimal.
 * @param[in] value Value to process.
 * @return Calculated or converted value.
 */
static uint8_t rtc14_bcd_to_decimal(uint8_t value)
{
    return (uint8_t)((value & 0x0FU) + ((value >> 4) * 10U));
}


/**
 * @brief Convert one decimal value to packed BCD.
 * @param[in] value Value to process.
 * @return Calculated or converted value.
 */
static uint8_t rtc14_decimal_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}


/**
 * @brief Read a consecutive RTC register range through IIC0.
 * @param[in] first_reg First reg value.
 * @param[out] data Data buffer.
 * @param[in] length Number of bytes in the buffer.
 * @return Calculated or converted value.
 */
static int rtc14_read_registers(uint8_t first_reg, uint8_t *data, uint32_t length)
{
    int result = iic0_bus_write(RTC14_I2C_ADDRESS,
                            &first_reg, 1U, 1U);

    if (result != RTC14_OK)
    {
        return result;
    }

    return iic0_bus_read_after_restart(RTC14_I2C_ADDRESS,
                                   data, length);
}


/**
 * @brief Initialize the module.
 * @return Zero on success; otherwise a negative error code.
 */
int rtc14_init(void)
{
    int result = iic0_bus_init();

    if (result != RTC14_OK)
    {
        return result;
    }

    return RTC14_OK;
}


/**
 * @brief Determine whether a full Gregorian year is a leap year.
 * @param[in] year Full year value.
 * @return Calculated or converted value.
 */
static uint8_t rtc14_is_leap_year(uint16_t year)
{
    if ((year % 400U) == 0U)
    {
        return 1U;
    }
    if ((year % 100U) == 0U)
    {
        return 0U;
    }
    return ((year % 4U) == 0U) ? 1U : 0U;
}


/**
 * @brief Return the valid day count for a month and year.
 * @param[in] year Full year value.
 * @param[in] month Month in the range 1 through 12.
 * @return Calculated or converted value.
 */
static uint8_t rtc14_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if ((month == 0U) || (month > 12U))
    {
        return 0U;
    }

    if ((month == 2U) && (rtc14_is_leap_year(year) != 0U))
    {
        return 29U;
    }

    return days[month - 1U];
}


/**
 * @brief Set datetime 24h.
 * @param[in] year Full year value.
 * @param[in] month Month in the range 1 through 12.
 * @param[in] day Day of month.
 * @param[in] hour Hour value.
 * @param[in] minute Minute value.
 * @param[in] second Second value.
 * @param[in] weekday Weekday number, where zero is Sunday.
 * @return Zero on success; otherwise a negative error code.
 */
int rtc14_set_datetime_24h(uint16_t year, uint8_t month,
                                  uint8_t day, uint8_t hour,
                                  uint8_t minute, uint8_t second,
                                  uint8_t weekday)
{
    uint8_t frame[8];
    uint8_t hour_12;
    uint8_t pm = 0U;
    uint8_t maximum_day;

    maximum_day = rtc14_days_in_month(year, month);
    if ((year < 2000U) || (year > 2099U) ||
        (maximum_day == 0U) || (day == 0U) || (day > maximum_day) ||
        (hour > 23U) || (minute > 59U) || (second > 59U) ||
        (weekday > 6U))
    {
        return RTC14_ERROR_ARGUMENT;
    }

    /* Convert the 24-hour BLE command to the RTC's 12-hour AM/PM format. */
    if (hour == 0U)
    {
        hour_12 = 12U;
    }
    else if (hour < 12U)
    {
        hour_12 = hour;
    }
    else if (hour == 12U)
    {
        hour_12 = 12U;
        pm = 1U;
    }
    else
    {
        hour_12 = (uint8_t)(hour - 12U);
        pm = 1U;
    }

    frame[0] = RTC14_REG_SECONDS;
    frame[1] = rtc14_decimal_to_bcd(second);
    frame[2] = rtc14_decimal_to_bcd(minute);
    frame[3] = (uint8_t)(RTC14_HOUR_FORMAT_12 |
                         ((pm != 0U) ? RTC14_HOUR_PM : 0U) |
                         rtc14_decimal_to_bcd(hour_12));
    frame[4] = rtc14_decimal_to_bcd(day);
    frame[5] = rtc14_decimal_to_bcd(month);
    frame[6] = rtc14_decimal_to_bcd((uint8_t)(year - 2000U));
    frame[7] = rtc14_decimal_to_bcd(weekday);

    return iic0_bus_write(RTC14_I2C_ADDRESS,
                      frame, sizeof(frame), 0U);
}


/**
 * @brief Set default datetime.
 * @return Calculated or converted value.
 */
int rtc14_set_default_datetime(void)
{
    return rtc14_set_datetime_24h(2000U + RTC14_DEFAULT_YEAR,
                                  RTC14_DEFAULT_MONTH,
                                  RTC14_DEFAULT_DAY,
                                  RTC14_DEFAULT_HOUR,
                                  RTC14_DEFAULT_MINUTE,
                                  RTC14_DEFAULT_SECOND,
                                  RTC14_DEFAULT_WEEKDAY);
}


/**
 * @brief Read datetime.
 * @param[out] datetime Decoded RTC date and time.
 * @return Zero on success; otherwise a negative error code.
 */
int rtc14_get_datetime(rtc14_datetime_t *datetime)
{
    uint8_t raw[7];
    uint8_t raw_hour;
    int result;

    if (datetime == 0)
    {
        return RTC14_ERROR_ARGUMENT;
    }

    /* Read 00h..06h in one transaction so date and time form one snapshot. */
    result = rtc14_read_registers(RTC14_REG_SECONDS, raw, 7U);
    if (result != RTC14_OK)
    {
        return result;
    }

    datetime->second = rtc14_bcd_to_decimal(raw[0]);
    datetime->minute = rtc14_bcd_to_decimal(raw[1]);

    raw_hour = raw[2];
    datetime->is_24_hour = ((raw_hour & RTC14_HOUR_FORMAT_24) != 0U) ? 1U : 0U;
    datetime->is_pm = 0U;

    if (datetime->is_24_hour != 0U)
    {
        raw_hour &= RTC14_HOUR_24_MASK;
    }
    else
    {
        datetime->is_pm = ((raw_hour & RTC14_HOUR_PM) != 0U) ? 1U : 0U;
        raw_hour &= RTC14_HOUR_12_MASK;
    }

    datetime->hour = rtc14_bcd_to_decimal(raw_hour);
    datetime->day = rtc14_bcd_to_decimal(raw[3]);
    datetime->month = rtc14_bcd_to_decimal(raw[4]);
    datetime->year = rtc14_bcd_to_decimal(raw[5]);
    datetime->weekday = rtc14_bcd_to_decimal(raw[6]);

    return RTC14_OK;
}


/**
 * @brief Print datetime.
 * @param[in] datetime Decoded RTC date and time.
 */
void rtc14_print_datetime(const rtc14_datetime_t *datetime)
{
    static const char *const weekday_name[7] =
    {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    const char *day_name = "Unknown";

    if (datetime->weekday < 7U)
    {
        day_name = weekday_name[datetime->weekday];
    }

    if (datetime->is_24_hour != 0U)
    {
        printf_me("RTC14: %s, %02u.%02u.20%02u  %02u:%02u:%02u\r\n",
                  day_name,
                  (unsigned int)datetime->day,
                  (unsigned int)datetime->month,
                  (unsigned int)datetime->year,
                  (unsigned int)datetime->hour,
                  (unsigned int)datetime->minute,
                  (unsigned int)datetime->second);
    }
    else
    {
        printf_me("RTC14: %s, %02u.%02u.20%02u  %02u:%02u:%02u %s\r\n",
                  day_name,
                  (unsigned int)datetime->day,
                  (unsigned int)datetime->month,
                  (unsigned int)datetime->year,
                  (unsigned int)datetime->hour,
                  (unsigned int)datetime->minute,
                  (unsigned int)datetime->second,
                  (datetime->is_pm != 0U) ? "PM" : "AM");
    }
}
