/**
 * @file project_platform.h
 * @brief Project-specific SDK hooks used by the example.
 */

#ifndef PROJECT_PLATFORM_H
#define PROJECT_PLATFORM_H

#include <stdint.h>
#include "mcu.h"
#include "delays.h"

/** @brief System clock frequencies supplied by the NECTO project. */
typedef struct
{
    uint32_t iclk;
    uint32_t pclka;
    uint32_t pclkb;
    uint32_t pclkc;
    uint32_t pclkd;
    uint32_t fclk;
} system_clocks_t;

/**
 * @brief Read the configured MCU clock frequencies.
 * @param[out] clocks Destination structure.
 */
void SYSTEM_GetClocksFrequency(system_clocks_t *clocks);

/**
 * @brief Project logging function with printf-compatible formatting.
 * @param format Formatting string.
 * @return Number of emitted characters, or a negative error code.
 */
int printf_me(const char *format, ...);

#endif /* PROJECT_PLATFORM_H */
