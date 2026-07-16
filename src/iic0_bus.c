/**
 * @file iic0_bus.c
 * @brief Polling IIC0 master transport shared by RTC 14 and Temp&Hum 27.
 */

#include "iic0_bus.h"
#include "app_config.h"
#include "app_types.h"
#include "ra4m2_baremetal.h"

/**
 * @brief Wait until all requested register bits become set.
 * @param[in] reg Register containing the bits to test.
 * @param[in] mask Bit mask to test or modify.
 * @return Result value.
 */
static int iic0_wait_set(volatile uint8_t *reg, uint8_t mask)
{
    uint32_t timeout = APP_I2C_TIMEOUT;

    while (((*reg) & mask) == 0U)
    {
        if (timeout-- == 0U)
        {
            return RTC14_ERROR_TIMEOUT;
        }
    }

    return RTC14_OK;
}


/**
 * @brief Wait until all requested register bits become clear.
 * @param[in] reg Register containing the bits to test.
 * @param[in] mask Bit mask to test or modify.
 * @return Result value.
 */
static int iic0_wait_clear(volatile uint8_t *reg, uint8_t mask)
{
    uint32_t timeout = APP_I2C_TIMEOUT;

    while (((*reg) & mask) != 0U)
    {
        if (timeout-- == 0U)
        {
            return RTC14_ERROR_TIMEOUT;
        }
    }

    return RTC14_OK;
}


/**
 * @brief Wait for the next IIC transmit-ready or completion flag.
 * @param[in] mask Bit mask to test or modify.
 * @return Result value.
 */
static int iic0_wait_transmit_flag(uint8_t mask)
{
    uint32_t timeout = APP_I2C_TIMEOUT;

    while ((R_IIC0->ICSR2 & mask) == 0U)
    {
        if ((R_IIC0->ICSR2 & IIC_ICSR2_NACKF) != 0U)
        {
            return RTC14_ERROR_NACK;
        }

        if (timeout-- == 0U)
        {
            return RTC14_ERROR_TIMEOUT;
        }
    }

    return ((R_IIC0->ICSR2 & IIC_ICSR2_NACKF) != 0U) ?
           RTC14_ERROR_NACK : RTC14_OK;
}


/**
 * @brief Configure the ACK/NACK response used for the next received byte.
 * @param[in] nack Nonzero to send NACK after the next received byte.
 */
static void iic0_set_receive_nack(uint8_t nack)
{
    R_IIC0->ICMR3 |= IIC_ICMR3_ACKWP;

    if (nack != 0U)
    {
        R_IIC0->ICMR3 |= IIC_ICMR3_ACKBT;
    }
    else
    {
        R_IIC0->ICMR3 &= (uint8_t)~IIC_ICMR3_ACKBT;
    }

    R_IIC0->ICMR3 &= (uint8_t)~IIC_ICMR3_ACKWP;
}


/**
 * @brief Clear receive flags before starting an IIC read phase.
 */
static void iic0_clear_receive_state(void)
{
    R_IIC0->ICMR3 &= (uint8_t)~IIC_ICMR3_WAIT;
    iic0_set_receive_nack(0U);
    R_IIC0->ICSR2 &= (uint8_t)~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
}


/**
 * @brief Generate an IIC STOP condition and wait for bus release.
 * @return Result value.
 */
static int iic0_request_stop(void)
{
    R_IIC0->ICSR2 &= (uint8_t)~IIC_ICSR2_STOP;
    R_IIC0->ICCR2 |= IIC_ICCR2_SP;

    if (iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_STOP) != RTC14_OK)
    {
        return RTC14_ERROR_TIMEOUT;
    }

    R_IIC0->ICSR2 &= (uint8_t)~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
    return RTC14_OK;
}


/* -------------------------------------------------------------------------- */
/* Clock and pin configuration                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Configure pins.
 */
static void iic0_configure_pins(void)
{
    /* Unlock PFS writes. */
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;

    /*
     * P400: IIC0 SCL, PSEL 7.
     * P401: IIC0 SDA, PSEL 7.
     * Start both lines high, enable pull-ups and open-drain peripheral mode.
     */
    R_PFS->PORT[4].PIN[0].PmnPFS =
        PFS_PODR_HIGH |
        PFS_PDR_OUTPUT |
        PFS_PCR_PULLUP |
        PFS_NCODR_OPEN_DRAIN |
        PFS_PMR_PERIPHERAL |
        PFS_PSEL(7U);

    R_PFS->PORT[4].PIN[1].PmnPFS =
        PFS_PODR_HIGH |
        PFS_PDR_OUTPUT |
        PFS_PCR_PULLUP |
        PFS_NCODR_OPEN_DRAIN |
        PFS_PMR_PERIPHERAL |
        PFS_PSEL(7U);

    /* RTC 14 EIN on P610: ordinary GPIO output, held low. */
    R_PFS->PORT[6].PIN[10].PmnPFS = PFS_PDR_OUTPUT;

    /* Lock PFS writes. */
    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;
}


/**
 * @brief Calculate and apply IIC0 clock timing registers.
 * @param[in] pclkb_hz Peripheral clock B frequency in hertz.
 * @param[in] bitrate_hz Bitrate hz value.
 * @return Zero on success; otherwise a negative error code.
 */
static int iic0_configure_bitrate(uint32_t pclkb_hz, uint32_t bitrate_hz)
{
    uint8_t cks;
    const uint8_t noise_filter_stages = 4U;
    const uint8_t minimum_br = noise_filter_stages + 1U;

    R_IIC0->ICFER |= (IIC_ICFER_SCLE | IIC_ICFER_NFE);
    R_IIC0->ICMR3 = (uint8_t)((R_IIC0->ICMR3 & (uint8_t)~IIC_ICMR3_NF_MASK) |
                              IIC_ICMR3_NF_MASK);

    for (cks = 0U; cks <= 7U; cks++)
    {
        uint32_t iic_clock_hz = pclkb_hz >> cks;
        uint32_t constant = (cks == 0U) ?
                            (3U + noise_filter_stages) :
                            (2U + noise_filter_stages);
        uint32_t clocks_per_bit;
        uint32_t br_sum;
        uint32_t brl;
        uint32_t brh;
        uint32_t actual_bitrate;
        uint32_t error;

        if (iic_clock_hz < bitrate_hz)
        {
            continue;
        }

        clocks_per_bit = iic_clock_hz / bitrate_hz;
        if (clocks_per_bit <= (2U * constant))
        {
            continue;
        }

        br_sum = clocks_per_bit - (2U * constant);

        if ((br_sum < (2U * minimum_br)) || (br_sum > 62U))
        {
            continue;
        }

        brl = br_sum / 2U;
        brh = br_sum - brl;

        if ((brl > 31U) || (brh > 31U))
        {
            continue;
        }

        actual_bitrate = iic_clock_hz / (br_sum + (2U * constant));
        error = (actual_bitrate > bitrate_hz) ?
                (actual_bitrate - bitrate_hz) :
                (bitrate_hz - actual_bitrate);

        /* Same 10% maximum error criterion used by the supplied SDK layer. */
        if (((uint64_t)error * 10ULL) > bitrate_hz)
        {
            continue;
        }

        R_IIC0->ICMR1 = (uint8_t)(cks << IIC_ICMR1_CKS_SHIFT);
        R_IIC0->ICBRL = (uint8_t)brl;
        R_IIC0->ICBRH = (uint8_t)brh;
        return RTC14_OK;
    }

    return RTC14_ERROR_CLOCK;
}


/**
 * @brief Handle bus init.
 * @return Zero on success; otherwise a negative error code.
 */
int iic0_bus_init(void)
{
    int result;
    system_clocks_t system_clocks;
    SYSTEM_GetClocksFrequency( &system_clocks );

    /* Start IIC0: MSTPCRB.MSTPB9 = 0. */
    R_MSTP->MSTPCRB &= ~(1UL << 9);
    (void)R_MSTP->MSTPCRB;

    /* Enter internal reset before programming IIC0. */
    R_IIC0->ICCR1 &= (uint8_t)~IIC_ICCR1_ICE;
    R_IIC0->ICCR1 |= IIC_ICCR1_IICRST;
    R_IIC0->ICCR1 |= IIC_ICCR1_ICE;
    R_IIC0->ICSER = 0U;

    result = iic0_configure_bitrate(system_clocks.pclkb, APP_I2C_BITRATE_HZ);
    if (result != RTC14_OK)
    {
        return result;
    }

    /* Release internal reset and connect the selected peripheral pins. */
    R_IIC0->ICCR1 &= (uint8_t)~IIC_ICCR1_IICRST;
    iic0_configure_pins();
    iic0_clear_receive_state();

    return RTC14_OK;
}


/* -------------------------------------------------------------------------- */
/* Polling IIC0 transactions                                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Handle bus write.
 * @param[in] address 7-bit I2C device address.
 * @param[in] data Data buffer.
 * @param[in] length Number of bytes in the buffer.
 * @param[in] restart_after_write Nonzero to leave the bus in repeated-START state.
 * @return Zero on success; otherwise a negative error code.
 */
int iic0_bus_write(uint8_t address, const uint8_t *data,
                      uint32_t length, uint8_t restart_after_write)
{
    uint32_t index;
    int result;

    if ((data == 0) || (length == 0U))
    {
        return RTC14_ERROR_ARGUMENT;
    }

    result = iic0_wait_clear(&R_IIC0->ICCR2, IIC_ICCR2_BBSY);
    if (result != RTC14_OK)
    {
        return result;
    }

    R_IIC0->ICSR2 &= (uint8_t)~(IIC_ICSR2_NACKF | IIC_ICSR2_STOP);
    R_IIC0->ICCR2 |= IIC_ICCR2_ST;

    result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_TDRE);
    if (result != RTC14_OK)
    {
        return result;
    }

    R_IIC0->ICDRT = (uint8_t)(address << 1);

    result = iic0_wait_transmit_flag(IIC_ICSR2_TDRE);
    if (result != RTC14_OK)
    {
        (void)iic0_request_stop();
        return result;
    }

    for (index = 0U; index < length; index++)
    {
        result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_TDRE);
        if (result != RTC14_OK)
        {
            return result;
        }

        R_IIC0->ICDRT = data[index];
    }

    result = iic0_wait_transmit_flag(IIC_ICSR2_TEND);
    if (result != RTC14_OK)
    {
        (void)iic0_request_stop();
        return result;
    }

    if (restart_after_write != 0U)
    {
        R_IIC0->ICCR2 |= IIC_ICCR2_RS;
        return RTC14_OK;
    }

    return iic0_request_stop();
}


/**
 * @brief Read bytes after a repeated START generated by the write phase.
 * @param[in] address 7-bit I2C device address.
 * @param[out] data Data buffer.
 * @param[in] length Number of bytes in the buffer.
 * @return Zero on success; otherwise a negative error code.
 */
int iic0_bus_read_after_restart(uint8_t address,
                                   uint8_t *data, uint32_t length)
{
    uint32_t index;
    volatile uint8_t dummy;
    int result;

    if ((data == 0) || (length == 0U))
    {
        return RTC14_ERROR_ARGUMENT;
    }

    result = iic0_wait_clear(&R_IIC0->ICCR2, IIC_ICCR2_RS);
    if (result != RTC14_OK)
    {
        return result;
    }

    R_IIC0->ICDRT = (uint8_t)((address << 1) | 1U);

    result = iic0_wait_clear(&R_IIC0->ICCR2, IIC_ICCR2_TRS);
    if (result != RTC14_OK)
    {
        return result;
    }

    result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_RDRF);
    if (result != RTC14_OK)
    {
        return result;
    }

    if (length == 1U)
    {
        R_IIC0->ICMR3 |= IIC_ICMR3_WAIT;
        iic0_set_receive_nack(1U);
    }

    if ((R_IIC0->ICSR2 & IIC_ICSR2_NACKF) != 0U)
    {
        R_IIC0->ICSR2 &= (uint8_t)~IIC_ICSR2_STOP;
        R_IIC0->ICCR2 |= IIC_ICCR2_SP;
        dummy = R_IIC0->ICDRR;
        (void)dummy;

        (void)iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_STOP);
        iic0_clear_receive_state();
        return RTC14_ERROR_NACK;
    }

    /* Dummy read starts reception. */
    dummy = R_IIC0->ICDRR;
    (void)dummy;

    if (length > 1U)
    {
        for (index = 0U; index < (length - 2U); index++)
        {
            result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_RDRF);
            if (result != RTC14_OK)
            {
                return result;
            }

            data[index] = R_IIC0->ICDRR;
        }

        result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_RDRF);
        if (result != RTC14_OK)
        {
            return result;
        }

        R_IIC0->ICMR3 |= IIC_ICMR3_WAIT;
        iic0_set_receive_nack(1U);
        data[length - 2U] = R_IIC0->ICDRR;
    }

    result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_RDRF);
    if (result != RTC14_OK)
    {
        return result;
    }

    R_IIC0->ICSR2 &= (uint8_t)~IIC_ICSR2_STOP;
    R_IIC0->ICCR2 |= IIC_ICCR2_SP;
    data[length - 1U] = R_IIC0->ICDRR;

    result = iic0_wait_set(&R_IIC0->ICSR2, IIC_ICSR2_STOP);
    iic0_clear_receive_state();

    return result;
}
