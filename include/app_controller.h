/**
 * @file app_controller.h
 * @brief Application state-machine and foreground-loop interface.
 */

#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include <stdint.h>

/**
 * @brief Initialize Click boards, sensors, display, BLE and interrupt sources.
 * @return RTC14_OK on success; otherwise a negative application error code.
 */
int app_controller_init(void);

/** @brief Run the cooperative foreground application loop indefinitely. */
void app_controller_run(void);

/** @brief Advance the system timebase and running stopwatch from the AGT0 ISR. */
void app_controller_tick_1ms_isr(void);

/** @brief Debounce the physical B3 input and queue its screen-cycle action. */
void app_controller_b3_button_isr(void);

/** @brief Debounce the physical B4 input and queue its stopwatch action. */
void app_controller_b4_button_isr(void);

/** @brief Request the BLE B1 action that opens the smartwatch dashboard. */
void app_controller_request_dashboard_screen(void);

/** @brief Request the BLE B2 action that opens the environment screen. */
void app_controller_request_environment_screen(void);

/** @brief Request the BLE B5 action that opens the Time of Day screen. */
void app_controller_request_astronomy_screen(void);

/** @brief Request the BLE B3 action that stops and resets the stopwatch. */
void app_controller_request_stopwatch_reset(void);

/** @brief Request the BLE B4 action that shows or toggles the stopwatch. */
void app_controller_request_stopwatch_control(void);

/** @brief Request a fresh RTC read and complete active-screen redraw. */
void app_controller_request_rtc_refresh(void);

/**
 * @brief Return the monotonic AGT0 application counter.
 * @return Milliseconds elapsed since the application tick was started.
 */
uint32_t app_controller_get_system_ms(void);

#endif /* APP_CONTROLLER_H */
