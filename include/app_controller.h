/**
 * @file app_controller.h
 * @brief Public declarations for this module.
 */

#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H
#include <stdint.h>
/** @brief Initialize all Click boards, UI, BLE and interrupt sources. */
int app_controller_init(void);
/** @brief Run the cooperative foreground application loop. */
void app_controller_run(void);
/** @brief Advance the application timebase from the AGT0 ISR. */
void app_controller_tick_1ms_isr(void);
/** @brief Handle the debounced physical B3 action from IRQ9. */
void app_controller_b3_button_isr(void);
/** @brief Handle the debounced physical B4 action from IRQ4. */
void app_controller_b4_button_isr(void);
/** @brief Request the BLE B2 action: show the watch without changing stopwatch state. */
void app_controller_request_watch_screen(void);
/** @brief Request the BLE B3 action: open/reset/back. */
void app_controller_request_stopwatch_navigation(void);
/** @brief Request the BLE B4 action: start/pause/continue. */
void app_controller_request_stopwatch_control(void);
/** @brief Request a fresh RTC read and a complete dashboard redraw. */
void app_controller_request_rtc_refresh(void);
/** @brief Return the monotonic AGT0 millisecond counter. */
uint32_t app_controller_get_system_ms(void);
#endif /* APP_CONTROLLER_H */
