/**
 * @file app_types.h
 * @brief Shared data types and result codes.
 */

#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

/** @brief Common return values used by the bare-metal drivers. */
typedef enum
{
    RTC14_OK = 0,
    RTC14_ERROR_TIMEOUT = -1,
    RTC14_ERROR_NACK = -2,
    RTC14_ERROR_CLOCK = -3,
    RTC14_ERROR_ARGUMENT = -4
} app_result_t;

/** @brief Decoded RTC 14 date and time. */
typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t weekday;
    uint8_t is_24_hour;
    uint8_t is_pm;
} rtc14_datetime_t;

/** @brief Fixed-point Temp&Hum 27 measurement. */
typedef struct
{
    int16_t temperature_tenths_c;
    uint16_t humidity_tenths_percent;
    uint8_t valid;
} temphum27_data_t;

/** @brief Visible application screen. */
typedef enum
{
    APP_SCREEN_CLOCK = 0,
    APP_SCREEN_STOPWATCH = 1
} app_screen_t;

#endif /* APP_TYPES_H */
