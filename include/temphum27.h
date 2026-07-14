/**
 * @file temphum27.h
 * @brief Public declarations for this module.
 */

#ifndef TEMPHUM27_H
#define TEMPHUM27_H
#include "app_types.h"
/** @brief Configure and reset Temp&Hum 27 Click. */
void temphum27_init(void);
/** @brief Read temperature and relative humidity with CRC validation. */
int temphum27_read_data(temphum27_data_t *environment);
/** @brief Print a fixed-point environmental measurement. */
void temphum27_print_data(const temphum27_data_t *environment);
#endif /* TEMPHUM27_H */
