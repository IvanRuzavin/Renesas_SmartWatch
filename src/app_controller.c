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
static volatile uint8_t app_b3_seen;
static volatile uint8_t app_b4_seen;
static volatile uint8_t app_stopwatch_running;
static volatile uint8_t app_rtc_update_pending;
static volatile uint8_t app_stopwatch_nav_pending;
static volatile uint8_t app_watch_nav_pending;
static volatile uint8_t app_stopwatch_control_pending;
static volatile uint8_t app_force_full_dashboard_refresh;
static volatile app_screen_t app_active_screen = APP_SCREEN_CLOCK;

static rtc14_datetime_t app_datetime;
static temphum27_data_t app_environment;
static uint32_t app_last_stopwatch_render_ms = 0xFFFFFFFFUL;

static void app_halt_with_error(const char *message, int error)
{
    printf_me(message, error);
    while (1)
    {
        __WFI();
    }
}

uint32_t app_controller_get_system_ms(void)
{
    return app_system_ms;
}

void app_controller_tick_1ms_isr(void)
{
    app_system_ms++;
    if (app_stopwatch_running != 0U)
    {
        app_stopwatch_ms++;
    }
    if ((app_system_ms % 1000UL) == 0U)
    {
        app_rtc_update_pending = 1U;
    }
}

void app_controller_b3_button_isr(void)
{
    uint32_t now = app_system_ms;
    if ((app_b3_seen == 0U) ||
        ((uint32_t)(now - app_b3_last_press_ms) >= APP_B3_DEBOUNCE_MS))
    {
        app_b3_seen = 1U;
        app_b3_last_press_ms = now;
        app_stopwatch_nav_pending = 1U;
    }
}

void app_controller_b4_button_isr(void)
{
    uint32_t now = app_system_ms;
    if ((app_b4_seen == 0U) ||
        ((uint32_t)(now - app_b4_last_press_ms) >= APP_B4_DEBOUNCE_MS))
    {
        app_b4_seen = 1U;
        app_b4_last_press_ms = now;
        app_stopwatch_control_pending = 1U;
    }
}

void app_controller_request_watch_screen(void)
{
    app_watch_nav_pending = 1U;
}

void app_controller_request_stopwatch_navigation(void)
{
    app_stopwatch_nav_pending = 1U;
}

void app_controller_request_stopwatch_control(void)
{
    app_stopwatch_control_pending = 1U;
}

void app_controller_request_rtc_refresh(void)
{
    app_force_full_dashboard_refresh = 1U;
    app_rtc_update_pending = 1U;
}

int app_controller_init(void)
{
    int result;
    int environment_result;
    system_clocks_t clocks;

    app_environment.temperature_tenths_c = 0;
    app_environment.humidity_tenths_percent = 0U;
    app_environment.valid = 0U;

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
        rtc14_print_datetime(&app_datetime);
        ips2_render_dashboard(&app_datetime, &app_environment, 0U);
    }
    else
    {
        ips2_render_error(0U);
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

    printf_me("RTC14 initialized to 13.07.2026 12:18:00 PM.\r\n");
    printf_me("AGT0 runs at 1 ms; B3 uses P304/IRQ9 and B4 uses P111/IRQ4.\r\n");
    printf_me("TempHum27 is sampled once per second on shared IIC0.\r\n");
    printf_me("BLE TINY uses SCI3 at 57600 baud.\r\n");
    printf_me("RTC: AT+MEM=0,RTC|YYYY|MM|DD|hh|mm|ss|weekday\r\n");
    printf_me("B2:  AT+MEM=0,STOPWATCH|B2\r\n");
    printf_me("B3:  AT+MEM=0,STOPWATCH|B3\r\n");
    printf_me("B4:  AT+MEM=0,STOPWATCH|B4\r\n");

    return RTC14_OK;
}

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

static uint8_t app_handle_watch_navigation(void)
{
    uint32_t primask;
    uint8_t changed = 0U;

    if (app_watch_nav_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_watch_nav_pending = 0U;
    if (app_active_screen != APP_SCREEN_CLOCK)
    {
        app_active_screen = APP_SCREEN_CLOCK;
        app_rtc_update_pending = 1U;
        changed = 1U;
    }
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (changed != 0U)
    {
        ips2_invalidate_screen();
        app_last_stopwatch_render_ms = 0xFFFFFFFFUL;
    }
    return 1U;
}

static uint8_t app_handle_stopwatch_navigation(void)
{
    uint32_t primask;
    uint8_t show_stopwatch;
    uint32_t elapsed;
    uint8_t running;

    if (app_stopwatch_nav_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_stopwatch_nav_pending = 0U;
    app_stopwatch_control_pending = 0U;

    if (app_active_screen == APP_SCREEN_CLOCK)
    {
        app_active_screen = APP_SCREEN_STOPWATCH;
    }
    else if (app_stopwatch_ms != 0U)
    {
        app_stopwatch_ms = 0U;
        app_stopwatch_running = 0U;
    }
    else
    {
        app_stopwatch_running = 0U;
        app_active_screen = APP_SCREEN_CLOCK;
        app_rtc_update_pending = 1U;
    }

    show_stopwatch = (app_active_screen == APP_SCREEN_STOPWATCH) ? 1U : 0U;
    elapsed = app_stopwatch_ms;
    running = app_stopwatch_running;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    ips2_invalidate_screen();
    app_last_stopwatch_render_ms = 0xFFFFFFFFUL;
    if (show_stopwatch != 0U)
    {
        ips2_render_timer(elapsed, running, bletiny_is_connected());
        app_last_stopwatch_render_ms = elapsed;
    }
    return 1U;
}

static uint8_t app_handle_stopwatch_control(void)
{
    uint32_t primask;
    uint8_t show_stopwatch;
    uint32_t elapsed;
    uint8_t running;

    if (app_stopwatch_control_pending == 0U)
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    app_stopwatch_control_pending = 0U;
    if (app_active_screen == APP_SCREEN_STOPWATCH)
    {
        app_stopwatch_running = app_stopwatch_running ? 0U : 1U;
    }
    show_stopwatch = (app_active_screen == APP_SCREEN_STOPWATCH) ? 1U : 0U;
    elapsed = app_stopwatch_ms;
    running = app_stopwatch_running;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (show_stopwatch != 0U)
    {
        ips2_render_timer(elapsed, running, bletiny_is_connected());
        app_last_stopwatch_render_ms = elapsed;
    }
    return 1U;
}

static uint8_t app_handle_periodic_update(void)
{
    int result;
    int environment_result;

    if (app_rtc_update_pending == 0U)
    {
        return 0U;
    }
    app_rtc_update_pending = 0U;

    if (app_force_full_dashboard_refresh != 0U)
    {
        app_force_full_dashboard_refresh = 0U;
        ips2_invalidate_screen();
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
        rtc14_print_datetime(&app_datetime);
        if (app_active_screen == APP_SCREEN_CLOCK)
        {
            ips2_render_dashboard(&app_datetime, &app_environment,
                                  bletiny_is_connected());
        }
    }
    else
    {
        printf_me("RTC14 read failed: %d\r\n", result);
        if (app_active_screen == APP_SCREEN_CLOCK)
        {
            ips2_render_error(bletiny_is_connected());
        }
    }
    return 1U;
}

static uint8_t app_refresh_stopwatch_if_due(void)
{
    uint32_t elapsed;

    if (app_active_screen != APP_SCREEN_STOPWATCH)
    {
        return 0U;
    }

    elapsed = app_stopwatch_ms;
    if ((app_last_stopwatch_render_ms != 0xFFFFFFFFUL) &&
        ((uint32_t)(elapsed - app_last_stopwatch_render_ms) <
         IPS2_STOPWATCH_REFRESH_MS))
    {
        return 0U;
    }

    ips2_render_timer(elapsed, app_stopwatch_running, bletiny_is_connected());
    app_last_stopwatch_render_ms = elapsed;
    return 1U;
}

void app_controller_run(void)
{
    while (1)
    {
        uint8_t did_work = 0U;
        uint32_t now = app_system_ms;

        did_work |= bletiny_process_rx();
        did_work |= app_handle_ble_status();
        did_work |= bletiny_service(now);
        did_work |= app_handle_watch_navigation();
        did_work |= app_handle_stopwatch_navigation();
        did_work |= app_handle_stopwatch_control();
        did_work |= app_handle_periodic_update();
        did_work |= app_refresh_stopwatch_if_due();

        if (did_work == 0U)
        {
            __WFI();
        }
    }
}
