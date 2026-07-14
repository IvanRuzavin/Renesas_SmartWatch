/**
 * @file bletiny.h
 * @brief Public declarations for this module.
 */

#ifndef BLETINY_H
#define BLETINY_H
#include <stdint.h>
/** @brief Initialize BLE TINY Click over SCI3 UART. */
int bletiny_init(uint32_t pclka_hz, uint32_t now_ms);
/** @brief Parse all bytes currently stored by the SCI3 RX interrupt. */
uint8_t bletiny_process_rx(void);
/** @brief Run periodic GAP status and CodeLess memory polling. */
uint8_t bletiny_service(uint32_t now_ms);
/** @brief Return the current BLE connection state. */
uint8_t bletiny_is_connected(void);
/** @brief Consume a pending BLE connection-state change. */
uint8_t bletiny_take_connection_changed(uint8_t *connected);
/** @brief SCI3 receive-data interrupt handler. */
void BLETINY_SCI3_RXI_IRQHandler(void);
/** @brief SCI3 receive-error interrupt handler. */
void BLETINY_SCI3_ERI_IRQHandler(void);
#endif /* BLETINY_H */
