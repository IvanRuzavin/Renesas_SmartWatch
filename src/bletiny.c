/**
 * @file bletiny.c
 * @brief BLE TINY CodeLess UART transport, connection tracking and remote commands.
 */

#include "bletiny.h"
#include "app_config.h"
#include "app_controller.h"
#include "app_types.h"
#include "ra4m2_baremetal.h"
#include "rtc14.h"

/* SCI3 receive ring and CodeLess session state. */
static volatile uint8_t bletiny_rx_buffer[BLETINY_RX_BUFFER_SIZE];
static volatile uint16_t bletiny_rx_head;
static volatile uint16_t bletiny_rx_tail;
static volatile uint8_t bletiny_rx_overflow;
static volatile uint8_t bletiny_connected;
static volatile uint8_t bletiny_status_dirty = 1U;
static uint32_t bletiny_next_status_query_ms;
static uint32_t bletiny_next_mem_query_ms;
static char bletiny_line_buffer[BLETINY_LINE_BUFFER_SIZE];
static uint16_t bletiny_line_length;

#define BLETINY_RST_MASK (1U << 11) /* P211 */


/**
 * @brief Return the SCI3 register view used by BLE Tiny Click.
 * @return Pointer to the requested register or data view.
 */
static volatile ra4m2_sci_registers_t *bletiny_sci3(void)
{
    return (volatile ra4m2_sci_registers_t *)(uintptr_t)R_SCI3;
}

/**
 * @brief Deassert the BLE Tiny hardware reset signal.
 */
static void bletiny_rst_high(void)
{
    R_PORT2->PODR |= BLETINY_RST_MASK;
}

/**
 * @brief Assert the BLE Tiny hardware reset signal.
 */
static void bletiny_rst_low(void)
{
    R_PORT2->PODR &= (uint16_t)~BLETINY_RST_MASK;
}

/**
 * @brief Configure pins.
 */
static void bletiny_configure_pins(void)
{
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;

    /* P408 = SCI3 RXD, P409 = SCI3 TXD, both alternate function 5. */
    R_PFS->PORT[4].PIN[8].PmnPFS =
        PFS_PMR_PERIPHERAL | PFS_PSEL(5U);
    R_PFS->PORT[4].PIN[9].PmnPFS =
        PFS_PODR_HIGH | PFS_PDR_OUTPUT |
        PFS_PMR_PERIPHERAL | PFS_PSEL(5U);

    /* BLE TINY reset pin on P211. */
    R_PFS->PORT[2].PIN[11].PmnPFS =
        PFS_PODR_HIGH | PFS_PDR_OUTPUT;

    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;
}

/**
 * @brief Select SCI3 baud-rate settings closest to the requested rate.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @param[in] baud_hz Baud hz value.
 * @return Zero on success; otherwise a negative error code.
 */
static int bletiny_sci3_set_baud(uint32_t pclka_hz, uint32_t baud_hz)
{
    typedef struct
    {
        uint16_t divisor;
        uint8_t bgdm;
        uint8_t abcs;
        uint8_t abcse;
        uint8_t cks;
    } bletiny_baud_setting_t;

    static const bletiny_baud_setting_t settings[] =
    {
        {    6U, 0U, 0U, 1U, 0U },
        {    8U, 1U, 1U, 0U, 0U },
        {   16U, 1U, 0U, 0U, 0U },
        {   24U, 0U, 0U, 1U, 1U },
        {   32U, 0U, 0U, 0U, 0U },
        {   64U, 1U, 0U, 0U, 1U },
        {   96U, 0U, 0U, 1U, 2U },
        {  128U, 0U, 0U, 0U, 1U },
        {  256U, 1U, 0U, 0U, 2U },
        {  384U, 0U, 0U, 1U, 3U },
        {  512U, 0U, 0U, 0U, 2U },
        { 1024U, 1U, 0U, 0U, 3U },
        { 2048U, 0U, 0U, 0U, 3U }
    };

    volatile ra4m2_sci_registers_t *sci = bletiny_sci3();
    uint32_t best_error = 0xFFFFFFFFUL;
    uint8_t best_brr = 0U;
    uint8_t best_index = 0U;
    uint8_t found = 0U;
    uint32_t index;

    if ((pclka_hz == 0U) || (baud_hz == 0U))
    {
        return -1;
    }

    for (index = 0U; index < (sizeof(settings) / sizeof(settings[0])); index++)
    {
        uint64_t denominator = (uint64_t)settings[index].divisor * baud_hz;
        uint32_t brr_plus_one;
        uint32_t actual_baud;
        uint32_t error;

        brr_plus_one = (uint32_t)(((uint64_t)pclka_hz +
                                   (denominator / 2ULL)) / denominator);
        if ((brr_plus_one == 0U) || (brr_plus_one > 256U))
        {
            continue;
        }

        actual_baud = pclka_hz /
                      ((uint32_t)settings[index].divisor * brr_plus_one);
        error = (actual_baud > baud_hz) ?
                (actual_baud - baud_hz) : (baud_hz - actual_baud);

        if (error < best_error)
        {
            best_error = error;
            best_brr = (uint8_t)(brr_plus_one - 1U);
            best_index = (uint8_t)index;
            found = 1U;
        }
    }

    if ((found == 0U) ||
        (((uint64_t)best_error * 100ULL) > ((uint64_t)baud_hz * 2ULL)))
    {
        return -1;
    }

    sci->semr &= (uint8_t)~(SCI_SEMR_BGDM | SCI_SEMR_ABCS |
                             SCI_SEMR_ABCSE | SCI_SEMR_BRME);
    if (settings[best_index].bgdm != 0U)
    {
        sci->semr |= SCI_SEMR_BGDM;
    }
    if (settings[best_index].abcs != 0U)
    {
        sci->semr |= SCI_SEMR_ABCS;
    }
    if (settings[best_index].abcse != 0U)
    {
        sci->semr |= SCI_SEMR_ABCSE;
    }

    sci->smr = (uint8_t)((sci->smr & (uint8_t)~SCI_SMR_CKS_MASK) |
                         settings[best_index].cks);
    sci->brr = best_brr;
    return 0;
}

/**
 * @brief Transmit one byte through SCI3.
 * @param[in] value Value to process.
 */
static void bletiny_uart_write_byte(uint8_t value)
{
    volatile ra4m2_sci_registers_t *sci = bletiny_sci3();

    while ((sci->ssr & SCI_SSR_TDRE) == 0U)
    {
    }
    sci->tdr = value;
}

/**
 * @brief Transmit a null-terminated command string through SCI3.
 * @param[in] text Null-terminated text string.
 */
static void bletiny_uart_write_text(const char *text)
{
    while (*text != '\0')
    {
        bletiny_uart_write_byte((uint8_t)*text);
        text++;
    }

    while ((bletiny_sci3()->ssr & SCI_SSR_TEND) == 0U)
    {
    }
}

/**
 * @brief Send a CodeLess command terminated with carriage return.
 * @param[in] command CodeLess or display command string/value.
 */
static void bletiny_send_command(const char *command)
{
    bletiny_uart_write_text(command);
    bletiny_uart_write_byte('\r');

    while ((bletiny_sci3()->ssr & SCI_SSR_TEND) == 0U)
    {
    }
}

/**
 * @brief Append one received byte to the interrupt-safe ring buffer.
 * @param[in] value Value to process.
 */
static void bletiny_rx_push(uint8_t value)
{
    uint16_t next = (uint16_t)((bletiny_rx_head + 1U) %
                               BLETINY_RX_BUFFER_SIZE);

    if (next != bletiny_rx_tail)
    {
        bletiny_rx_buffer[bletiny_rx_head] = value;
        bletiny_rx_head = next;
    }
    else
    {
        bletiny_rx_overflow = 1U;
    }
}

/**
 * @brief Remove one byte from the receive ring buffer.
 * @param[in] value Value to process.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static int bletiny_rx_pop(uint8_t *value)
{
    uint32_t primask;

    if (value == 0)
    {
        return 0;
    }

    primask = __get_PRIMASK();
    __disable_irq();

    if (bletiny_rx_tail == bletiny_rx_head)
    {
        if ((primask & 1U) == 0U)
        {
            __enable_irq();
        }
        return 0;
    }

    *value = bletiny_rx_buffer[bletiny_rx_tail];
    bletiny_rx_tail = (uint16_t)((bletiny_rx_tail + 1U) %
                                 BLETINY_RX_BUFFER_SIZE);

    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
    return 1;
}

/**
 * @brief Test whether a received response line contains a token.
 * @param[in] line Null-terminated received response line.
 * @param[in] token Token to locate or match.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t bletiny_line_contains(const char *line, const char *token)
{
    uint16_t start;
    uint16_t token_length = 0U;

    while (token[token_length] != '\0')
    {
        token_length++;
    }

    if (token_length == 0U)
    {
        return 1U;
    }

    for (start = 0U; line[start] != '\0'; start++)
    {
        uint16_t index = 0U;
        while ((index < token_length) &&
               (line[start + index] != '\0') &&
               (line[start + index] == token[index]))
        {
            index++;
        }

        if (index == token_length)
        {
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief Locate a token inside a received response line.
 * @param[in] line Null-terminated received response line.
 * @param[in] token Token to locate or match.
 * @param[in] token_index Token index value.
 * @return Result value.
 */
static uint8_t bletiny_find_token(const char *line, const char *token,
                                   uint16_t *token_index)
{
    uint16_t start;
    uint16_t token_length = 0U;

    if ((line == 0) || (token == 0) || (token_index == 0))
    {
        return 0U;
    }

    while (token[token_length] != '\0')
    {
        token_length++;
    }

    for (start = 0U; line[start] != '\0'; start++)
    {
        uint16_t index = 0U;

        while ((index < token_length) &&
               (line[start + index] != '\0') &&
               (line[start + index] == token[index]))
        {
            index++;
        }

        if (index == token_length)
        {
            *token_index = start;
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief Parse one unsigned decimal field from a command payload.
 * @param[in] cursor Current parser position.
 * @param[in] value Value to process.
 * @param[in] expect_separator Expect separator value.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
static uint8_t bletiny_parse_uint_field(const char **cursor,
                                        uint32_t *value,
                                        uint8_t expect_separator)
{
    const char *position;
    uint32_t parsed = 0U;
    uint8_t digits = 0U;

    if ((cursor == 0) || (*cursor == 0) || (value == 0))
    {
        return 0U;
    }

    position = *cursor;
    while ((*position >= '0') && (*position <= '9'))
    {
        if (parsed > 1000000UL)
        {
            return 0U;
        }

        parsed = (parsed * 10UL) + (uint32_t)(*position - '0');
        position++;
        digits++;
    }

    if (digits == 0U)
    {
        return 0U;
    }

    if (expect_separator != 0U)
    {
        if (*position != '|')
        {
            return 0U;
        }
        position++;
    }
    else if (*position != '\0')
    {
        return 0U;
    }

    *cursor = position;
    *value = parsed;
    return 1U;
}

/**
 * @brief Validate and apply an RTC command received through CodeLess memory.
 * @param[in] line Null-terminated received response line.
 * @return Result value.
 */
static uint8_t bletiny_process_rtc_command(const char *line)
{
    uint16_t command_index;
    const char *cursor;
    uint32_t fields[7];
    uint8_t field;
    int result;

    if (bletiny_find_token(line, "RTC|", &command_index) == 0U)
    {
        return 0U;
    }

    cursor = &line[command_index + 4U];
    for (field = 0U; field < 7U; field++)
    {
        if (bletiny_parse_uint_field(&cursor, &fields[field],
                                     (field < 6U) ? 1U : 0U) == 0U)
        {
            printf_me("BLE RTC command has invalid syntax: %s\r\n", line);
            bletiny_send_command("AT+MEM=0,EMPTY");
            return 1U;
        }
    }

    /* Reject out-of-range values before narrowing them to RTC field sizes. */
    if ((fields[0] < 2000UL) || (fields[0] > 2099UL) ||
        (fields[1] < 1UL) || (fields[1] > 12UL) ||
        (fields[2] < 1UL) || (fields[2] > 31UL) ||
        (fields[3] > 23UL) || (fields[4] > 59UL) ||
        (fields[5] > 59UL) || (fields[6] > 6UL))
    {
        printf_me("BLE RTC command contains an invalid date/time: %s\r\n",
                  line);
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    result = rtc14_set_datetime_24h((uint16_t)fields[0],
                                    (uint8_t)fields[1],
                                    (uint8_t)fields[2],
                                    (uint8_t)fields[3],
                                    (uint8_t)fields[4],
                                    (uint8_t)fields[5],
                                    (uint8_t)fields[6]);

    if (result == RTC14_OK)
    {
        printf_me("RTC14 synchronized over BLE: "
                  "%04u-%02u-%02u %02u:%02u:%02u weekday=%u\r\n",
                  (unsigned int)fields[0],
                  (unsigned int)fields[1],
                  (unsigned int)fields[2],
                  (unsigned int)fields[3],
                  (unsigned int)fields[4],
                  (unsigned int)fields[5],
                  (unsigned int)fields[6]);

        /* Force an immediate fresh RTC read and active-screen repaint. */
        app_controller_request_rtc_refresh();

        /* Mark the slot consumed so the same value is not applied repeatedly. */
        bletiny_send_command("AT+MEM=0,EMPTY");
    }
    else if (result == RTC14_ERROR_ARGUMENT)
    {
        printf_me("BLE RTC command contains an invalid date/time: %s\r\n",
                  line);
        bletiny_send_command("AT+MEM=0,EMPTY");
    }
    else
    {
        /* Keep the command in memory so a transient I2C error can be retried. */
        printf_me("RTC14 BLE synchronization failed: %d\r\n", result);
    }

    return 1U;
}

/**
 * @brief Decode BLE screen and stopwatch trigger commands.
 * @param[in] line Null-terminated received response line.
 * @return Result value.
 */
static uint8_t bletiny_process_stopwatch_command(const char *line)
{
    uint16_t command_index;

    /*
     * SmartConsole stores one of these payloads in CodeLess memory slot 0:
     *
     *   SCREEN|B1     - show the smartwatch dashboard
     *   SCREEN|B2     - show the animated temperature/humidity screen
     *   SCREEN|B5     - show the astronomical time-of-day screen
     *   STOPWATCH|B3  - stop and reset the stopwatch
     *   STOPWATCH|B4  - show/start/pause/continue the stopwatch
     *
     * STOPWATCH|B1, STOPWATCH|B2 and STOPWATCH|B5 are accepted as aliases
     * for SmartConsole layouts that use a common command prefix.
     */
    if ((bletiny_find_token(line, "SCREEN|B1", &command_index) != 0U) ||
        (bletiny_find_token(line, "STOPWATCH|B1", &command_index) != 0U))
    {
        (void)command_index;
        app_controller_request_dashboard_screen();
        printf_me("BLE screen trigger: B1 (smartwatch dashboard)\r\n");
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    if ((bletiny_find_token(line, "SCREEN|B2", &command_index) != 0U) ||
        (bletiny_find_token(line, "STOPWATCH|B2", &command_index) != 0U))
    {
        (void)command_index;
        app_controller_request_environment_screen();
        printf_me("BLE screen trigger: B2 (temperature/humidity)\r\n");
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    if ((bletiny_find_token(line, "SCREEN|B5", &command_index) != 0U) ||
        (bletiny_find_token(line, "STOPWATCH|B5", &command_index) != 0U))
    {
        (void)command_index;
        app_controller_request_astronomy_screen();
        printf_me("BLE screen trigger: B5 (time of day)\r\n");
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    if (bletiny_find_token(line, "STOPWATCH|B3", &command_index) != 0U)
    {
        (void)command_index;
        app_controller_request_stopwatch_reset();
        printf_me("BLE stopwatch trigger: B3 (stop/reset)\r\n");
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    if (bletiny_find_token(line, "STOPWATCH|B4", &command_index) != 0U)
    {
        (void)command_index;
        app_controller_request_stopwatch_control();
        printf_me("BLE stopwatch trigger: B4 (show/control)\r\n");
        bletiny_send_command("AT+MEM=0,EMPTY");
        return 1U;
    }

    return 0U;
}

/**
 * @brief Update the cached BLE connection state and change flag.
 * @param[in] connected BLE connection state or destination for that state.
 */
static void bletiny_set_connected(uint8_t connected)
{
    connected = (connected != 0U) ? 1U : 0U;
    if (bletiny_connected != connected)
    {
        bletiny_connected = connected;
        bletiny_status_dirty = 1U;

        if (connected != 0U)
        {
            /* Read SmartConsole memory shortly after the link is established. */
            bletiny_next_mem_query_ms =
                app_controller_get_system_ms() + BLETINY_MEM_FIRST_QUERY_MS;
        }
    }
}

/**
 * @brief Parse one complete SCI3 response line.
 * @param[in] line Null-terminated received response line.
 */
static void bletiny_process_line(const char *line)
{
    uint16_t index;

    if (bletiny_process_rtc_command(line) != 0U)
    {
        return;
    }

    if (bletiny_process_stopwatch_command(line) != 0U)
    {
        return;
    }

    /* DISCONNECTED must be checked first because it contains CONNECTED. */
    if (bletiny_line_contains(line, "+DISCONNECTED") != 0U)
    {
        bletiny_set_connected(0U);
        return;
    }

    if (bletiny_line_contains(line, "+CONNECTED") != 0U)
    {
        bletiny_set_connected(1U);
        return;
    }

    /* AT+GAPSTATUS replies with role,connection, for example 0,1. */
    for (index = 0U;
         (line[index] != '\0') &&
         (line[index + 1U] != '\0') &&
         (line[index + 2U] != '\0');
         index++)
    {
        if (((line[index] == '0') || (line[index] == '1')) &&
            (line[index + 1U] == ',') &&
            ((line[index + 2U] == '0') || (line[index + 2U] == '1')))
        {
            bletiny_set_connected((line[index + 2U] == '1') ? 1U : 0U);
            return;
        }
    }
}

/**
 * @brief Process rx.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
uint8_t bletiny_process_rx(void)
{
    uint8_t value;
    uint8_t processed = 0U;

    if (bletiny_rx_overflow != 0U)
    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        bletiny_rx_overflow = 0U;
        bletiny_rx_tail = bletiny_rx_head;
        if ((primask & 1U) == 0U)
        {
            __enable_irq();
        }
        bletiny_line_length = 0U;
        printf_me("BLE TINY UART receive buffer overflow.\r\n");
        processed = 1U;
    }

    while (bletiny_rx_pop(&value) != 0)
    {
        processed = 1U;

        if ((value == '\r') || (value == '\n'))
        {
            if (bletiny_line_length != 0U)
            {
                bletiny_line_buffer[bletiny_line_length] = '\0';
                bletiny_process_line(bletiny_line_buffer);
                bletiny_line_length = 0U;
            }
        }
        else if ((value >= 0x20U) && (value <= 0x7EU))
        {
            if (bletiny_line_length < (BLETINY_LINE_BUFFER_SIZE - 1U))
            {
                bletiny_line_buffer[bletiny_line_length++] = (char)value;
            }
            else
            {
                bletiny_line_length = 0U;
            }
        }
    }

    return processed;
}

/**
 * @brief Initialize the module.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @param[in] now_ms Current monotonic application time in milliseconds.
 * @return Zero on success; otherwise a negative error code.
 */
int bletiny_init(uint32_t pclka_hz, uint32_t now_ms)
{
    volatile ra4m2_sci_registers_t *sci = bletiny_sci3();
    IRQn_Type rx_irq = (IRQn_Type)BLETINY_SCI3_RX_IRQ_SLOT;
    IRQn_Type eri_irq = (IRQn_Type)BLETINY_SCI3_ERI_IRQ_SLOT;
    volatile uint32_t wait;

    bletiny_configure_pins();
    bletiny_rst_high();

    /* Start SCI3: MSTPCRB.MSTPB28 = 0. */
    R_MSTP->MSTPCRB &= ~(1UL << 28);
    (void)R_MSTP->MSTPCRB;

    NVIC_DisableIRQ(rx_irq);
    NVIC_DisableIRQ(eri_irq);
    NVIC_ClearPendingIRQ(rx_irq);
    NVIC_ClearPendingIRQ(eri_irq);
    R_ICU->IELSR[BLETINY_SCI3_RX_IRQ_SLOT] = 0U;
    R_ICU->IELSR[BLETINY_SCI3_ERI_IRQ_SLOT] = 0U;

    sci->scr = 0U;
    sci->smr = 0U;
    sci->scmr &= (uint8_t)~(SCI_SCMR_SMIF | SCI_SCMR_SINV | SCI_SCMR_SDIR);
    sci->scmr |= SCI_SCMR_CHR1;  /* 8 data bits. */
    sci->smr &= (uint8_t)~(SCI_SMR_STOP | SCI_SMR_PM |
                            SCI_SMR_PE | SCI_SMR_CHR | SCI_SMR_CM);
    sci->semr = 0U;
    sci->snfr = 0U;
    sci->simr1 &= (uint8_t)~SCI_SIMR1_IICM;
    sci->fcr &= (uint16_t)~SCI_FCR_FM;
    sci->spmr = 0U;

    if (bletiny_sci3_set_baud(pclka_hz, BLETINY_UART_BAUD) != 0)
    {
        return -1;
    }

    /* Allow at least one bit period after writing BRR. */
    for (wait = 0U; wait < 1000U; wait++)
    {
        __asm volatile ("nop");
    }

    sci->ssr &= (uint8_t)~(SCI_SSR_ORER | SCI_SSR_FER | SCI_SSR_PER);

    R_ICU->IELSR[BLETINY_SCI3_RX_IRQ_SLOT] = BLETINY_SCI3_RX_EVENT;
    R_ICU->IELSR[BLETINY_SCI3_ERI_IRQ_SLOT] = BLETINY_SCI3_ERI_EVENT;

    NVIC_SetPriority(rx_irq, BLETINY_SCI3_IRQ_PRIORITY);
    NVIC_SetPriority(eri_irq, BLETINY_SCI3_IRQ_PRIORITY);
    NVIC_ClearPendingIRQ(rx_irq);
    NVIC_ClearPendingIRQ(eri_irq);
    NVIC_EnableIRQ(rx_irq);
    NVIC_EnableIRQ(eri_irq);

    sci->scr = (uint8_t)(SCI_SCR_TE | SCI_SCR_RE | SCI_SCR_RIE);

    /* Reset sequence used by the supplied BLE TINY Click driver. */
    bletiny_rst_high();
    Delay_100ms();
    bletiny_rst_low();
    Delay_1sec();

    bletiny_connected = 0U;
    bletiny_status_dirty = 1U;

    /*
     * Check CodeLess, enable immediate connection/disconnection events,
     * ensure advertising, then query the initial GAP state.
     */
    bletiny_send_command("AT");
    Delay_100ms();
    bletiny_send_command("AT+EVENT=2,1");
    Delay_100ms();
    bletiny_send_command("AT+EVENT=3,1");
    Delay_100ms();
    bletiny_send_command("AT+ADVSTART");
    Delay_100ms();
    bletiny_send_command("AT+GAPSTATUS");
    bletiny_next_status_query_ms = now_ms + BLETINY_STATUS_QUERY_MS;
    bletiny_next_mem_query_ms = now_ms + BLETINY_MEM_QUERY_MS;

    return 0;
}

/**
 * @brief Store a received SCI3 byte in the BLE ring buffer.
 */
void BLETINY_SCI3_RXI_IRQHandler(void)
{
    volatile ra4m2_sci_registers_t *sci = bletiny_sci3();
    uint8_t value = sci->rdr;

    bletiny_rx_push(value);
    R_ICU->IELSR[BLETINY_SCI3_RX_IRQ_SLOT] &= ~ICU_IELSR_IR;
}

/**
 * @brief Clear SCI3 receive errors and preserve any pending byte.
 */
void BLETINY_SCI3_ERI_IRQHandler(void)
{
    volatile ra4m2_sci_registers_t *sci = bletiny_sci3();

    if ((sci->ssr & SCI_SSR_RDRF) != 0U)
    {
        bletiny_rx_push(sci->rdr);
    }

    sci->ssr &= (uint8_t)~(SCI_SSR_ORER | SCI_SSR_FER | SCI_SSR_PER);
    R_ICU->IELSR[BLETINY_SCI3_ERI_IRQ_SLOT] &= ~ICU_IELSR_IR;
}


/**
 * @brief Determine whether connected.
 * @return Result value.
 */
uint8_t bletiny_is_connected(void)
{
    return (bletiny_connected != 0U) ? 1U : 0U;
}

/**
 * @brief Consume connection changed.
 * @param[out] connected BLE connection state or destination for that state.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
uint8_t bletiny_take_connection_changed(uint8_t *connected)
{
    uint32_t primask;

    if ((connected == 0) || (bletiny_status_dirty == 0U))
    {
        return 0U;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    *connected = bletiny_connected;
    bletiny_status_dirty = 0U;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    return 1U;
}

/**
 * @brief Service periodic module work.
 * @param[in] now_ms Current monotonic application time in milliseconds.
 * @return Nonzero when the requested work succeeds or changes state; otherwise zero.
 */
uint8_t bletiny_service(uint32_t now_ms)
{
    uint8_t did_work = 0U;

    if ((int32_t)(now_ms - bletiny_next_status_query_ms) >= 0)
    {
        bletiny_send_command("AT+GAPSTATUS");
        bletiny_next_status_query_ms = now_ms + BLETINY_STATUS_QUERY_MS;
        did_work = 1U;
    }

    if ((bletiny_connected != 0U) &&
        ((int32_t)(now_ms - bletiny_next_mem_query_ms) >= 0))
    {
        bletiny_send_command("AT+MEM=0");
        bletiny_next_mem_query_ms = now_ms + BLETINY_MEM_QUERY_MS;
        did_work = 1U;
    }

    return did_work;
}
