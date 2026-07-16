/**
 * @file ips2_display.h
 * @brief IPS Display 2 transport and smartwatch UI rendering interface.
 */

#ifndef IPS2_DISPLAY_H
#define IPS2_DISPLAY_H

#include <stdint.h>
#include "app_types.h"

/**
 * @brief Initialize IPS Display 2 Click over SCI0 simple SPI.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @return Zero on success; otherwise a negative error code.
 */
int ips2_init(uint32_t pclka_hz);

/** @brief Invalidate UI caches so the next render redraws the complete screen. */
void ips2_invalidate_screen(void);

/**
 * @brief Draw the smartwatch dashboard with time, weekday, date and summaries.
 * @param[in] datetime Current decoded RTC date and time.
 * @param[in] environment Current temperature and humidity measurement.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_dashboard(const rtc14_datetime_t *datetime,
                           const temphum27_data_t *environment,
                           uint8_t ble_connected);

/**
 * @brief Draw the animated temperature and humidity screen.
 * @param[in] environment Current temperature and humidity measurement.
 * @param[in] ble_connected Current BLE connection state.
 * @param[in] animation_ms Animation timebase in milliseconds.
 */
void ips2_render_environment(const temphum27_data_t *environment,
                             uint8_t ble_connected,
                             uint32_t animation_ms);

/**
 * @brief Draw RTC time with the sun/moon Time of Day scene.
 * @param[in] datetime Current decoded RTC date and time.
 * @param[in] ble_connected Current BLE connection state.
 * @param[in] animation_ms Application timebase in milliseconds.
 */
void ips2_render_astronomy(const rtc14_datetime_t *datetime,
                           uint8_t ble_connected,
                           uint32_t animation_ms);

/**
 * @brief Draw the stopwatch and its context-sensitive button actions.
 * @param[in] elapsed_ms Stopwatch elapsed time in milliseconds.
 * @param[in] running Nonzero while the stopwatch is running.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_timer(uint32_t elapsed_ms, uint8_t running,
                       uint8_t ble_connected);

/**
 * @brief Draw the RTC error screen while retaining BLE status.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_error(uint8_t ble_connected);

/**
 * @brief Refresh only the BLE icon on the selected screen.
 * @param[in] connected Current BLE connection state.
 * @param[in] screen Screen whose BLE status area should be refreshed.
 */
void ips2_render_ble_status(uint8_t connected, app_screen_t screen);

#endif /* IPS2_DISPLAY_H */
