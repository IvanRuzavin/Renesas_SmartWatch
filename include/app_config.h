/**
 * @file app_config.h
 * @brief Compile-time configuration for the R7FA4M2 smartwatch example.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* Shared IIC0 bus. */
#define APP_I2C_BITRATE_HZ                  (100000UL)
#define APP_I2C_TIMEOUT                     (1000000UL)

/* RTC 14 Click. */
#define RTC14_I2C_ADDRESS                   (0x6FU)
#define RTC14_DEFAULT_SECOND                (0U)
#define RTC14_DEFAULT_MINUTE                (18U)
#define RTC14_DEFAULT_HOUR                  (12U)
#define RTC14_DEFAULT_DAY                   (13U)
#define RTC14_DEFAULT_MONTH                 (7U)
#define RTC14_DEFAULT_YEAR                  (26U)
#define RTC14_DEFAULT_WEEKDAY               (1U)

/* Temp&Hum 27 Click. */
#define TEMPHUM27_I2C_ADDRESS               (0x54U)
#define TEMPHUM27_CMD_HOLD_TEMP_HUM         (0xE5U)
#define TEMPHUM27_DATA_RESOLUTION           (0x3FFFUL)
#define TEMPHUM27_TEMP_SPAN_TENTHS          (1650L)
#define TEMPHUM27_TEMP_OFFSET_TENTHS        (400L)
#define TEMPHUM27_HUM_SPAN_TENTHS           (1000UL)
#define TEMPHUM27_ERROR_CRC                 (-5)

/* AGT0 1 ms application tick. */
#define APP_AGT0_IRQ_SLOT                   (15U)
#define APP_AGT0_EVENT                      (0x040U)
#define APP_AGT0_IRQ_PRIORITY               (12U)
#define APP_AGT0_TICK_HZ                    (1000UL)

/* P304 / IRQ9: stopwatch screen, reset and back. */
#define APP_B3_IRQ_SLOT                     (16U)
#define APP_B3_IRQ_EVENT                    (0x00AU)
#define APP_B3_IRQ_CHANNEL                  (9U)
#define APP_B3_IRQ_PRIORITY                 (6U)
#define APP_B3_DEBOUNCE_MS                  (200UL)

/* P111 / IRQ4: stopwatch start, pause and continue. */
#define APP_B4_IRQ_SLOT                     (17U)
#define APP_B4_IRQ_EVENT                    (0x005U)
#define APP_B4_IRQ_CHANNEL                  (4U)
#define APP_B4_IRQ_PRIORITY                 (6U)
#define APP_B4_DEBOUNCE_MS                  (200UL)

/* BLE TINY Click / SCI3. */
#define BLETINY_UART_BAUD                   (57600UL)
#define BLETINY_SCI3_RX_IRQ_SLOT            (18U)
#define BLETINY_SCI3_RX_EVENT               (0x192U)
#define BLETINY_SCI3_ERI_IRQ_SLOT           (19U)
#define BLETINY_SCI3_ERI_EVENT              (0x195U)
#define BLETINY_SCI3_IRQ_PRIORITY           (5U)
#define BLETINY_STATUS_QUERY_MS             (5000UL)
#define BLETINY_MEM_QUERY_MS                (200UL)
#define BLETINY_MEM_FIRST_QUERY_MS          (250UL)
#define BLETINY_RX_BUFFER_SIZE              (256U)
#define BLETINY_LINE_BUFFER_SIZE            (96U)

/* IPS Display 2 Click. */
#define IPS2_SPI_HZ                         (5000000UL)
#define IPS2_WIDTH                          (240U)
#define IPS2_HEIGHT                         (240U)
#define IPS2_STOPWATCH_REFRESH_MS           (16UL)

/* RA4M2 configurable interrupt vector layout. */
#define APP_CORE_VECTOR_COUNT               (16U)
#define APP_EXTERNAL_VECTOR_COUNT           (30U)
#define APP_VECTOR_COUNT                    (APP_CORE_VECTOR_COUNT + APP_EXTERNAL_VECTOR_COUNT)

#endif /* APP_CONFIG_H */
