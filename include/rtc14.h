/**
 * @file rtc14.h
 * @brief RTC 14 Click driver interface.
 */

#ifndef RTC14_H
#define RTC14_H

#include <stdint.h>
#include "app_types.h"

/**
 * @brief Initialize the shared IIC0 transport used by RTC 14.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int rtc14_init(void);

/**
 * @brief Apply the example's configured default date and time.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int rtc14_set_default_datetime(void);

/**
 * @brief Write a validated 24-hour date/time value to RTC 14.
 * @param[in] year Full Gregorian year.
 * @param[in] month Month in the range 1 through 12.
 * @param[in] day Day of month.
 * @param[in] hour Hour in the range 0 through 23.
 * @param[in] minute Minute in the range 0 through 59.
 * @param[in] second Second in the range 0 through 59.
 * @param[in] weekday Weekday number, where zero is Sunday.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int rtc14_set_datetime_24h(uint16_t year, uint8_t month, uint8_t day,
                           uint8_t hour, uint8_t minute, uint8_t second,
                           uint8_t weekday);

/**
 * @brief Read a coherent RTC date/time snapshot.
 * @param[out] datetime Destination decoded date/time structure.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int rtc14_get_datetime(rtc14_datetime_t *datetime);

/**
 * @brief Print a decoded RTC value through printf_me().
 * @param[in] datetime Date/time value to print.
 */
void rtc14_print_datetime(const rtc14_datetime_t *datetime);

#endif /* RTC14_H */
