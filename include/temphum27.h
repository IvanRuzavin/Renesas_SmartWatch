/**
 * @file temphum27.h
 * @brief Temp&Hum 27 Click driver interface.
 */

#ifndef TEMPHUM27_H
#define TEMPHUM27_H

#include "app_types.h"

/** @brief Configure, enable and reset Temp&Hum 27 Click. */
void temphum27_init(void);

/**
 * @brief Read temperature and relative humidity with CRC validation.
 * @param[out] environment Destination fixed-point environmental measurement.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int temphum27_read_data(temphum27_data_t *environment);

/**
 * @brief Print a fixed-point environmental measurement through printf_me().
 * @param[in] environment Measurement to print.
 */
void temphum27_print_data(const temphum27_data_t *environment);

#endif /* TEMPHUM27_H */
