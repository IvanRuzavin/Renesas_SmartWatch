/**
 * @file app_interrupts.h
 * @brief Public declarations for this module.
 */

#ifndef APP_INTERRUPTS_H
#define APP_INTERRUPTS_H
#include <stdint.h>
/** @brief Install RAM vectors and initialize AGT0, IRQ9 and IRQ4. */
int app_interrupts_init(uint32_t pclkb_hz);
/** @brief AGT0 1 ms underflow interrupt handler. */
void AGT0_IRQHandler(void);
/** @brief P304 / IRQ9 B3 interrupt handler. */
void P304_IRQ9_IRQHandler(void);
/** @brief P111 / IRQ4 B4 interrupt handler. */
void P111_IRQ4_IRQHandler(void);
#endif /* APP_INTERRUPTS_H */
