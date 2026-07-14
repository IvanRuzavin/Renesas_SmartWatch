/**
 * @file iic0_bus.h
 * @brief Public declarations for this module.
 */

#ifndef IIC0_BUS_H
#define IIC0_BUS_H
#include <stdint.h>
/** @brief Initialize IIC0 on P400/P401 at the configured bus speed. */
int iic0_bus_init(void);
/** @brief Write one transaction to a 7-bit I2C address. */
int iic0_bus_write(uint8_t address, const uint8_t *data, uint32_t length, uint8_t restart_after_write);
/** @brief Read after a previously generated repeated START. */
int iic0_bus_read_after_restart(uint8_t address, uint8_t *data, uint32_t length);
#endif /* IIC0_BUS_H */
