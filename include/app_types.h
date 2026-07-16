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
    RTC14_OK = 0,              /**< Operation completed successfully. */
    RTC14_ERROR_TIMEOUT = -1, /**< A peripheral status flag did not arrive in time. */
    RTC14_ERROR_NACK = -2,    /**< The addressed I2C device returned NACK. */
    RTC14_ERROR_CLOCK = -3,   /**< A valid peripheral timing configuration was not found. */
    RTC14_ERROR_ARGUMENT = -4 /**< A pointer or numeric argument was invalid. */
} app_result_t;

/** @brief Decoded RTC 14 date and time. */
typedef struct
{
    uint8_t second;     /**< Seconds in the range 0 through 59. */
    uint8_t minute;     /**< Minutes in the range 0 through 59. */
    uint8_t hour;       /**< Decoded hour in RTC-selected 12- or 24-hour form. */
    uint8_t day;        /**< Day of month. */
    uint8_t month;      /**< Month in the range 1 through 12. */
    uint8_t year;       /**< Two-digit year value. */
    uint8_t weekday;    /**< Weekday number, where zero is Sunday. */
    uint8_t is_24_hour; /**< Nonzero when the RTC is operating in 24-hour mode. */
    uint8_t is_pm;      /**< Nonzero for PM while using 12-hour mode. */
} rtc14_datetime_t;

/** @brief Fixed-point Temp&Hum 27 measurement. */
typedef struct
{
    int16_t temperature_tenths_c;       /**< Temperature in tenths of a degree Celsius. */
    uint16_t humidity_tenths_percent;   /**< Relative humidity in tenths of a percent. */
    uint8_t valid;                      /**< Nonzero when both values passed CRC validation. */
} temphum27_data_t;

/** @brief Visible application screen. */
typedef enum
{
    APP_SCREEN_DASHBOARD = 0,  /**< Smartwatch summary with clock, weekday and date. */
    APP_SCREEN_ENVIRONMENT = 1,/**< Animated thermometer and humidity droplet. */
    APP_SCREEN_ASTRONOMY = 2,  /**< Time of Day sun/moon scene. */
    APP_SCREEN_STOPWATCH = 3   /**< Stopwatch display and controls. */
} app_screen_t;

#endif /* APP_TYPES_H */
