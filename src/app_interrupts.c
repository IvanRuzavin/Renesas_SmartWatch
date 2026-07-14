/**
 * @file app_interrupts.c
 * @brief RAM vector installation, AGT0 tick and external button IRQ routing.
 */

#include "app_interrupts.h"
#include "app_config.h"
#include "app_controller.h"
#include "app_types.h"
#include "bletiny.h"
#include "ra4m2_baremetal.h"

static uintptr_t app_ram_vector_table[APP_VECTOR_COUNT] __attribute__((aligned(256)));

/* -------------------------------------------------------------------------- */
/* 1 ms AGT0 tick plus P304/IRQ9 and P111/IRQ4 controls                      */
/* -------------------------------------------------------------------------- */

static volatile ra4m2_agt16_registers_t *rtc14_agt0_registers(void)
{
    return (volatile ra4m2_agt16_registers_t *)(uintptr_t)R_AGT0;
}


static void rtc14_install_interrupt_vectors(void)
{
    uint32_t index;
    uint32_t primask = __get_PRIMASK();
    const uintptr_t *active_vector_table =
        (const uintptr_t *)(uintptr_t)SCB->VTOR;

    __disable_irq();

    for (index = 0U; index < APP_VECTOR_COUNT; index++)
    {
        app_ram_vector_table[index] = active_vector_table[index];
    }

    app_ram_vector_table[APP_CORE_VECTOR_COUNT + APP_AGT0_IRQ_SLOT] =
        (uintptr_t)AGT0_IRQHandler;
    app_ram_vector_table[APP_CORE_VECTOR_COUNT + APP_B3_IRQ_SLOT] =
        (uintptr_t)P304_IRQ9_IRQHandler;
    app_ram_vector_table[APP_CORE_VECTOR_COUNT + APP_B4_IRQ_SLOT] =
        (uintptr_t)P111_IRQ4_IRQHandler;
    app_ram_vector_table[APP_CORE_VECTOR_COUNT + BLETINY_SCI3_RX_IRQ_SLOT] =
        (uintptr_t)BLETINY_SCI3_RXI_IRQHandler;
    app_ram_vector_table[APP_CORE_VECTOR_COUNT + BLETINY_SCI3_ERI_IRQ_SLOT] =
        (uintptr_t)BLETINY_SCI3_ERI_IRQHandler;

    SCB->VTOR = (uint32_t)(uintptr_t)app_ram_vector_table;
    __DSB();
    __ISB();

    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
}


static int rtc14_agt0_select_1ms_clock(uint32_t pclkb_hz,
                                       uint8_t *agtmr1,
                                       uint16_t *reload)
{
    static const uint8_t tck_value[3] =
    {
        AGT_AGTMR1_TCK_PCLKB_DIV_1,
        AGT_AGTMR1_TCK_PCLKB_DIV_2,
        AGT_AGTMR1_TCK_PCLKB_DIV_8
    };
    static const uint8_t divisor[3] = { 1U, 2U, 8U };
    uint32_t index;

    if ((agtmr1 == 0) || (reload == 0) || (pclkb_hz == 0U))
    {
        return RTC14_ERROR_ARGUMENT;
    }

    for (index = 0U; index < 3U; index++)
    {
        uint32_t source_hz = pclkb_hz / divisor[index];
        uint32_t counts = (source_hz + (APP_AGT0_TICK_HZ / 2U)) /
                          APP_AGT0_TICK_HZ;

        if ((counts != 0U) && (counts <= 65535U))
        {
            *agtmr1 = tck_value[index];
            *reload = (uint16_t)(counts - 1U);
            return RTC14_OK;
        }
    }

    return RTC14_ERROR_CLOCK;
}


static int rtc14_agt0_init(uint32_t pclkb_hz)
{
    volatile ra4m2_agt16_registers_t *agt = rtc14_agt0_registers();
    uint32_t timeout = APP_I2C_TIMEOUT;
    IRQn_Type irq = (IRQn_Type)APP_AGT0_IRQ_SLOT;
    uint8_t agtmr1;
    uint16_t reload;
    int result;

    result = rtc14_agt0_select_1ms_clock(pclkb_hz, &agtmr1, &reload);
    if (result != RTC14_OK)
    {
        return result;
    }

    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    R_ICU->IELSR[APP_AGT0_IRQ_SLOT] = 0U;

    /* Start AGT0: MSTPCRD.MSTPD3 = 0. */
    R_MSTP->MSTPCRD &= ~(1UL << AGT0_MSTPCRD_BIT);
    (void)R_MSTP->MSTPCRD;

    agt->AGTCR = 0U;
    while ((agt->AGTCR & AGT_AGTCR_TCSTF) != 0U)
    {
        if (timeout-- == 0U)
        {
            return RTC14_ERROR_TIMEOUT;
        }
    }

    /* Periodic mode using PCLKB / 1, /2 or /8 for an accurate 1 ms tick. */
    agt->AGTMR2 = 0U;
    agt->AGTMR1 = agtmr1;
    agt->AGTIOC = 0U;
    agt->AGTISR = 0U;
    agt->AGTCMSR = 0U;
    agt->AGTIOSEL = 0U;
    agt->AGTCMA = 0U;
    agt->AGTCMB = 0U;
    agt->AGT = reload;

    R_ICU->IELSR[APP_AGT0_IRQ_SLOT] = APP_AGT0_EVENT;

    NVIC_SetPriority(irq, APP_AGT0_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);

    agt->AGTCR = AGT_AGTCR_START_TIMER;

    timeout = APP_I2C_TIMEOUT;
    while ((agt->AGTCR & AGT_AGTCR_TCSTF) == 0U)
    {
        if (timeout-- == 0U)
        {
            NVIC_DisableIRQ(irq);
            return RTC14_ERROR_TIMEOUT;
        }
    }

    return RTC14_OK;
}


static void rtc14_p304_irq_init(void)
{
    IRQn_Type irq = (IRQn_Type)APP_B3_IRQ_SLOT;

    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    R_ICU->IELSR[APP_B3_IRQ_SLOT] = 0U;

    /* P304 as IRQ9 input, active-low button with internal pull-up. */
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;
    R_PFS->PORT[3].PIN[4].PmnPFS = PFS_PCR_PULLUP | PFS_ISEL_IRQ;
    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;

    /* IRQ9 falling edge. IELSR must be disconnected while IRQCR is changed. */
    R_ICU->IRQCR[APP_B3_IRQ_CHANNEL] = ICU_IRQCR_FALLING_EDGE;
    R_ICU->IELSR[APP_B3_IRQ_SLOT] = APP_B3_IRQ_EVENT;

    NVIC_SetPriority(irq, APP_B3_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);
}


static void rtc14_p111_irq_init(void)
{
    IRQn_Type irq = (IRQn_Type)APP_B4_IRQ_SLOT;

    NVIC_DisableIRQ(irq);
    NVIC_ClearPendingIRQ(irq);
    R_ICU->IELSR[APP_B4_IRQ_SLOT] = 0U;

    /* P111 as IRQ4 input, active-low button with internal pull-up. */
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;
    R_PFS->PORT[1].PIN[11].PmnPFS = PFS_PCR_PULLUP | PFS_ISEL_IRQ;
    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;

    /* IRQ4 falling edge. IELSR must be disconnected while IRQCR is changed. */
    R_ICU->IRQCR[APP_B4_IRQ_CHANNEL] = ICU_IRQCR_FALLING_EDGE;
    R_ICU->IELSR[APP_B4_IRQ_SLOT] = APP_B4_IRQ_EVENT;

    NVIC_SetPriority(irq, APP_B4_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(irq);
    NVIC_EnableIRQ(irq);
}


void AGT0_IRQHandler(void)
{
    volatile ra4m2_agt16_registers_t *agt = rtc14_agt0_registers();
    uint8_t agtcr = agt->AGTCR;

    R_ICU->IELSR[APP_AGT0_IRQ_SLOT] &= ~ICU_IELSR_IR;
    agt->AGTCR = (uint8_t)(agtcr & (uint8_t)~AGT_AGTCR_STATUS_FLAGS);
    app_controller_tick_1ms_isr();
}


void P304_IRQ9_IRQHandler(void)
{
    R_ICU->IELSR[APP_B3_IRQ_SLOT] &= ~ICU_IELSR_IR;
    app_controller_b3_button_isr();
}


void P111_IRQ4_IRQHandler(void)
{
    R_ICU->IELSR[APP_B4_IRQ_SLOT] &= ~ICU_IELSR_IR;
    app_controller_b4_button_isr();
}


int app_interrupts_init(uint32_t pclkb_hz)
{
    int result;

    rtc14_install_interrupt_vectors();
    result = rtc14_agt0_init(pclkb_hz);
    if (result != RTC14_OK)
    {
        return result;
    }

    rtc14_p304_irq_init();
    rtc14_p111_irq_init();
    return RTC14_OK;
}
