/**
 * @file rtc14.h
 * @brief Public declarations for this module.
 */

#ifndef RTC14_H
#define RTC14_H
#include <stdint.h>
#include "app_types.h"
/** @brief Initialize the shared IIC0 transport used by RTC 14. */
int rtc14_init(void);
/** @brief Apply the example's configured default date and time. */
int rtc14_set_default_datetime(void);
/** @brief Write a validated 24-hour date/time value to RTC 14. */
int rtc14_set_datetime_24h(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday);
/** @brief Read a coherent RTC date/time snapshot. */
int rtc14_get_datetime(rtc14_datetime_t *datetime);
/** @brief Print a decoded RTC value through printf_me(). */
void rtc14_print_datetime(const rtc14_datetime_t *datetime);
#endif /* RTC14_H */
