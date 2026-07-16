/**
 * @file app_controller.c
 * @brief Application state machine and foreground scheduling.
 */

#include "app_controller.h"
#include "app_config.h"
#include "app_interrupts.h"
#include "app_types.h"
#include "bletiny.h"
#include "ips2_display.h"
#include "project_platform.h"
#include "rtc14.h"
#include "temphum27.h"

static volatile uint32_t app_system_ms;
static volatile uint32_t app_stopwatch_ms;
static volatile uint32_t app_b3_last_press_ms;
static volatile uint32_t app_b4_last_press_ms;
static volatile uint32_t app_auto_b3_last_trigger_ms;
static volatile uint8_t app_b3_seen;
static volatile uint8_t app_b4_seen;
static volatile uint8_t app_stopwatch_running;
static volatile uint8_t app_periodic_update_pending;
static volatile uint8_t app_physical_b3_pending;
static volatile uint8_t app_physical_b4_pending;
static volatile uint8_t app_ble_screen_pending;
static volatile app_screen_t app_ble_requested_screen;
static volatile uint8_t app_ble_stopwatch_reset_pending;
static volatile uint8_t app_ble_stopwatch_control_pending;
static volatile uint8_t app_force_full_refresh;
static volatile app_screen_t app_active_screen = APP_SCREEN_DASHBOARD;

static rtc14_datetime_t app_datetime;
static temphum27_data_t app_environment;
static uint8_t app_datetime_valid;
static uint32_t app_last_stopwatch_render_ms = 0xFFFFFFFFUL;
static uint32_t app_last_dashboard_render_ms = 0xFFFFFFFFUL;
static uint32_t app_last_environment_render_ms = 0xFFFFFFFFUL;
static uint32_t app_last_astronomy_render_ms = 0xFFFFFFFFUL;

/**
 * @brief Log a fatal initialization error and halt the processor.
 * @param[in] message Printf-compatible fatal error message.
 * @param[in] error Error code appended to the log message.
 */
static void app_halt_with_error(const char *message, int error)
{
    printf_me(message, error);
    while (1)
    {
        __WFI();
    }
}

/**
 * @brief Return the monotonic application timebase.
 * @return Result value.
 */
uint32_t app_controller_get_system_ms(void)
{
    return app_system_ms;
}

/**
 * @brief Advance the system and stopwatch counters from the 1 ms tick ISR.
 */
void app_controller_tick_1ms_isr(void)
{
    app_system_ms++;
    if (app_stopwatch_running != 0U)
    {
        app_stopwatch_ms++;
    }
    if ((app_system_ms % 1000UL) == 0U)
    {
        app_periodic_update_pending = 1U;
    }
}

/**
 * @brief Debounce B3 and queue the physical screen-cycle action.
 */
void app_controller_b3_button_isr(void)
{
    uint32_t now = app_system_ms;
    if ((app_b3_seen == 0U) ||
        ((uint32_t)(now - app_b3_last_press_ms) >= APP_B3_DEBOUNCE_MS))
    {
        app_b3_seen = 1U;
        app_b3_last_press_ms = now;
        app_physical_b3_pending = 1U;
    }
}

/**
 * @brief Debounce B4 and queue the physical stopwatch-control action.
 */
void app_controller_b4_button_isr(void)
{
    uint32_t now = app_system_ms;
    if ((app_b4_seen == 0U) ||
        ((uint32_t)(now - app_b4_last_press_ms) >= APP_B4_DEBOUNCE_MS))
    {
        app_b4_seen = 1U;
        app_b4_last_press_ms = now;
        app_physical_b4_pending = 1U;
    }
}

/**
 * @brief Queue dashboard screen.
 */
void app_controller_request_dashboard_screen(void)
{
    app_ble_requested_screen = APP_SCREEN_DASHBOARD;
    app_ble_screen_pending = 1U;
}

/**
 * @brief Queue environment screen.
 */
void app_controller_request_environment_screen(void)
{
    app_ble_requested_screen = APP_SCREEN_ENVIRONMENT;
    app_ble_screen_pending = 1U;
}

/**
 * @brief Queue astronomy screen.
 */
void app_controller_request_astronomy_screen(void)
{
    app_ble_requested_screen = APP_SCREEN_ASTRONOMY;
    app_ble_screen_pending = 1U;
}

/**
 * @brief Queue stopwatch reset.
 */
void app_controller_request_stopwatch_reset(void)
{
    app_ble_stopwatch_reset_pending = 1U;
}

/**
 * @brief Queue stopwatch control.
 */
void app_controller_request_stopwatch_control(void)
{
    app_ble_stopwatch_control_pending = 1U;
}

/**
 * @brief Queue rtc refresh.
 */
void app_controller_request_rtc_refresh(void)
{
    app_force_full_refresh = 1U;
    app_periodic_update_pending = 1U;
}

/**
 * @brief Invalidate all application-level display refresh timestamps.
 */
static void app_reset_render_timers(void)
{
    app_last_stopwatch_render_ms = 0xFFFFFFFFUL;
    app_last_dashboard_render_ms = 0xFFFFFFFFUL;
    app_last_environment_render_ms = 0xFFFFFFFFUL;
    app_last_astronomy_render_ms = 0xFFFFFFFFUL;
}

/**
 * @brief Render the currently selected application screen.
 * @param[in] now Current monotonic application time in milliseconds.
 */
static void app_render_active_screen(uint32_t now)
{
    uint8_t connected = bletiny_is_connected();

    if (app_active_screen == APP_SCREEN_DASHBOARD)
    {
        if (app_datetime_valid != 0U)
        {
            ips2_render_dashboard(&app_datetime, &app_environment, connected);
        }
        else
        {
            ips2_render_error(connected);
        }
        app_last_dashboard_render_ms = now;
    }
    else if (app_active_screen == APP_SCREEN_ENVIRONMENT)
    {
        ips2_render_environment(&app_environment, connected, now);
        app_last_environment_render_ms = now;
    }
    else if (app_active_screen == APP_SCREEN_ASTRONOMY)
    {
        if (app_datetime_valid != 0U)
        {
            ips2_render_astronomy(&app_datetime, connected, now);
        }
        else
        {
            ips2_render_error(connected);
        }
        app_last_astronomy_render_ms = now;
    }
    else
    {
        ips2_render_timer(app_stopwatch_ms, app_stopwatch_running, connected);
        app_last_stopwatch_render_ms = app_stopwatch_ms;
    }
}

/**
 * @brief Select a screen, invalidate display caches and render it immediately.
 * @param[in] screen Application screen whose status area is being updated.
 * @param[in] now Current monotonic application time in milliseconds.
 */
static void app_switch_screen(app_screen_t screen, uint32_t now)
{
    if (app_active_screen != screen)
    {
        app_active_screen = screen;
        ips2_invalidate_screen();
        app_reset_render_timers();
    }
    app_render_active_screen(now);
}

/**
 * @brief Initialize the module.
 * @return Zero on success; otherwise a negative error code.
 */
int app_controller_init(void)
{
    int result;
    int environment_result;
    system_clocks_t clocks;

    app_environment.temperature_tenths_c = 0;
    app_environment.humidity_tenths_percent = 0U;
    app_environment.valid = 0U;
    app_datetime_valid = 0U;

    result = rtc14_init();
    if (result != RTC14_OK)
    {
        app_halt_with_error("RTC14 initialization failed: %d\r\n", result);
    }

    result = rtc14_set_default_datetime();
    if (result != RTC14_OK)
    {
        app_halt_with_error("RTC14 default date/time write failed: %d\r\n", result);
    }

    temphum27_init();
    SYSTEM_GetClocksFrequency(&clocks);

    if (ips2_init(clocks.pclka) != 0)
    {
        app_halt_with_error("IPS Display 2 initialization failed: %d\r\n", -1);
    }

    environment_result = temphum27_read_data(&app_environment);
    if (environment_result == RTC14_OK)
    {
        temphum27_print_data(&app_environment);
    }
    else
    {
        printf_me("TempHum27 initial read failed: %d\r\n", environment_result);
    }

    result = rtc14_get_datetime(&app_datetime);
    if (result == RTC14_OK)
    {
        app_datetime_valid = 1U;
        rtc14_print_datetime(&app_datetime);
    }

    result = app_interrupts_init(clocks.pclkb);
    if (result != RTC14_OK)
    {
        app_halt_with_error("Interrupt initialization failed: %d\r\n", result);
    }

    if (bletiny_init(clocks.pclka, app_system_ms) != 0)
    {
        app_halt_with_error("BLE TINY SCI3 initialization failed: %d\r\n", -1);
    }

    if (APP_AUTO_B3_DEMO_ENABLED)
    {
        /* Demo mode keeps the stopwatch running in the background at all times. */
        app_stopwatch_ms = 0U;
        app_stopwatch_running = 1U;
    }

    if (app_datetime_valid != 0U)
    {
        ips2_render_dashboard(&app_datetime, &app_environment,
                              0U);
    }
    else
    {
        ips2_render_error(0U);
    }
    app_last_dashboard_render_ms = app_system_ms;
    app_auto_b3_last_trigger_ms = app_system_ms;

    printf_me("RTC14 initialized to 13.07.2026 12:18:00.\r\n");
    if (APP_AUTO_B3_DEMO_ENABLED)
    {
        printf_me("Automatic demo enabled: WATCH -> ENV -> TIME OF DAY -> STOPWATCH -> WATCH every 15 seconds.\r\n");
        printf_me("The stopwatch starts at boot, runs continuously, and B3 only changes screens.\r\n");
        printf_me("B4 and BLE stopwatch stop/pause/reset commands are locked in demo mode.\r\n");
    }
    else
    {
        printf_me("Physical B3 cycles WATCH -> ENV -> TIME OF DAY -> STOPWATCH -> WATCH; at a nonzero stopwatch value it resets first.\r\n");
        printf_me("Physical B4 starts, pauses and continues the stopwatch while its screen is active.\r\n");
        printf_me("Automatic B3 demo is disabled. Set APP_AUTO_B3_DEMO_ENABLED to true to enable it.\r\n");
    }
    printf_me("TempHum27 and RTC14 are sampled once per second on shared IIC0.\r\n");
    printf_me("BLE TINY uses SCI3 at 57600 baud.\r\n");
    printf_me("RTC: AT+MEM=0,RTC|YYYY|MM|DD|hh|mm|ss|weekday\r\n");
    printf_me("B1:  AT+MEM=0,SCREEN|B1       (smartwatch dashboard)\r\n");
    printf_me("B2:  AT+MEM=0,SCREEN|B2       (temperature/humidity)\r\n");
    printf_me("B5:  AT+MEM=0,SCREEN|B5       (time of day)\r\n");
    printf_me("B3:  AT+MEM=0,STOPWATCH|B3    (stop and reset)\r\n");
    printf_me("B4:  AT+MEM=0,STOPWATCH|B4    (show/start/pause/continue)\r\n");

    return RTC14_OK;
}

/**
 * @brief Queue a physical B3 action when automatic demonstration mode expires.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_auto_b3(uint32_t now)
{
    if (APP_AUTO_B3_DEMO_ENABLED &&
        ((uint32_t)(now - app_auto_b3_last_trigger_ms) >=
         APP_AUTO_B3_DEMO_INTERVAL_MS))
    {
        app_auto_b3_last_trigger_ms = now;
        app_physical_b3_pending = 1U;
        return 1U;
    }

    return 0U;
}

/**
 * @brief Refresh the UI after a BLE connection-state change.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_ble_status(void)
{
    uint8_t connected;
    if (bletiny_take_connection_changed(&connected) == 0U)
    {
        return 0U;
    }

    ips2_render_ble_status(connected, app_active_screen);
    printf_me("BLE TINY: %s\r\n", connected ? "CONNECTED" : "DISCONNECTED");
    return 1U;
}

/**
 * @brief Process a pending physical or automatic B3 action.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_physical_b3(uint32_t now)
{
    uint32_t primask;
    app_screen_t next_screen;
    uint8_t reset_only = 0U;

    if (app_physical_b3_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_physical_b3_pending = 0U;
    app_auto_b3_last_trigger_ms = now;

    if ((!APP_AUTO_B3_DEMO_ENABLED) &&
        (app_active_screen == APP_SCREEN_STOPWATCH) &&
        ((app_stopwatch_ms != 0U) || (app_stopwatch_running != 0U)))
    {
        app_stopwatch_ms = 0U;
        app_stopwatch_running = 0U;
        reset_only = 1U;
        next_screen = APP_SCREEN_STOPWATCH;
    }
    else if (app_active_screen == APP_SCREEN_DASHBOARD)
    {
        next_screen = APP_SCREEN_ENVIRONMENT;
    }
    else if (app_active_screen == APP_SCREEN_ENVIRONMENT)
    {
        next_screen = APP_SCREEN_ASTRONOMY;
    }
    else if (app_active_screen == APP_SCREEN_ASTRONOMY)
    {
        next_screen = APP_SCREEN_STOPWATCH;
    }
    else
    {
        /* The stopwatch is already at 00:00:00.000: wrap to screen one. */
        next_screen = APP_SCREEN_DASHBOARD;
    }

    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (reset_only != 0U)
    {
        ips2_render_timer(0U, 0U, bletiny_is_connected());
        app_last_stopwatch_render_ms = 0U;
    }
    else
    {
        app_switch_screen(next_screen, now);
    }
    return 1U;
}

/**
 * @brief Process a pending physical B4 stopwatch action.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_physical_b4(void)
{
    uint32_t primask;
    uint8_t redraw = 0U;
    uint32_t elapsed;
    uint8_t running;

    if (app_physical_b4_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_physical_b4_pending = 0U;
    if (app_active_screen == APP_SCREEN_STOPWATCH)
    {
        if (APP_AUTO_B3_DEMO_ENABLED)
        {
            /* The automatic showcase owns the stopwatch run state. */
            app_stopwatch_running = 1U;
        }
        else
        {
            app_stopwatch_running = app_stopwatch_running ? 0U : 1U;
        }
        redraw = 1U;
    }
    elapsed = app_stopwatch_ms;
    running = app_stopwatch_running;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (redraw != 0U)
    {
        ips2_render_timer(elapsed, running, bletiny_is_connected());
        app_last_stopwatch_render_ms = elapsed;
    }
    return 1U;
}

/**
 * @brief Apply a pending BLE screen-selection request.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_ble_screen_request(uint32_t now)
{
    uint32_t primask;
    app_screen_t requested;

    if (app_ble_screen_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    requested = app_ble_requested_screen;
    app_ble_screen_pending = 0U;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    app_switch_screen(requested, now);
    return 1U;
}

/**
 * @brief Apply the BLE stopwatch stop/reset request.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_ble_stopwatch_reset(uint32_t now)
{
    uint32_t primask;

    if (app_ble_stopwatch_reset_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_ble_stopwatch_reset_pending = 0U;
    if (APP_AUTO_B3_DEMO_ENABLED)
    {
        /* Demo mode must remain continuous even when BLE B3 is received. */
        app_stopwatch_running = 1U;
    }
    else
    {
        app_stopwatch_ms = 0U;
        app_stopwatch_running = 0U;
    }
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    /* BLE B3 opens the timer; reset is suppressed during automatic demo mode. */
    app_switch_screen(APP_SCREEN_STOPWATCH, now);
    return 1U;
}

/**
 * @brief Apply the BLE stopwatch start/pause/continue request.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_ble_stopwatch_control(uint32_t now)
{
    uint32_t primask;
    uint32_t elapsed;
    uint8_t running;
    uint8_t screen_changed;

    if (app_ble_stopwatch_control_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_ble_stopwatch_control_pending = 0U;
    screen_changed = (app_active_screen != APP_SCREEN_STOPWATCH) ? 1U : 0U;
    app_active_screen = APP_SCREEN_STOPWATCH;
    if (APP_AUTO_B3_DEMO_ENABLED)
    {
        /* BLE B4 may show the stopwatch but cannot pause the automatic demo. */
        app_stopwatch_running = 1U;
    }
    else
    {
        app_stopwatch_running = app_stopwatch_running ? 0U : 1U;
    }
    elapsed = app_stopwatch_ms;
    running = app_stopwatch_running;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (screen_changed != 0U)
    {
        ips2_invalidate_screen();
        app_reset_render_timers();
    }
    ips2_render_timer(elapsed, running, bletiny_is_connected());
    app_last_stopwatch_render_ms = elapsed;
    (void)now;
    return 1U;
}

/**
 * @brief Read the RTC and environmental sensor during the one-second update.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t app_handle_periodic_update(uint32_t now)
{
    int result;
    int environment_result;

    if (app_periodic_update_pending == 0U)
    {
        return 0U;
    }
    app_periodic_update_pending = 0U;

    if (app_force_full_refresh != 0U)
    {
        app_force_full_refresh = 0U;
        ips2_invalidate_screen();
        app_reset_render_timers();
    }

    environment_result = temphum27_read_data(&app_environment);
    if (environment_result == RTC14_OK)
    {
        temphum27_print_data(&app_environment);
    }
    else
    {
        printf_me("TempHum27 read failed: %d\r\n", environment_result);
    }

    result = rtc14_get_datetime(&app_datetime);
    if (result == RTC14_OK)
    {
        app_datetime_valid = 1U;
        rtc14_print_datetime(&app_datetime);
    }
    else
    {
        app_datetime_valid = 0U;
        printf_me("RTC14 read failed: %d\r\n", result);
    }

    if (app_active_screen == APP_SCREEN_DASHBOARD)
    {
        if (app_datetime_valid != 0U)
        {
            ips2_render_dashboard(&app_datetime, &app_environment,
                                  bletiny_is_connected());
        }
        else
        {
            ips2_render_error(bletiny_is_connected());
        }
        app_last_dashboard_render_ms = now;
    }
    else if (app_active_screen == APP_SCREEN_ENVIRONMENT)
    {
        ips2_render_environment(&app_environment, bletiny_is_connected(), now);
        app_last_environment_render_ms = now;
    }
    else if (app_active_screen == APP_SCREEN_ASTRONOMY)
    {
        if (app_datetime_valid != 0U)
        {
            ips2_render_astronomy(&app_datetime, bletiny_is_connected(), now);
        }
        else
        {
            ips2_render_error(bletiny_is_connected());
        }
        app_last_astronomy_render_ms = now;
    }
    return 1U;
}

/**
 * @brief Refresh animated screen content when its render interval expires.
 * @param[in] now Current monotonic application time in milliseconds.
 * @return Result value.
 */
static uint8_t app_refresh_active_screen_if_due(uint32_t now)
{
    if (app_active_screen == APP_SCREEN_STOPWATCH)
    {
        uint32_t elapsed = app_stopwatch_ms;
        if ((app_last_stopwatch_render_ms != 0xFFFFFFFFUL) &&
            ((uint32_t)(elapsed - app_last_stopwatch_render_ms) <
             IPS2_STOPWATCH_REFRESH_MS))
        {
            return 0U;
        }

        ips2_render_timer(elapsed, app_stopwatch_running,
                          bletiny_is_connected());
        app_last_stopwatch_render_ms = elapsed;
        return 1U;
    }

    if (app_active_screen == APP_SCREEN_DASHBOARD)
    {
        if ((app_last_dashboard_render_ms != 0xFFFFFFFFUL) &&
            ((uint32_t)(now - app_last_dashboard_render_ms) <
             IPS2_DASHBOARD_REFRESH_MS))
        {
            return 0U;
        }

        if (app_datetime_valid != 0U)
        {
            ips2_render_dashboard(&app_datetime, &app_environment,
                                  bletiny_is_connected());
        }
        else
        {
            ips2_render_error(bletiny_is_connected());
        }
        app_last_dashboard_render_ms = now;
        return 1U;
    }

    if (app_active_screen == APP_SCREEN_ENVIRONMENT)
    {
        if ((app_last_environment_render_ms != 0xFFFFFFFFUL) &&
            ((uint32_t)(now - app_last_environment_render_ms) <
             IPS2_ENVIRONMENT_REFRESH_MS))
        {
            return 0U;
        }

        ips2_render_environment(&app_environment, bletiny_is_connected(), now);
        app_last_environment_render_ms = now;
        return 1U;
    }

    if ((app_last_astronomy_render_ms != 0xFFFFFFFFUL) &&
        ((uint32_t)(now - app_last_astronomy_render_ms) <
         IPS2_ASTRONOMY_REFRESH_MS))
    {
        return 0U;
    }

    if (app_datetime_valid != 0U)
    {
        ips2_render_astronomy(&app_datetime, bletiny_is_connected(), now);
    }
    else
    {
        ips2_render_error(bletiny_is_connected());
    }
    app_last_astronomy_render_ms = now;
    return 1U;
}

/**
 * @brief Handle run.
 */
void app_controller_run(void)
{
    while (1)
    {
        uint8_t did_work = 0U;
        uint32_t now = app_system_ms;

        did_work |= bletiny_process_rx();
        did_work |= app_handle_ble_status();
        did_work |= bletiny_service(now);
        did_work |= app_handle_auto_b3(now);
        did_work |= app_handle_physical_b3(now);
        did_work |= app_handle_physical_b4();
        did_work |= app_handle_ble_screen_request(now);
        did_work |= app_handle_ble_stopwatch_reset(now);
        did_work |= app_handle_ble_stopwatch_control(now);
        did_work |= app_handle_periodic_update(now);
        did_work |= app_refresh_active_screen_if_due(now);

        if (did_work == 0U)
        {
            __WFI();
        }
    }
}
