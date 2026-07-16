/**
 * @file bletiny.h
 * @brief BLE Tiny Click SCI3 and CodeLess command interface.
 */

#ifndef BLETINY_H
#define BLETINY_H

#include <stdint.h>

/**
 * @brief Initialize BLE Tiny Click over SCI3 UART.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @param[in] now_ms Current application time in milliseconds.
 * @return Zero on success; otherwise a negative error code.
 */
int bletiny_init(uint32_t pclka_hz, uint32_t now_ms);

/**
 * @brief Parse all bytes currently stored by the SCI3 receive interrupt.
 * @return Nonzero when at least one complete response line was processed.
 */
uint8_t bletiny_process_rx(void);

/**
 * @brief Run periodic GAP-status and CodeLess-memory polling.
 * @param[in] now_ms Current application time in milliseconds.
 * @return Nonzero when a command was transmitted or processed.
 */
uint8_t bletiny_service(uint32_t now_ms);

/**
 * @brief Return the current BLE connection state.
 * @return Nonzero while connected; otherwise zero.
 */
uint8_t bletiny_is_connected(void);

/**
 * @brief Consume a pending BLE connection-state change.
 * @param[out] connected Destination for the latest connection state.
 * @return Nonzero when a pending state change was consumed.
 */
uint8_t bletiny_take_connection_changed(uint8_t *connected);

/** @brief SCI3 receive-data interrupt handler. */
void BLETINY_SCI3_RXI_IRQHandler(void);

/** @brief SCI3 receive-error interrupt handler. */
void BLETINY_SCI3_ERI_IRQHandler(void);

#endif /* BLETINY_H */
