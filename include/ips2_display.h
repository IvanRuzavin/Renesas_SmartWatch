/**
 * @file ips2_display.h
 * @brief Public declarations for this module.
 */

#ifndef IPS2_DISPLAY_H
#define IPS2_DISPLAY_H
#include <stdint.h>
#include "app_types.h"
/** @brief Initialize IPS Display 2 Click over SCI0 SPI. */
int ips2_init(uint32_t pclka_hz);
/** @brief Invalidate all UI caches so the next render redraws the screen. */
void ips2_invalidate_screen(void);
/** @brief Draw the clock, environment, date and BLE status dashboard. */
void ips2_render_dashboard(const rtc14_datetime_t *datetime, const temphum27_data_t *environment, uint8_t ble_connected);
/** @brief Draw the stopwatch and its current button actions. */
void ips2_render_timer(uint32_t elapsed_ms, uint8_t running, uint8_t ble_connected);
/** @brief Draw the RTC error screen while retaining BLE status. */
void ips2_render_error(uint8_t ble_connected);
/** @brief Refresh only the BLE icon on the selected screen. */
void ips2_render_ble_status(uint8_t connected, app_screen_t screen);
#endif /* IPS2_DISPLAY_H */
