/**
 * @file ips2_display.c
 * @brief IPS Display 2 initialization, graphics primitives and smartwatch UI.
 */

#include "ips2_display.h"
#include "app_config.h"
#include "ra4m2_baremetal.h"

/* IPS Display 2 control pins on mikroBUS 2. */
#define IPS2_CS_MASK (1U << 8)
#define IPS2_RST_MASK (1U << 10)
#define IPS2_DC_MASK (1U << 15)
#define IPS2_BCK_MASK (1U << 1)
#define IPS2_CMD_SLPOUT (0x11U)
#define IPS2_CMD_INVON (0x21U)
#define IPS2_CMD_DISPON (0x29U)
#define IPS2_CMD_CASET (0x2AU)
#define IPS2_CMD_RASET (0x2BU)
#define IPS2_CMD_RAMWR (0x2CU)
#define IPS2_CMD_MADCTL (0x36U)
#define IPS2_CMD_COLMOD (0x3AU)
#define IPS2_CMD_PORCTRL (0xB2U)
#define IPS2_CMD_GCTRL (0xB7U)
#define IPS2_CMD_VCOMS (0xBBU)
#define IPS2_CMD_VDVVRHEN (0xC2U)
#define IPS2_CMD_VRHS (0xC3U)
#define IPS2_CMD_VDVSET (0xC4U)
#define IPS2_CMD_FRCTR2 (0xC6U)
#define IPS2_CMD_PWCTRL1 (0xD0U)
#define IPS2_CMD_PVGAMCTRL (0xE0U)
#define IPS2_CMD_NVGAMCTRL (0xE1U)
#define IPS2_COLOR_BG (0x0842U)
#define IPS2_COLOR_CARD (0x10C4U)
#define IPS2_COLOR_CARD_2 (0x18E6U)
#define IPS2_COLOR_WHITE (0xFFFFU)
#define IPS2_COLOR_SOFT_WHITE (0xDEDBU)
#define IPS2_COLOR_MUTED (0x7BEFU)
#define IPS2_COLOR_CYAN (0x07FFU)
#define IPS2_COLOR_BLUE (0x249FU)
#define IPS2_COLOR_DARK_CYAN (0x0318U)
#define IPS2_COLOR_ERROR (0xF986U)
#define IPS2_COLOR_ORANGE (0xFD20U)
#define IPS2_COLOR_DEEP_ORANGE (0xA280U)
#define IPS2_COLOR_AQUA (0x4E7FU)
#define IPS2_COLOR_YELLOW (0xFFE0U)
#define IPS2_COLOR_SKY_DAY_TOP (0x147DU)
#define IPS2_COLOR_SKY_DAY_MID (0x24BFU)
#define IPS2_COLOR_SKY_DAY_LOW (0x65BFU)
#define IPS2_COLOR_SKY_NIGHT_TOP (0x0008U)
#define IPS2_COLOR_SKY_NIGHT_MID (0x0842U)
#define IPS2_COLOR_SKY_NIGHT_LOW (0x18A7U)
#define IPS2_COLOR_HORIZON_DAY (0xF5C9U)
#define IPS2_COLOR_HORIZON_NIGHT (0x2948U)

static uint8_t ips2_frame_drawn;
static uint8_t ips2_last_day = 0xFFU;
static uint8_t ips2_last_month = 0xFFU;
static uint8_t ips2_last_year = 0xFFU;
static uint8_t ips2_last_weekday = 0xFFU;
static uint8_t ips2_last_hour = 0xFFU;
static uint8_t ips2_last_minute = 0xFFU;
static uint8_t ips2_last_second = 0xFFU;
static uint8_t ips2_last_mode = 0xFFU;
static uint8_t ips2_last_astronomy_mode = 0xFFU;
static uint8_t ips2_last_timer_hour = 0xFFU;
static uint8_t ips2_last_timer_minute = 0xFFU;
static uint8_t ips2_last_timer_second = 0xFFU;
static uint8_t ips2_last_timer_state = 0xFFU;
static uint8_t ips2_last_timer_b3_action = 0xFFU;
static uint8_t ips2_last_timer_b4_action = 0xFFU;
static int16_t ips2_last_temperature_tenths = (int16_t)0x7FFF;
static uint16_t ips2_last_humidity_tenths = 0xFFFFU;
static uint8_t ips2_last_environment_valid = 0xFFU;
static uint8_t ips2_last_ble_connected = 0xFFU;
static int16_t ips2_animated_temperature_tenths = (int16_t)0x7FFF;
static uint16_t ips2_animated_humidity_tenths = 0xFFFFU;
static int16_t ips2_last_drawn_temperature_tenths = (int16_t)0x7FFF;
static uint16_t ips2_last_drawn_humidity_tenths = 0xFFFFU;
static uint16_t ips2_last_body_x = 0xFFFFU;
static uint16_t ips2_last_body_y = 0xFFFFU;

/**
 * @brief Return the SCI0 register view used by IPS Display 2 Click.
 * @return Pointer to the requested register or data view.
 */
static volatile ra4m2_sci_registers_t *ips2_sci0(void)
{
    return (volatile ra4m2_sci_registers_t *)(uintptr_t)R_SCI0;
}


/**
 * @brief Assert the display chip-select signal.
 */
static void ips2_cs_low(void)  { R_PORT7->PODR &= (uint16_t)~IPS2_CS_MASK; }


/**
 * @brief Deassert the display chip-select signal.
 */
static void ips2_cs_high(void) { R_PORT7->PODR |= IPS2_CS_MASK; }


/**
 * @brief Assert the display reset signal.
 */
static void ips2_rst_low(void) { R_PORT2->PODR &= (uint16_t)~IPS2_RST_MASK; }


/**
 * @brief Deassert the display reset signal.
 */
static void ips2_rst_high(void){ R_PORT2->PODR |= IPS2_RST_MASK; }


/**
 * @brief Select display command mode.
 */
static void ips2_dc_low(void)  { R_PORT4->PODR &= (uint16_t)~IPS2_DC_MASK; }


/**
 * @brief Select display data mode.
 */
static void ips2_dc_high(void) { R_PORT4->PODR |= IPS2_DC_MASK; }


/**
 * @brief Disable the display backlight.
 */
static void ips2_bck_low(void) { R_PORT0->PODR &= (uint16_t)~IPS2_BCK_MASK; }


/**
 * @brief Enable the display backlight.
 */
static void ips2_bck_high(void){ R_PORT0->PODR |= IPS2_BCK_MASK; }


/**
 * @brief Handle delay ms.
 * @param[in] milliseconds Delay duration in milliseconds.
 */
static void ips2_delay_ms(uint32_t milliseconds)
{
    while (milliseconds-- != 0U)
    {
        Delay_1ms();
    }
}


/**
 * @brief Configure pins.
 */
static void ips2_configure_pins(void)
{
    R_PMISC->PWPR_b.B0WI = 0U;
    R_PMISC->PWPR_b.PFSWE = 1U;

    /* SCI0 simple SPI: P410=MISO0, P411=MOSI0, P412=SCK0, all AF4. */
    R_PFS->PORT[4].PIN[10].PmnPFS = PFS_PMR_PERIPHERAL | PFS_PSEL(4U);
    R_PFS->PORT[4].PIN[11].PmnPFS =
        PFS_PODR_HIGH | PFS_PDR_OUTPUT | PFS_PMR_PERIPHERAL | PFS_PSEL(4U);
    R_PFS->PORT[4].PIN[12].PmnPFS =
        PFS_PDR_OUTPUT | PFS_PMR_PERIPHERAL | PFS_PSEL(4U);

    /* GPIO controls: BCK=P001, RST=P210, DC=P415, CS=P708. */
    R_PFS->PORT[0].PIN[1].PmnPFS = PFS_PDR_OUTPUT;
    R_PFS->PORT[2].PIN[10].PmnPFS = PFS_PODR_HIGH | PFS_PDR_OUTPUT;
    R_PFS->PORT[4].PIN[15].PmnPFS = PFS_PODR_HIGH | PFS_PDR_OUTPUT;
    R_PFS->PORT[7].PIN[8].PmnPFS = PFS_PODR_HIGH | PFS_PDR_OUTPUT;

    R_PMISC->PWPR_b.PFSWE = 0U;
    R_PMISC->PWPR_b.B0WI = 1U;
}


/**
 * @brief Handle sci0 set speed.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @param[in] speed_hz Requested serial clock frequency in hertz.
 * @return Calculated or converted value.
 */
static int ips2_sci0_set_speed(uint32_t pclka_hz, uint32_t speed_hz)
{
    volatile ra4m2_sci_registers_t *sci = ips2_sci0();
    uint32_t best_error = 0xFFFFFFFFUL;
    uint8_t best_cks = 0U;
    uint8_t best_brr = 0U;
    uint8_t found = 0U;
    uint8_t cks;

    for (cks = 0U; cks < 4U; cks++)
    {
        uint32_t divisor = 4UL << (2U * cks);
        uint64_t denominator = (uint64_t)speed_hz * divisor;
        uint32_t brr_plus_one;
        uint32_t actual_hz;
        uint32_t error;

        if (denominator == 0ULL)
        {
            continue;
        }

        brr_plus_one = (uint32_t)(((uint64_t)pclka_hz + (denominator / 2ULL)) /
                                  denominator);
        if ((brr_plus_one == 0U) || (brr_plus_one > 256U))
        {
            continue;
        }

        actual_hz = pclka_hz / (divisor * brr_plus_one);
        error = (actual_hz > speed_hz) ?
                (actual_hz - speed_hz) : (speed_hz - actual_hz);

        if (error < best_error)
        {
            best_error = error;
            best_cks = cks;
            best_brr = (uint8_t)(brr_plus_one - 1U);
            found = 1U;
        }
    }

    if (found == 0U)
    {
        return -1;
    }

    sci->smr = (uint8_t)((sci->smr & (uint8_t)~0x03U) | best_cks);
    sci->brr = best_brr;
    sci->semr &= (uint8_t)~SCI_SEMR_BRME;
    return 0;
}


/**
 * @brief Handle spi init.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @return Zero on success; otherwise a negative error code.
 */
static int ips2_spi_init(uint32_t pclka_hz)
{
    volatile ra4m2_sci_registers_t *sci = ips2_sci0();

    /* SCI0 module stop release: MSTPCRB.MSTPB31 = 0. */
    R_MSTP->MSTPCRB &= ~(1UL << 31);
    (void)R_MSTP->MSTPCRB;

    sci->scr = 0U;
    sci->spmr = 0U;
    sci->fcr &= (uint16_t)~SCI_FCR_FM;
    sci->simr1 &= (uint8_t)~SCI_SIMR1_IICM;

    sci->smr = SCI_SMR_CM;
    sci->scmr &= (uint8_t)~(SCI_SCMR_SMIF | SCI_SCMR_SINV);
    sci->scmr |= SCI_SCMR_SDIR;
    sci->sptr = 0U;

    /* SPI mode 0 according to the SDK SCI implementation: CKPOL=0, CKPH=1. */
    sci->spmr &= (uint8_t)~SCI_SPMR_CKPOL;
    sci->spmr |= SCI_SPMR_CKPH;

    if (ips2_sci0_set_speed(pclka_hz, IPS2_SPI_HZ) != 0)
    {
        return -1;
    }

    sci->ssr &= (uint8_t)~(SCI_SSR_ORER | SCI_SSR_FER | SCI_SSR_PER);
    sci->scr = (uint8_t)(SCI_SCR_TE | SCI_SCR_RE);
    return 0;
}


/**
 * @brief Transfer one byte over SCI0 simple SPI.
 * @param[in] value Value to process.
 */
static void ips2_spi_write_byte(uint8_t value)
{
    volatile ra4m2_sci_registers_t *sci = ips2_sci0();
    volatile uint8_t dummy;

    if ((sci->ssr & SCI_SSR_ORER) != 0U)
    {
        sci->ssr &= (uint8_t)~SCI_SSR_ORER;
    }

    while ((sci->ssr & SCI_SSR_TDRE) == 0U)
    {
    }

    sci->tdr = value;

    while ((sci->ssr & SCI_SSR_RDRF) == 0U)
    {
    }

    dummy = sci->rdr;
    (void)dummy;
}


/**
 * @brief Wait until the SCI0 transmission is complete.
 */
static void ips2_spi_wait_complete(void)
{
    volatile ra4m2_sci_registers_t *sci = ips2_sci0();
    while ((sci->ssr & SCI_SSR_TEND) == 0U)
    {
    }
}


/**
 * @brief Send one command byte to the display controller.
 * @param[in] command CodeLess or display command string/value.
 */
static void ips2_write_command(uint8_t command)
{
    ips2_cs_low();
    ips2_dc_low();
    ips2_spi_write_byte(command);
    ips2_spi_wait_complete();
    ips2_cs_high();
}


/**
 * @brief Send a display command followed by its data payload.
 * @param[in] command CodeLess or display command string/value.
 * @param[in] data Data buffer.
 * @param[in] length Number of bytes in the buffer.
 */
static void ips2_write_command_data(uint8_t command,
                                    const uint8_t *data,
                                    uint32_t length)
{
    uint32_t index;

    ips2_cs_low();
    ips2_dc_low();
    ips2_spi_write_byte(command);
    ips2_dc_high();

    for (index = 0U; index < length; index++)
    {
        ips2_spi_write_byte(data[index]);
    }

    ips2_spi_wait_complete();
    ips2_cs_high();
}


/**
 * @brief Select the rectangular display RAM write window.
 * @param[in] x0 Left display coordinate.
 * @param[in] y0 Top display coordinate.
 * @param[in] x1 Right display coordinate.
 * @param[in] y1 Bottom display coordinate.
 */
static void ips2_set_window(uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    ips2_write_command_data(IPS2_CMD_CASET, data, 4U);

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    ips2_write_command_data(IPS2_CMD_RASET, data, 4U);

    ips2_write_command(IPS2_CMD_RAMWR);
}


/**
 * @brief Fill a clipped rectangular area with one RGB565 color.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_fill_rect(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint16_t color)
{
    uint32_t count;
    uint8_t high;
    uint8_t low;

    if ((width == 0U) || (height == 0U) ||
        (x >= IPS2_WIDTH) || (y >= IPS2_HEIGHT))
    {
        return;
    }

    if ((uint32_t)x + width > IPS2_WIDTH)
    {
        width = (uint16_t)(IPS2_WIDTH - x);
    }
    if ((uint32_t)y + height > IPS2_HEIGHT)
    {
        height = (uint16_t)(IPS2_HEIGHT - y);
    }

    ips2_set_window(x, y,
                    (uint16_t)(x + width - 1U),
                    (uint16_t)(y + height - 1U));

    high = (uint8_t)(color >> 8);
    low = (uint8_t)color;
    count = (uint32_t)width * height;

    ips2_cs_low();
    ips2_dc_high();
    while (count-- != 0U)
    {
        ips2_spi_write_byte(high);
        ips2_spi_write_byte(low);
    }
    ips2_spi_wait_complete();
    ips2_cs_high();
}


/**
 * @brief Draw a rectangular frame with the requested thickness.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] thickness Drawing thickness in pixels.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_frame(uint16_t x, uint16_t y,
                            uint16_t width, uint16_t height,
                            uint16_t thickness, uint16_t color)
{
    ips2_fill_rect(x, y, width, thickness, color);
    ips2_fill_rect(x, (uint16_t)(y + height - thickness), width, thickness, color);
    ips2_fill_rect(x, y, thickness, height, color);
    ips2_fill_rect((uint16_t)(x + width - thickness), y, thickness, height, color);
}


/* Compact 5x7 font. Each value is one vertical column, bit 0 at the top. */
/**
 * @brief Return the compact 5x7 bitmap for one supported character.
 * @param[in] character Character to draw or look up.
 * @return Pointer to the requested register or data view.
 */
static const uint8_t *ips2_glyph(char character)
{
    static const uint8_t blank[5] = { 0, 0, 0, 0, 0 };
    static const uint8_t digits[10][5] =
    {
        { 0x3E, 0x51, 0x49, 0x45, 0x3E },
        { 0x00, 0x42, 0x7F, 0x40, 0x00 },
        { 0x42, 0x61, 0x51, 0x49, 0x46 },
        { 0x21, 0x41, 0x45, 0x4B, 0x31 },
        { 0x18, 0x14, 0x12, 0x7F, 0x10 },
        { 0x27, 0x45, 0x45, 0x45, 0x39 },
        { 0x3C, 0x4A, 0x49, 0x49, 0x30 },
        { 0x01, 0x71, 0x09, 0x05, 0x03 },
        { 0x36, 0x49, 0x49, 0x49, 0x36 },
        { 0x06, 0x49, 0x49, 0x29, 0x1E }
    };
    static const uint8_t letters[26][5] =
    {
        { 0x7E, 0x11, 0x11, 0x11, 0x7E },
        { 0x7F, 0x49, 0x49, 0x49, 0x36 },
        { 0x3E, 0x41, 0x41, 0x41, 0x22 },
        { 0x7F, 0x41, 0x41, 0x22, 0x1C },
        { 0x7F, 0x49, 0x49, 0x49, 0x41 },
        { 0x7F, 0x09, 0x09, 0x09, 0x01 },
        { 0x3E, 0x41, 0x49, 0x49, 0x7A },
        { 0x7F, 0x08, 0x08, 0x08, 0x7F },
        { 0x00, 0x41, 0x7F, 0x41, 0x00 },
        { 0x20, 0x40, 0x41, 0x3F, 0x01 },
        { 0x7F, 0x08, 0x14, 0x22, 0x41 },
        { 0x7F, 0x40, 0x40, 0x40, 0x40 },
        { 0x7F, 0x02, 0x0C, 0x02, 0x7F },
        { 0x7F, 0x04, 0x08, 0x10, 0x7F },
        { 0x3E, 0x41, 0x41, 0x41, 0x3E },
        { 0x7F, 0x09, 0x09, 0x09, 0x06 },
        { 0x3E, 0x41, 0x51, 0x21, 0x5E },
        { 0x7F, 0x09, 0x19, 0x29, 0x46 },
        { 0x46, 0x49, 0x49, 0x49, 0x31 },
        { 0x01, 0x01, 0x7F, 0x01, 0x01 },
        { 0x3F, 0x40, 0x40, 0x40, 0x3F },
        { 0x1F, 0x20, 0x40, 0x20, 0x1F },
        { 0x3F, 0x40, 0x38, 0x40, 0x3F },
        { 0x63, 0x14, 0x08, 0x14, 0x63 },
        { 0x07, 0x08, 0x70, 0x08, 0x07 },
        { 0x61, 0x51, 0x49, 0x45, 0x43 }
    };
    static const uint8_t colon[5] = { 0x00, 0x36, 0x36, 0x00, 0x00 };
    static const uint8_t dot[5] = { 0x00, 0x60, 0x60, 0x00, 0x00 };
    static const uint8_t dash[5] = { 0x08, 0x08, 0x08, 0x08, 0x08 };
    static const uint8_t percent[5] = { 0x63, 0x13, 0x08, 0x64, 0x63 };

    if ((character >= '0') && (character <= '9'))
    {
        return digits[(uint8_t)character - (uint8_t)'0'];
    }
    if ((character >= 'A') && (character <= 'Z'))
    {
        return letters[(uint8_t)character - (uint8_t)'A'];
    }
    if (character == ':')
    {
        return colon;
    }
    if (character == '.')
    {
        return dot;
    }
    if (character == '-')
    {
        return dash;
    }
    if (character == '%')
    {
        return percent;
    }
    return blank;
}


/**
 * @brief Calculate rendered text width for the selected font scale.
 * @param[in] text Null-terminated text string.
 * @param[in] scale Integer font scale.
 * @return Calculated or converted value.
 */
static uint16_t ips2_text_width(const char *text, uint8_t scale)
{
    uint16_t length = 0U;
    while (text[length] != '\0')
    {
        length++;
    }
    return (length == 0U) ? 0U :
           (uint16_t)(length * (6U * scale) - scale);
}


/**
 * @brief Draw one scaled 5x7 character.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] character Character to draw or look up.
 * @param[in] scale Integer font scale.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_char(uint16_t x, uint16_t y,
                           char character, uint8_t scale,
                           uint16_t color)
{
    const uint8_t *glyph = ips2_glyph(character);
    uint8_t column;
    uint8_t row;

    for (column = 0U; column < 5U; column++)
    {
        for (row = 0U; row < 7U; row++)
        {
            if ((glyph[column] & (1U << row)) != 0U)
            {
                ips2_fill_rect((uint16_t)(x + column * scale),
                               (uint16_t)(y + row * scale),
                               scale, scale, color);
            }
        }
    }
}


/**
 * @brief Draw a null-terminated text string.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] text Null-terminated text string.
 * @param[in] scale Integer font scale.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_text(uint16_t x, uint16_t y,
                           const char *text, uint8_t scale,
                           uint16_t color)
{
    while (*text != '\0')
    {
        ips2_draw_char(x, y, *text, scale, color);
        x = (uint16_t)(x + 6U * scale);
        text++;
    }
}


/**
 * @brief Draw text centered horizontally on the display.
 * @param[in] y Vertical display coordinate.
 * @param[in] text Null-terminated text string.
 * @param[in] scale Integer font scale.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_text_centered(uint16_t y,
                                    const char *text,
                                    uint8_t scale,
                                    uint16_t color)
{
    uint16_t width = ips2_text_width(text, scale);
    uint16_t x = (width < IPS2_WIDTH) ?
                 (uint16_t)((IPS2_WIDTH - width) / 2U) : 0U;
    ips2_draw_text(x, y, text, scale, color);
}


/**
 * @brief Draw text while repainting its rectangular background.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] text Null-terminated text string.
 * @param[in] scale Integer font scale.
 * @param[in] foreground RGB565 text color.
 * @param[in] background RGB565 background color.
 */
static void ips2_draw_text_opaque(uint16_t x, uint16_t y,
                                  const char *text, uint8_t scale,
                                  uint16_t foreground, uint16_t background)
{
    uint16_t length = 0U;
    uint16_t width;
    uint16_t height;
    uint16_t py;
    uint16_t px;

    while (text[length] != '\0')
    {
        length++;
    }

    if ((length == 0U) || (scale == 0U))
    {
        return;
    }

    width = (uint16_t)(length * 6U * scale);
    height = (uint16_t)(7U * scale);
    ips2_set_window(x, y, (uint16_t)(x + width - 1U),
                    (uint16_t)(y + height - 1U));

    ips2_cs_low();
    ips2_dc_high();

    for (py = 0U; py < height; py++)
    {
        uint8_t glyph_row = (uint8_t)(py / scale);

        for (px = 0U; px < width; px++)
        {
            uint16_t character_index = (uint16_t)(px / (6U * scale));
            uint8_t glyph_column = (uint8_t)((px % (6U * scale)) / scale);
            const uint8_t *glyph = ips2_glyph(text[character_index]);
            uint16_t color = background;

            if ((glyph_column < 5U) &&
                ((glyph[glyph_column] & (1U << glyph_row)) != 0U))
            {
                color = foreground;
            }

            ips2_spi_write_byte((uint8_t)(color >> 8));
            ips2_spi_write_byte((uint8_t)color);
        }
    }

    ips2_spi_wait_complete();
    ips2_cs_high();
}


/**
 * @brief Invalidate screen.
 */
void ips2_invalidate_screen(void)
{
    ips2_frame_drawn = 0U;
    ips2_last_day = 0xFFU;
    ips2_last_month = 0xFFU;
    ips2_last_year = 0xFFU;
    ips2_last_weekday = 0xFFU;
    ips2_last_hour = 0xFFU;
    ips2_last_minute = 0xFFU;
    ips2_last_second = 0xFFU;
    ips2_last_mode = 0xFFU;
    ips2_last_astronomy_mode = 0xFFU;
    ips2_last_timer_hour = 0xFFU;
    ips2_last_timer_minute = 0xFFU;
    ips2_last_timer_second = 0xFFU;
    ips2_last_timer_state = 0xFFU;
    ips2_last_timer_b3_action = 0xFFU;
    ips2_last_timer_b4_action = 0xFFU;
    ips2_last_temperature_tenths = (int16_t)0x7FFF;
    ips2_last_humidity_tenths = 0xFFFFU;
    ips2_last_environment_valid = 0xFFU;
    ips2_last_ble_connected = 0xFFU;
    ips2_animated_temperature_tenths = (int16_t)0x7FFF;
    ips2_animated_humidity_tenths = 0xFFFFU;
    ips2_last_drawn_temperature_tenths = (int16_t)0x7FFF;
    ips2_last_drawn_humidity_tenths = 0xFFFFU;
    ips2_last_body_x = 0xFFFFU;
    ips2_last_body_y = 0xFFFFU;
}


/**
 * @brief Draw text centered inside a rectangular region.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] width Rectangle width in pixels.
 * @param[in] text Null-terminated text string.
 * @param[in] scale Integer font scale.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_text_in_rect_centered(uint16_t x, uint16_t y,
                                             uint16_t width,
                                             const char *text,
                                             uint8_t scale,
                                             uint16_t color)
{
    uint16_t text_width = ips2_text_width(text, scale);
    uint16_t draw_x = x;

    if (text_width < width)
    {
        draw_x = (uint16_t)(x + ((width - text_width) / 2U));
    }

    ips2_draw_text(draw_x, y, text, scale, color);
}


/**
 * @brief Calculate the integer square root of an unsigned value.
 * @param[in] value Value to process.
 * @return Calculated or converted value.
 */
static uint32_t ips2_isqrt(uint32_t value)
{
    uint32_t result = 0U;
    uint32_t bit = 1UL << 30;

    while (bit > value)
    {
        bit >>= 2;
    }

    while (bit != 0U)
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}


/**
 * @brief Draw a clipped filled circle.
 * @param[in] center_x Center x value.
 * @param[in] center_y Center y value.
 * @param[in] radius Circle radius in pixels.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_filled_circle(int32_t center_x, int32_t center_y,
                                    uint16_t radius, uint16_t color)
{
    int32_t dy;
    uint32_t radius_squared = (uint32_t)radius * radius;

    for (dy = -(int32_t)radius; dy <= (int32_t)radius; dy++)
    {
        uint32_t dy_squared = (uint32_t)(dy * dy);
        uint16_t half_width = (uint16_t)ips2_isqrt(radius_squared - dy_squared);
        int32_t x = center_x - half_width;
        int32_t y = center_y + dy;
        uint16_t width = (uint16_t)(half_width * 2U + 1U);

        if ((y >= 0) && (y < (int32_t)IPS2_HEIGHT) &&
            (x < (int32_t)IPS2_WIDTH) && ((x + width) > 0))
        {
            if (x < 0)
            {
                width = (uint16_t)(width + x);
                x = 0;
            }
            ips2_fill_rect((uint16_t)x, (uint16_t)y, width, 1U, color);
        }
    }
}


/**
 * @brief Draw a line segment with configurable thickness.
 * @param[in] x0 Left display coordinate.
 * @param[in] y0 Top display coordinate.
 * @param[in] x1 Right display coordinate.
 * @param[in] y1 Bottom display coordinate.
 * @param[in] thickness Drawing thickness in pixels.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_line(int32_t x0, int32_t y0,
                           int32_t x1, int32_t y1,
                           uint16_t thickness, uint16_t color)
{
    int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = (y1 > y0) ? (y0 - y1) : (y1 - y0);
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t error = dx + dy;

    while (1)
    {
        if ((x0 >= 0) && (y0 >= 0) &&
            (x0 < (int32_t)IPS2_WIDTH) && (y0 < (int32_t)IPS2_HEIGHT))
        {
            ips2_fill_rect((uint16_t)x0, (uint16_t)y0,
                           thickness, thickness, color);
        }

        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        {
            int32_t doubled = error * 2;
            if (doubled >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (doubled <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    }
}


/**
 * @brief Format a signed fixed-point tenths value as text.
 * @param[in] text Null-terminated text string.
 * @param[in] value Value to process.
 * @param[in] unit Unit value.
 */
static void ips2_format_tenths(char *text, int32_t value, char unit)
{
    uint32_t magnitude;
    uint32_t whole;
    uint8_t index = 0U;

    if (value < 0)
    {
        text[index++] = '-';
        magnitude = (uint32_t)(-value);
    }
    else
    {
        magnitude = (uint32_t)value;
    }

    whole = magnitude / 10U;

    if (whole >= 100U)
    {
        text[index++] = (char)('0' + ((whole / 100U) % 10U));
        text[index++] = (char)('0' + ((whole / 10U) % 10U));
    }
    else if (whole >= 10U)
    {
        text[index++] = (char)('0' + ((whole / 10U) % 10U));
    }

    text[index++] = (char)('0' + (whole % 10U));
    text[index++] = '.';
    text[index++] = (char)('0' + (magnitude % 10U));
    text[index++] = ' ';
    text[index++] = unit;
    text[index] = '\0';
}


/**
 * @brief Format a value as two decimal digits.
 * @param[in] text Null-terminated text string.
 * @param[in] value Value to process.
 */
static void ips2_format_two_digits(char *text, uint32_t value)
{
    value %= 100U;
    text[0] = (char)('0' + (value / 10U));
    text[1] = (char)('0' + (value % 10U));
}


/**
 * @brief Format a value as three decimal digits.
 * @param[in] text Null-terminated text string.
 * @param[in] value Value to process.
 */
static void ips2_format_three_digits(char *text, uint32_t value)
{
    value %= 1000U;
    text[0] = (char)('0' + (value / 100U));
    text[1] = (char)('0' + ((value / 10U) % 10U));
    text[2] = (char)('0' + (value % 10U));
}


/**
 * @brief Draw the compact Bluetooth status symbol.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] color RGB565 drawing color.
 */
static void ips2_draw_bluetooth_symbol(uint16_t x, uint16_t y,
                                        uint16_t color)
{
    uint8_t index;

    ips2_fill_rect((uint16_t)(x + 8U), (uint16_t)(y + 1U),
                   2U, 25U, color);

    for (index = 0U; index < 7U; index++)
    {
        ips2_fill_rect((uint16_t)(x + 9U + index),
                       (uint16_t)(y + 1U + index), 2U, 2U, color);
        ips2_fill_rect((uint16_t)(x + 15U - index),
                       (uint16_t)(y + 7U + index), 2U, 2U, color);
        ips2_fill_rect((uint16_t)(x + 9U + index),
                       (uint16_t)(y + 13U + index), 2U, 2U, color);
        ips2_fill_rect((uint16_t)(x + 15U - index),
                       (uint16_t)(y + 19U + index), 2U, 2U, color);
    }

    for (index = 0U; index < 10U; index++)
    {
        ips2_fill_rect((uint16_t)(x + index),
                       (uint16_t)(y + 7U + index), 2U, 2U, color);
        ips2_fill_rect((uint16_t)(x + index),
                       (uint16_t)(y + 18U - index), 2U, 2U, color);
    }
}


/**
 * @brief Render ble status.
 * @param[in] connected BLE connection state or destination for that state.
 * @param[in] screen Application screen whose status area is being updated.
 */
void ips2_render_ble_status(uint8_t connected, app_screen_t screen)
{
    uint16_t x;
    uint16_t y;
    uint16_t background;
    uint16_t symbol_color;
    uint8_t index;

    connected = (connected != 0U) ? 1U : 0U;
    if (connected == ips2_last_ble_connected)
    {
        return;
    }

    if (screen == APP_SCREEN_DASHBOARD)
    {
        x = 201U;
        y = 19U;
        background = IPS2_COLOR_CARD;
    }
    else if (screen == APP_SCREEN_STOPWATCH)
    {
        x = 198U;
        y = 9U;
        background = IPS2_COLOR_BG;
    }
    else
    {
        x = 199U;
        y = 6U;
        background = IPS2_COLOR_CARD;
    }

    ips2_fill_rect(x, y,
                   (screen == APP_SCREEN_DASHBOARD) ? 24U : 27U,
                   31U, background);
    symbol_color = (connected != 0U) ? IPS2_COLOR_CYAN : IPS2_COLOR_MUTED;
    ips2_draw_bluetooth_symbol((uint16_t)(x + 2U),
                               (uint16_t)(y + 2U), symbol_color);

    if (connected != 0U)
    {
        ips2_fill_rect((uint16_t)(x + 19U), (uint16_t)(y + 23U),
                       5U, 5U, IPS2_COLOR_CYAN);
    }
    else
    {
        for (index = 0U; index < 13U; index++)
        {
            ips2_fill_rect((uint16_t)(x + 1U + index),
                           (uint16_t)(y + 3U + (index * 2U)),
                           2U, 2U, IPS2_COLOR_ERROR);
        }
    }

    ips2_last_ble_connected = connected;
}



/**
 * @brief Draw the compact dashboard temperature icon.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 */
static void ips2_draw_dashboard_thermometer_icon(uint16_t x, uint16_t y)
{
    ips2_fill_rect((uint16_t)(x + 7U), y, 8U, 24U, IPS2_COLOR_ORANGE);
    ips2_fill_rect((uint16_t)(x + 9U), (uint16_t)(y + 3U),
                   4U, 19U, IPS2_COLOR_CARD);
    ips2_fill_rect((uint16_t)(x + 9U), (uint16_t)(y + 10U),
                   4U, 15U, IPS2_COLOR_ERROR);
    ips2_fill_rect((uint16_t)(x + 4U), (uint16_t)(y + 20U),
                   14U, 14U, IPS2_COLOR_ORANGE);
    ips2_fill_rect((uint16_t)(x + 7U), (uint16_t)(y + 23U),
                   8U, 8U, IPS2_COLOR_ERROR);
}


/**
 * @brief Draw the compact dashboard humidity icon.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 */
static void ips2_draw_dashboard_humidity_icon(uint16_t x, uint16_t y)
{
    ips2_fill_rect((uint16_t)(x + 10U), y, 4U, 4U, IPS2_COLOR_AQUA);
    ips2_fill_rect((uint16_t)(x + 8U), (uint16_t)(y + 4U),
                   8U, 5U, IPS2_COLOR_AQUA);
    ips2_fill_rect((uint16_t)(x + 5U), (uint16_t)(y + 9U),
                   14U, 7U, IPS2_COLOR_AQUA);
    ips2_fill_rect((uint16_t)(x + 3U), (uint16_t)(y + 16U),
                   18U, 10U, IPS2_COLOR_CYAN);
    ips2_fill_rect((uint16_t)(x + 6U), (uint16_t)(y + 26U),
                   12U, 5U, IPS2_COLOR_CYAN);
    ips2_fill_rect((uint16_t)(x + 10U), (uint16_t)(y + 31U),
                   4U, 3U, IPS2_COLOR_CYAN);
    ips2_fill_rect((uint16_t)(x + 7U), (uint16_t)(y + 18U),
                   4U, 5U, IPS2_COLOR_WHITE);
}


/**
 * @brief Draw all static elements of the smartwatch dashboard.
 */
static void ips2_draw_dashboard_static(void)
{
    ips2_fill_rect(0U, 0U, IPS2_WIDTH, IPS2_HEIGHT, IPS2_COLOR_BG);

    ips2_fill_rect(14U, 12U, 212U, 47U, IPS2_COLOR_CARD);
    ips2_fill_rect(14U, 12U, 5U, 47U, IPS2_COLOR_BLUE);
    ips2_draw_frame(14U, 12U, 212U, 47U, 1U, IPS2_COLOR_CARD_2);

    ips2_fill_rect(14U, 69U, 102U, 82U, IPS2_COLOR_CARD);
    ips2_fill_rect(124U, 69U, 102U, 82U, IPS2_COLOR_CARD);
    ips2_draw_frame(14U, 69U, 102U, 82U, 1U, IPS2_COLOR_DEEP_ORANGE);
    ips2_draw_frame(124U, 69U, 102U, 82U, 1U, IPS2_COLOR_DARK_CYAN);

    ips2_draw_dashboard_thermometer_icon(25U, 80U);
    ips2_draw_dashboard_humidity_icon(135U, 80U);
    ips2_draw_text(57U, 82U, "TEMP", 1U, IPS2_COLOR_MUTED);
    ips2_draw_text(165U, 82U, "HUMIDITY", 1U, IPS2_COLOR_MUTED);

    ips2_fill_rect(14U, 160U, 212U, 64U, IPS2_COLOR_CARD);
    ips2_fill_rect(14U, 160U, 5U, 64U, IPS2_COLOR_CYAN);
    ips2_draw_frame(14U, 160U, 212U, 64U, 1U, IPS2_COLOR_CARD_2);

    ips2_draw_text_centered(231U, "B3 - NEXT", 1U, IPS2_COLOR_MUTED);
}


/**
 * @brief Format the RTC date for the smartwatch dashboard.
 * @param[in] date Date value.
 * @param[in] datetime Decoded RTC date and time.
 */
static void ips2_format_dashboard_date(char *date,
                                        const rtc14_datetime_t *datetime)
{
    date[0] = (char)('0' + (datetime->day / 10U));
    date[1] = (char)('0' + (datetime->day % 10U));
    date[2] = '.';
    date[3] = (char)('0' + (datetime->month / 10U));
    date[4] = (char)('0' + (datetime->month % 10U));
    date[5] = '.';
    date[6] = '2';
    date[7] = '0';
    date[8] = (char)('0' + (datetime->year / 10U));
    date[9] = (char)('0' + (datetime->year % 10U));
    date[10] = '\0';
}


/**
 * @brief Draw the weekday and date area of the dashboard.
 * @param[in] datetime Decoded RTC date and time.
 */
static void ips2_draw_dashboard_date(const rtc14_datetime_t *datetime)
{
    static const char *const weekday_name[7] =
    {
        "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
        "THURSDAY", "FRIDAY", "SATURDAY"
    };
    const char *weekday = "UNKNOWN";
    char date[11];

    if (datetime->weekday < 7U)
    {
        weekday = weekday_name[datetime->weekday];
    }

    ips2_fill_rect(20U, 163U, 202U, 58U, IPS2_COLOR_CARD);
    ips2_draw_text_centered(169U, weekday, 2U, IPS2_COLOR_CYAN);
    ips2_format_dashboard_date(date, datetime);
    ips2_draw_text_centered(195U, date, 3U, IPS2_COLOR_SOFT_WHITE);
}


/**
 * @brief Draw the dashboard clock and AM/PM indicator.
 * @param[in] datetime Decoded RTC date and time.
 */
static void ips2_draw_dashboard_time(const rtc14_datetime_t *datetime)
{
    uint8_t hour = datetime->hour;
    uint8_t minute = datetime->minute;
    uint8_t mode = (datetime->is_24_hour != 0U) ? 2U : datetime->is_pm;
    char time_text[6];
    const char *mode_text;
    uint8_t mode_scale;
    uint16_t mode_width;
    uint16_t mode_x;

    if ((hour == ips2_last_hour) &&
        (minute == ips2_last_minute) &&
        (mode == ips2_last_mode))
    {
        return;
    }

    time_text[0] = (char)('0' + (hour / 10U));
    time_text[1] = (char)('0' + (hour % 10U));
    time_text[2] = ':';
    time_text[3] = (char)('0' + (minute / 10U));
    time_text[4] = (char)('0' + (minute % 10U));
    time_text[5] = '\0';

    ips2_fill_rect(20U, 15U, 178U, 41U, IPS2_COLOR_CARD);
    ips2_draw_text(27U, 19U, time_text, 4U, IPS2_COLOR_WHITE);

    mode_text = (datetime->is_24_hour != 0U) ? "24H" :
                ((datetime->is_pm != 0U) ? "PM" : "AM");
    mode_scale = (datetime->is_24_hour != 0U) ? 2U : 3U;
    mode_width = (uint16_t)(ips2_text_width(mode_text, mode_scale) + 16U);
    mode_x = 148U;

    ips2_fill_rect(mode_x, 20U, mode_width, 31U, IPS2_COLOR_BLUE);
    ips2_draw_text_in_rect_centered(mode_x, 25U, mode_width,
                                    mode_text, mode_scale,
                                    IPS2_COLOR_WHITE);

    ips2_last_hour = hour;
    ips2_last_minute = minute;
    ips2_last_mode = mode;
}


/**
 * @brief Draw compact temperature and humidity summary cards.
 * @param[in] environment Temperature and humidity measurement.
 */
static void ips2_draw_dashboard_environment(
    const temphum27_data_t *environment)
{
    char temperature[9];
    char humidity[9];

    if ((environment->valid == ips2_last_environment_valid) &&
        (environment->temperature_tenths_c == ips2_last_temperature_tenths) &&
        (environment->humidity_tenths_percent == ips2_last_humidity_tenths))
    {
        return;
    }

    ips2_fill_rect(20U, 115U, 90U, 29U, IPS2_COLOR_CARD);
    ips2_fill_rect(130U, 115U, 90U, 29U, IPS2_COLOR_CARD);

    if (environment->valid != 0U)
    {
        ips2_format_tenths(temperature,
                           environment->temperature_tenths_c, 'C');
        ips2_format_tenths(humidity,
                           environment->humidity_tenths_percent, '%');
    }
    else
    {
        temperature[0] = '-';
        temperature[1] = '-';
        temperature[2] = '.';
        temperature[3] = '-';
        temperature[4] = ' ';
        temperature[5] = 'C';
        temperature[6] = '\0';

        humidity[0] = '-';
        humidity[1] = '-';
        humidity[2] = '.';
        humidity[3] = '-';
        humidity[4] = ' ';
        humidity[5] = '%';
        humidity[6] = '\0';
    }

    ips2_draw_text_in_rect_centered(20U, 120U, 90U,
                                    temperature, 2U,
                                    IPS2_COLOR_SOFT_WHITE);
    ips2_draw_text_in_rect_centered(130U, 120U, 90U,
                                    humidity, 2U,
                                    IPS2_COLOR_SOFT_WHITE);

    ips2_last_temperature_tenths = environment->temperature_tenths_c;
    ips2_last_humidity_tenths = environment->humidity_tenths_percent;
    ips2_last_environment_valid = environment->valid;
}


/**
 * @brief Render dashboard.
 * @param[in] datetime Decoded RTC date and time.
 * @param[in] environment Temperature and humidity measurement.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_dashboard(const rtc14_datetime_t *datetime,
                           const temphum27_data_t *environment,
                           uint8_t ble_connected)
{
    if ((datetime == 0) || (environment == 0))
    {
        ips2_render_error(ble_connected);
        return;
    }

    if (ips2_frame_drawn == 0U)
    {
        ips2_draw_dashboard_static();
        ips2_frame_drawn = 1U;
    }

    ips2_draw_dashboard_time(datetime);
    ips2_render_ble_status(ble_connected, APP_SCREEN_DASHBOARD);
    ips2_draw_dashboard_environment(environment);

    if ((datetime->day != ips2_last_day) ||
        (datetime->month != ips2_last_month) ||
        (datetime->year != ips2_last_year) ||
        (datetime->weekday != ips2_last_weekday))
    {
        ips2_draw_dashboard_date(datetime);
        ips2_last_day = datetime->day;
        ips2_last_month = datetime->month;
        ips2_last_year = datetime->year;
        ips2_last_weekday = datetime->weekday;
    }
}


/**
 * @brief Map a temperature value to an RGB565 cold-to-hot color.
 * @param[in] temperature_tenths Temperature tenths value.
 * @return Calculated or converted value.
 */
static uint16_t ips2_temperature_color(int16_t temperature_tenths)
{
    int32_t clamped = temperature_tenths;
    uint16_t red;
    uint16_t green;
    uint16_t blue;

    if (clamped < 0)
    {
        clamped = 0;
    }
    if (clamped > 400)
    {
        clamped = 400;
    }

    red = (uint16_t)((31L * clamped) / 400L);
    blue = (uint16_t)(31U - red);
    green = (uint16_t)(8U + ((24L * (400L -
                      ((clamped > 200) ? (clamped - 200) : (200 - clamped)))) /
                      400L));
    if (green > 63U)
    {
        green = 63U;
    }

    return (uint16_t)((red << 11) | (green << 5) | blue);
}


/**
 * @brief Move a signed animated value toward its target by a bounded step.
 * @param[in] current Current animation value.
 * @param[in] target Target animation value.
 * @param[in] step Maximum change applied in one update.
 * @return Calculated or converted value.
 */
static int16_t ips2_approach_i16(int16_t current, int16_t target,
                                 int16_t step)
{
    if (current < target)
    {
        int32_t next = (int32_t)current + step;
        return (next > target) ? target : (int16_t)next;
    }
    if (current > target)
    {
        int32_t next = (int32_t)current - step;
        return (next < target) ? target : (int16_t)next;
    }
    return current;
}


/**
 * @brief Move an unsigned animated value toward its target by a bounded step.
 * @param[in] current Current animation value.
 * @param[in] target Target animation value.
 * @param[in] step Maximum change applied in one update.
 * @return Calculated or converted value.
 */
static uint16_t ips2_approach_u16(uint16_t current, uint16_t target,
                                  uint16_t step)
{
    if (current < target)
    {
        uint32_t next = (uint32_t)current + step;
        return (next > target) ? target : (uint16_t)next;
    }
    if (current > target)
    {
        return ((uint16_t)(current - target) < step) ? target :
               (uint16_t)(current - step);
    }
    return current;
}


/**
 * @brief Draw all static elements of the environment screen.
 */
static void ips2_draw_environment_static(void)
{
    ips2_fill_rect(0U, 0U, IPS2_WIDTH, IPS2_HEIGHT, IPS2_COLOR_BG);

    ips2_fill_rect(12U, 6U, 216U, 33U, IPS2_COLOR_CARD);
    ips2_fill_rect(12U, 6U, 5U, 33U, IPS2_COLOR_CYAN);
    ips2_draw_text(25U, 14U, "CLIMATE", 2U, IPS2_COLOR_SOFT_WHITE);

    ips2_fill_rect(12U, 47U, 101U, 166U, IPS2_COLOR_CARD);
    ips2_fill_rect(121U, 47U, 107U, 166U, IPS2_COLOR_CARD);
    ips2_draw_frame(12U, 47U, 101U, 166U, 1U, IPS2_COLOR_CARD_2);
    ips2_draw_frame(121U, 47U, 107U, 166U, 1U, IPS2_COLOR_CARD_2);

    ips2_draw_text_in_rect_centered(12U, 54U, 101U,
                                    "TEMPERATURE", 1U, IPS2_COLOR_MUTED);
    ips2_draw_text_in_rect_centered(121U, 54U, 107U,
                                    "HUMIDITY", 1U, IPS2_COLOR_MUTED);

    /* Thermometer shell and scale. */
    ips2_fill_rect(48U, 72U, 24U, 88U, IPS2_COLOR_SOFT_WHITE);
    ips2_fill_rect(54U, 77U, 12U, 81U, IPS2_COLOR_CARD);
    ips2_draw_filled_circle(60, 165, 25U, IPS2_COLOR_SOFT_WHITE);
    ips2_draw_filled_circle(60, 165, 19U, IPS2_COLOR_CARD);

    ips2_fill_rect(78U, 78U, 12U, 2U, IPS2_COLOR_MUTED);
    ips2_fill_rect(78U, 98U, 8U, 2U, IPS2_COLOR_MUTED);
    ips2_fill_rect(78U, 118U, 12U, 2U, IPS2_COLOR_MUTED);
    ips2_fill_rect(78U, 138U, 8U, 2U, IPS2_COLOR_MUTED);
    ips2_fill_rect(78U, 156U, 12U, 2U, IPS2_COLOR_MUTED);

    ips2_draw_text_centered(225U, "B3 - NEXT", 1U, IPS2_COLOR_MUTED);
}


/**
 * @brief Draw the animated thermometer and temperature value.
 * @param[in] temperature_tenths Temperature tenths value.
 * @param[in] valid Valid value.
 */
static void ips2_draw_temperature_dynamic(int16_t temperature_tenths,
                                           uint8_t valid)
{
    char text[10];
    int32_t clamped;
    uint16_t level;
    uint16_t liquid_color;

    ips2_fill_rect(54U, 77U, 12U, 81U, IPS2_COLOR_CARD);
    ips2_draw_filled_circle(60, 165, 19U, IPS2_COLOR_CARD);

    if (valid != 0U)
    {
        clamped = temperature_tenths;
        if (clamped < -100)
        {
            clamped = -100;
        }
        if (clamped > 500)
        {
            clamped = 500;
        }

        level = (uint16_t)(((clamped + 100L) * 77L) / 600L);
        liquid_color = ips2_temperature_color(temperature_tenths);

        if (level != 0U)
        {
            ips2_fill_rect(54U, (uint16_t)(158U - level),
                           12U, level, liquid_color);
        }
        ips2_draw_filled_circle(60, 165, 17U, liquid_color);
        ips2_draw_filled_circle(54, 159, 4U, IPS2_COLOR_WHITE);
        ips2_format_tenths(text, temperature_tenths, 'C');
    }
    else
    {
        text[0] = '-';
        text[1] = '-';
        text[2] = '.';
        text[3] = '-';
        text[4] = ' ';
        text[5] = 'C';
        text[6] = '\0';
    }

    ips2_fill_rect(18U, 190U, 89U, 18U, IPS2_COLOR_CARD);
    ips2_draw_text_in_rect_centered(18U, 193U, 89U,
                                    text, 2U, IPS2_COLOR_WHITE);
}


/**
 * @brief Draw the humidity droplet and its animated water level.
 * @param[in] humidity_tenths Humidity tenths value.
 * @param[in] valid Valid value.
 * @param[in] animation_ms Animation timebase in milliseconds.
 */
static void ips2_draw_humidity_drop(uint16_t humidity_tenths,
                                    uint8_t valid,
                                    uint32_t animation_ms)
{
    const uint16_t x0 = 128U;
    const uint16_t y0 = 67U;
    const uint16_t width = 92U;
    const uint16_t height = 111U;
    const int32_t center_x = 46;
    const int32_t tip_y = 1;
    const int32_t circle_y = 66;
    const int32_t radius = 42;
    const int32_t bottom_y = circle_y + radius;
    uint16_t fill_top;
    uint16_t py;
    uint16_t px;
    uint8_t wave_phase = (uint8_t)((animation_ms / 120UL) & 0x03U);
    char text[9];

    if (humidity_tenths > 1000U)
    {
        humidity_tenths = 1000U;
    }

    fill_top = (uint16_t)(bottom_y -
               (((uint32_t)(bottom_y - tip_y - 4) * humidity_tenths) / 1000UL));

    ips2_set_window(x0, y0,
                    (uint16_t)(x0 + width - 1U),
                    (uint16_t)(y0 + height - 1U));
    ips2_cs_low();
    ips2_dc_high();

    for (py = 0U; py < height; py++)
    {
        int32_t local_y = py;
        int32_t triangle_half = 0;
        int32_t circle_half = 0;
        int32_t outer_half;
        int32_t inner_half;

        if ((local_y >= tip_y) && (local_y <= circle_y))
        {
            triangle_half = ((local_y - tip_y) * radius) /
                            (circle_y - tip_y);
        }
        if ((local_y >= (circle_y - radius)) &&
            (local_y <= (circle_y + radius)))
        {
            int32_t dy = local_y - circle_y;
            circle_half = (int32_t)ips2_isqrt((uint32_t)
                          ((radius * radius) - (dy * dy)));
        }

        outer_half = (triangle_half > circle_half) ?
                     triangle_half : circle_half;
        inner_half = (outer_half > 3) ? (outer_half - 3) : 0;

        for (px = 0U; px < width; px++)
        {
            int32_t dx = (int32_t)px - center_x;
            int32_t absolute_dx = (dx < 0) ? -dx : dx;
            uint16_t color = IPS2_COLOR_CARD;

            if ((outer_half > 0) && (absolute_dx <= outer_half))
            {
                if ((inner_half == 0) || (absolute_dx > inner_half))
                {
                    color = IPS2_COLOR_CYAN;
                }
                else
                {
                    int32_t wave = 0;
                    if ((local_y >= ((int32_t)fill_top - 2)) &&
                        (local_y <= ((int32_t)fill_top + 2)))
                    {
                        wave = ((((px / 8U) + wave_phase) & 1U) != 0U) ? 1 : -1;
                    }

                    if ((valid != 0U) &&
                        (local_y >= ((int32_t)fill_top + wave)))
                    {
                        color = IPS2_COLOR_AQUA;
                    }

                    if ((dx >= -20) && (dx <= -15) &&
                        (local_y >= 50) && (local_y <= 78))
                    {
                        color = IPS2_COLOR_WHITE;
                    }
                }
            }

            ips2_spi_write_byte((uint8_t)(color >> 8));
            ips2_spi_write_byte((uint8_t)color);
        }
    }

    ips2_spi_wait_complete();
    ips2_cs_high();

    if (valid != 0U)
    {
        ips2_format_tenths(text, humidity_tenths, '%');
    }
    else
    {
        text[0] = '-';
        text[1] = '-';
        text[2] = '.';
        text[3] = '-';
        text[4] = ' ';
        text[5] = '%';
        text[6] = '\0';
    }

    ips2_fill_rect(127U, 190U, 95U, 18U, IPS2_COLOR_CARD);
    ips2_draw_text_in_rect_centered(127U, 193U, 95U,
                                    text, 2U, IPS2_COLOR_WHITE);
}


/**
 * @brief Render environment.
 * @param[in] environment Temperature and humidity measurement.
 * @param[in] ble_connected Current BLE connection state.
 * @param[in] animation_ms Animation timebase in milliseconds.
 */
void ips2_render_environment(const temphum27_data_t *environment,
                              uint8_t ble_connected,
                              uint32_t animation_ms)
{
    int16_t target_temperature = 0;
    uint16_t target_humidity = 0U;
    uint8_t valid = 0U;

    if (environment != 0)
    {
        target_temperature = environment->temperature_tenths_c;
        target_humidity = environment->humidity_tenths_percent;
        valid = environment->valid;
    }

    if (ips2_frame_drawn == 0U)
    {
        ips2_draw_environment_static();
        ips2_frame_drawn = 1U;
    }

    if (valid != 0U)
    {
        if (ips2_animated_temperature_tenths == (int16_t)0x7FFF)
        {
            ips2_animated_temperature_tenths = target_temperature;
        }
        else
        {
            ips2_animated_temperature_tenths =
                ips2_approach_i16(ips2_animated_temperature_tenths,
                                   target_temperature, 5);
        }

        if (ips2_animated_humidity_tenths == 0xFFFFU)
        {
            ips2_animated_humidity_tenths = target_humidity;
        }
        else
        {
            ips2_animated_humidity_tenths =
                ips2_approach_u16(ips2_animated_humidity_tenths,
                                  target_humidity, 10U);
        }
    }

    if ((valid != ips2_last_environment_valid) ||
        (ips2_animated_temperature_tenths !=
         ips2_last_drawn_temperature_tenths))
    {
        ips2_draw_temperature_dynamic(ips2_animated_temperature_tenths,
                                      valid);
        ips2_last_drawn_temperature_tenths =
            ips2_animated_temperature_tenths;
    }

    /* The water surface is deliberately redrawn to keep a subtle wave moving. */
    ips2_draw_humidity_drop(ips2_animated_humidity_tenths,
                            valid, animation_ms);
    ips2_last_drawn_humidity_tenths = ips2_animated_humidity_tenths;

    ips2_render_ble_status(ble_connected, APP_SCREEN_ENVIRONMENT);
    ips2_last_temperature_tenths = target_temperature;
    ips2_last_humidity_tenths = target_humidity;
    ips2_last_environment_valid = valid;
}


/**
 * @brief Convert the decoded RTC hour to a 24-hour value.
 * @param[in] datetime Decoded RTC date and time.
 * @return Calculated or converted value.
 */
static uint8_t ips2_datetime_hour24(const rtc14_datetime_t *datetime)
{
    uint8_t hour = datetime->hour;

    if (datetime->is_24_hour != 0U)
    {
        return (uint8_t)(hour % 24U);
    }

    hour = (uint8_t)(hour % 12U);
    if (datetime->is_pm != 0U)
    {
        hour = (uint8_t)(hour + 12U);
    }
    return hour;
}


/**
 * @brief Return the sky gradient color for a vertical scene coordinate.
 * @param[in] daylight Nonzero for the daytime scene.
 * @param[in] y Vertical display coordinate.
 * @return Calculated or converted value.
 */
static uint16_t ips2_scene_color_at_y(uint8_t daylight, uint16_t y)
{
    if (y < 55U)
    {
        return daylight ? IPS2_COLOR_SKY_DAY_TOP : IPS2_COLOR_SKY_NIGHT_TOP;
    }
    if (y < 105U)
    {
        return daylight ? IPS2_COLOR_SKY_DAY_MID : IPS2_COLOR_SKY_NIGHT_MID;
    }
    if (y < 149U)
    {
        return daylight ? IPS2_COLOR_SKY_DAY_LOW : IPS2_COLOR_SKY_NIGHT_LOW;
    }
    if (y < 177U)
    {
        return daylight ? IPS2_COLOR_HORIZON_DAY : IPS2_COLOR_HORIZON_NIGHT;
    }
    return IPS2_COLOR_BG;
}


/**
 * @brief Restore a rectangular area using the current sky gradient.
 * @param[in] x Horizontal display coordinate.
 * @param[in] y Vertical display coordinate.
 * @param[in] width Rectangle width in pixels.
 * @param[in] height Rectangle height in pixels.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_fill_scene_rect(uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height,
                                 uint8_t daylight)
{
    uint16_t row;
    uint16_t run_start = y;
    uint16_t current_color;

    if ((width == 0U) || (height == 0U) ||
        (x >= IPS2_WIDTH) || (y >= IPS2_HEIGHT))
    {
        return;
    }

    if ((uint32_t)y + height > IPS2_HEIGHT)
    {
        height = (uint16_t)(IPS2_HEIGHT - y);
    }

    current_color = ips2_scene_color_at_y(daylight, y);
    for (row = (uint16_t)(y + 1U); row < (uint16_t)(y + height); row++)
    {
        uint16_t color = ips2_scene_color_at_y(daylight, row);
        if (color != current_color)
        {
            ips2_fill_rect(x, run_start, width,
                           (uint16_t)(row - run_start), current_color);
            run_start = row;
            current_color = color;
        }
    }
    ips2_fill_rect(x, run_start, width,
                   (uint16_t)((y + height) - run_start), current_color);
}


/**
 * @brief Calculate the lowered celestial trajectory height for a phase.
 * @param[in] phase_minutes Minutes elapsed in the current 12-hour celestial phase.
 * @return Calculated or converted value.
 */
static uint16_t ips2_astronomy_arc_y(uint16_t phase_minutes)
{
    uint64_t numerator = 4ULL * phase_minutes *
                         (720U - phase_minutes) * 62ULL;
    uint32_t rise = (uint32_t)(numerator / (720ULL * 720ULL));
    return (uint16_t)(145U - rise);
}


/**
 * @brief Draw guide points along the sun/moon trajectory.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_draw_astronomy_arc(uint8_t daylight)
{
    uint16_t phase;
    uint16_t color = daylight ? IPS2_COLOR_SOFT_WHITE : IPS2_COLOR_MUTED;

    for (phase = 0U; phase <= 720U; phase = (uint16_t)(phase + 30U))
    {
        uint16_t x = (uint16_t)(24U +
                      (((uint32_t)phase * 192UL) / 720UL));
        uint16_t y = ips2_astronomy_arc_y(phase);
        ips2_fill_rect(x, y, 2U, 2U, color);
    }
}


/**
 * @brief Draw the static nighttime star field.
 */
static void ips2_draw_night_stars(void)
{
    static const uint8_t star_x[11] =
        { 18U, 40U, 65U, 92U, 118U, 145U, 171U, 198U, 218U, 32U, 184U };
    static const uint8_t star_y[11] =
        { 48U, 82U, 28U, 67U, 36U, 78U, 25U, 61U, 92U, 112U, 115U };
    uint8_t index;

    for (index = 0U; index < 11U; index++)
    {
        ips2_fill_rect(star_x[index], star_y[index], 2U, 2U,
                       IPS2_COLOR_SOFT_WHITE);
    }
}


/**
 * @brief Draw the day or night horizon silhouette.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_draw_horizon_silhouette(uint8_t daylight)
{
    uint16_t color = daylight ? IPS2_COLOR_DEEP_ORANGE : IPS2_COLOR_CARD_2;

    ips2_fill_rect(0U, 164U, 240U, 13U, color);
    ips2_fill_rect(18U, 155U, 42U, 9U, color);
    ips2_fill_rect(150U, 157U, 58U, 7U, color);
}


/**
 * @brief Draw all static elements of the Time of Day screen.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_draw_astronomy_static(uint8_t daylight)
{
    ips2_fill_scene_rect(0U, 0U, IPS2_WIDTH, 177U, daylight);
    ips2_fill_rect(0U, 177U, IPS2_WIDTH, 63U, IPS2_COLOR_BG);

    ips2_fill_rect(12U, 6U, 216U, 33U, IPS2_COLOR_CARD);
    ips2_fill_rect(12U, 6U, 5U, 33U,
                   daylight ? IPS2_COLOR_ORANGE : IPS2_COLOR_CYAN);
    ips2_draw_text_centered(14U, "TIME OF DAY", 2U, IPS2_COLOR_SOFT_WHITE);

    ips2_draw_astronomy_arc(daylight);
    if (daylight == 0U)
    {
        ips2_draw_night_stars();
    }

    ips2_draw_horizon_silhouette(daylight);

    ips2_fill_rect(20U, 184U, 200U, 38U, IPS2_COLOR_CARD);
    ips2_draw_frame(20U, 184U, 200U, 38U, 1U, IPS2_COLOR_CARD_2);
    ips2_draw_text_centered(228U, "B3 - NEXT", 1U, IPS2_COLOR_MUTED);
}


/**
 * @brief Restore the sky behind a previous celestial-body position.
 * @param[in] body_x Celestial-body center x-coordinate.
 * @param[in] body_y Celestial-body center y-coordinate.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_clear_celestial_region(uint16_t body_x, uint16_t body_y,
                                        uint8_t daylight)
{
    uint16_t x = (body_x > 30U) ? (uint16_t)(body_x - 30U) : 0U;
    uint16_t y = (body_y > 30U) ? (uint16_t)(body_y - 30U) : 0U;
    uint16_t width = 61U;
    uint16_t height = 61U;

    if ((uint32_t)x + width > IPS2_WIDTH)
    {
        width = (uint16_t)(IPS2_WIDTH - x);
    }
    if ((uint32_t)y + height > 177U)
    {
        height = (y < 177U) ? (uint16_t)(177U - y) : 0U;
    }

    ips2_fill_scene_rect(x, y, width, height, daylight);
}


/**
 * @brief Draw the transparent-background sun or crescent moon.
 * @param[in] body_x Celestial-body center x-coordinate.
 * @param[in] body_y Celestial-body center y-coordinate.
 * @param[in] daylight Nonzero for the daytime scene.
 */
static void ips2_draw_celestial_body(uint16_t body_x, uint16_t body_y,
                                     uint8_t daylight)
{
    if (daylight != 0U)
    {
        uint16_t ray = 25U;
        ips2_draw_line(body_x, (int32_t)body_y - 20,
                       body_x, (int32_t)body_y - ray,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line(body_x, (int32_t)body_y + 20,
                       body_x, (int32_t)body_y + ray,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x - 20, body_y,
                       (int32_t)body_x - ray, body_y,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x + 20, body_y,
                       (int32_t)body_x + ray, body_y,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x - 15, (int32_t)body_y - 15,
                       (int32_t)body_x - 21, (int32_t)body_y - 21,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x + 15, (int32_t)body_y - 15,
                       (int32_t)body_x + 21, (int32_t)body_y - 21,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x - 15, (int32_t)body_y + 15,
                       (int32_t)body_x - 21, (int32_t)body_y + 21,
                       2U, IPS2_COLOR_YELLOW);
        ips2_draw_line((int32_t)body_x + 15, (int32_t)body_y + 15,
                       (int32_t)body_x + 21, (int32_t)body_y + 21,
                       2U, IPS2_COLOR_YELLOW);

        ips2_draw_filled_circle(body_x, body_y,
                                16U,
                                IPS2_COLOR_YELLOW);
        ips2_draw_filled_circle((int32_t)body_x - 5,
                                (int32_t)body_y - 5,
                                4U, IPS2_COLOR_WHITE);
    }
    else
    {
        uint16_t sky = ips2_scene_color_at_y(0U, body_y);
        ips2_draw_filled_circle(body_x, body_y,
                                16U,
                                IPS2_COLOR_SOFT_WHITE);
        ips2_draw_filled_circle((int32_t)body_x + 7,
                                (int32_t)body_y - 5,
                                15U, sky);
        ips2_draw_filled_circle((int32_t)body_x - 5,
                                (int32_t)body_y - 5,
                                2U, IPS2_COLOR_WHITE);
    }
}


/**
 * @brief Render astronomy.
 * @param[in] datetime Decoded RTC date and time.
 * @param[in] ble_connected Current BLE connection state.
 * @param[in] animation_ms Animation timebase in milliseconds.
 */
void ips2_render_astronomy(const rtc14_datetime_t *datetime,
                            uint8_t ble_connected,
                            uint32_t animation_ms)
{
    uint8_t hour24;
    uint16_t minute_of_day;
    uint8_t daylight;
    uint16_t phase_minutes;
    uint16_t body_x;
    uint16_t body_y;
    char time_text[9] = "00:00:00";

    (void)animation_ms;

    if (datetime == 0)
    {
        ips2_render_error(ble_connected);
        return;
    }

    hour24 = ips2_datetime_hour24(datetime);
    minute_of_day = (uint16_t)((uint16_t)hour24 * 60U + datetime->minute);
    daylight = ((minute_of_day >= 360U) && (minute_of_day < 1080U)) ? 1U : 0U;

    if (daylight != 0U)
    {
        phase_minutes = (uint16_t)(minute_of_day - 360U);
    }
    else if (minute_of_day >= 1080U)
    {
        phase_minutes = (uint16_t)(minute_of_day - 1080U);
    }
    else
    {
        phase_minutes = (uint16_t)(minute_of_day + 360U);
    }

    body_x = (uint16_t)(24U +
             (((uint32_t)phase_minutes * 192UL) / 720UL));
    body_y = ips2_astronomy_arc_y(phase_minutes);

    if ((ips2_frame_drawn == 0U) ||
        (daylight != ips2_last_astronomy_mode))
    {
        ips2_draw_astronomy_static(daylight);
        ips2_frame_drawn = 1U;
        ips2_last_body_x = 0xFFFFU;
        ips2_last_body_y = 0xFFFFU;
        ips2_last_astronomy_mode = daylight;
    }

    if ((body_x != ips2_last_body_x) ||
        (body_y != ips2_last_body_y))
    {
        if (ips2_last_body_x != 0xFFFFU)
        {
            ips2_clear_celestial_region(ips2_last_body_x,
                                        ips2_last_body_y, daylight);
        }
        ips2_clear_celestial_region(body_x, body_y, daylight);
        ips2_draw_astronomy_arc(daylight);
        if (daylight == 0U)
        {
            ips2_draw_night_stars();
        }
        ips2_draw_horizon_silhouette(daylight);
        ips2_draw_celestial_body(body_x, body_y, daylight);
        ips2_last_body_x = body_x;
        ips2_last_body_y = body_y;
    }

    if ((hour24 != ips2_last_hour) ||
        (datetime->minute != ips2_last_minute) ||
        (datetime->second != ips2_last_second))
    {
        uint16_t time_width;
        uint16_t time_x;

        ips2_format_two_digits(&time_text[0], hour24);
        ips2_format_two_digits(&time_text[3], datetime->minute);
        ips2_format_two_digits(&time_text[6], datetime->second);
        time_width = ips2_text_width(time_text, 3U);
        time_x = (uint16_t)((IPS2_WIDTH - time_width) / 2U);
        ips2_draw_text_opaque(time_x, 193U, time_text, 3U,
                              IPS2_COLOR_WHITE, IPS2_COLOR_CARD);

        ips2_last_hour = hour24;
        ips2_last_minute = datetime->minute;
        ips2_last_second = datetime->second;
    }

    ips2_render_ble_status(ble_connected, APP_SCREEN_ASTRONOMY);
}


/**
 * @brief Draw all static elements of the stopwatch screen.
 */
static void ips2_draw_timer_static(void)
{
    ips2_fill_rect(0U, 0U, IPS2_WIDTH, IPS2_HEIGHT, IPS2_COLOR_BG);

    ips2_fill_rect(18U, 17U, 7U, 7U, IPS2_COLOR_CYAN);
    ips2_draw_text(32U, 14U, "STOPWATCH", 2U, IPS2_COLOR_SOFT_WHITE);
    ips2_fill_rect(18U, 39U, 204U, 2U, IPS2_COLOR_DARK_CYAN);

    ips2_draw_text_centered(61U, "HOURS   MIN   SEC", 1U, IPS2_COLOR_MUTED);
    ips2_fill_rect(18U, 76U, 204U, 54U, IPS2_COLOR_CARD);
    ips2_draw_frame(18U, 76U, 204U, 54U, 1U, IPS2_COLOR_CARD_2);

    ips2_fill_rect(55U, 146U, 130U, 54U, IPS2_COLOR_CARD_2);
    ips2_draw_frame(55U, 146U, 130U, 54U, 1U, IPS2_COLOR_DARK_CYAN);
    ips2_draw_text(151U, 174U, "MS", 2U, IPS2_COLOR_SOFT_WHITE);
}


/**
 * @brief Draw context-sensitive B3 and B4 stopwatch action hints.
 * @param[in] elapsed_ms Stopwatch elapsed time in milliseconds.
 * @param[in] running Nonzero when the stopwatch is running.
 */
static void ips2_draw_timer_actions(uint32_t elapsed_ms, uint8_t running)
{
    uint8_t b3_action;
    uint8_t b4_action;
    const char *b3_text;
    const char *b4_text;
    uint16_t b4_color;

    if (elapsed_ms == 0U)
    {
        b3_action = 0U;
        b3_text = "B3 - NEXT";
    }
    else
    {
        b3_action = 1U;
        b3_text = "B3 - RESET";
    }

    if (running != 0U)
    {
        b4_action = 1U;
        b4_text = "B4 - PAUSE";
        b4_color = IPS2_COLOR_ERROR;
    }
    else if (elapsed_ms == 0U)
    {
        b4_action = 0U;
        b4_text = "B4 - START";
        b4_color = IPS2_COLOR_CYAN;
    }
    else
    {
        b4_action = 2U;
        b4_text = "B4 - CONTINUE";
        b4_color = IPS2_COLOR_CYAN;
    }

    if (b4_action != ips2_last_timer_b4_action)
    {
        ips2_fill_rect(0U, 205U, IPS2_WIDTH, 15U, IPS2_COLOR_BG);
        ips2_draw_text_centered(209U, b4_text, 1U, b4_color);
        ips2_last_timer_b4_action = b4_action;
    }

    if (b3_action != ips2_last_timer_b3_action)
    {
        ips2_fill_rect(0U, 220U, IPS2_WIDTH, 15U, IPS2_COLOR_BG);
        ips2_draw_text_centered(224U, b3_text, 1U, IPS2_COLOR_MUTED);
        ips2_last_timer_b3_action = b3_action;
    }
}


/**
 * @brief Draw the current stopwatch state label.
 * @param[in] elapsed_ms Stopwatch elapsed time in milliseconds.
 * @param[in] running Nonzero when the stopwatch is running.
 */
static void ips2_draw_timer_state(uint32_t elapsed_ms, uint8_t running)
{
    uint8_t state;
    const char *text;
    uint16_t color;

    if (running != 0U)
    {
        state = 2U;
        text = "RUNNING";
        color = IPS2_COLOR_CYAN;
    }
    else if (elapsed_ms == 0U)
    {
        state = 0U;
        text = "READY";
        color = IPS2_COLOR_SOFT_WHITE;
    }
    else
    {
        state = 1U;
        text = "PAUSED";
        color = IPS2_COLOR_ERROR;
    }

    if (state != ips2_last_timer_state)
    {
        ips2_fill_rect(0U, 44U, IPS2_WIDTH, 13U, IPS2_COLOR_BG);
        ips2_draw_text_centered(47U, text, 1U, color);
        ips2_last_timer_state = state;
    }
}


/**
 * @brief Render timer.
 * @param[in] elapsed_ms Stopwatch elapsed time in milliseconds.
 * @param[in] running Nonzero when the stopwatch is running.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_timer(uint32_t elapsed_ms, uint8_t running,
                       uint8_t ble_connected)
{
    char clock_text[9] = "00:00:00";
    char ms_text[4] = "000";
    uint32_t total_seconds = elapsed_ms / 1000UL;
    uint32_t hours = (total_seconds / 3600UL) % 100UL;
    uint32_t minutes = (total_seconds / 60UL) % 60UL;
    uint32_t seconds = total_seconds % 60UL;
    uint32_t milliseconds = elapsed_ms % 1000UL;
    uint16_t progress_width = (uint16_t)((milliseconds * 180UL) / 999UL);

    if (ips2_frame_drawn == 0U)
    {
        ips2_draw_timer_static();
        ips2_frame_drawn = 1U;
    }

    ips2_render_ble_status(ble_connected, APP_SCREEN_STOPWATCH);
    ips2_draw_timer_state(elapsed_ms, running);
    ips2_draw_timer_actions(elapsed_ms, running);

    ips2_format_two_digits(&clock_text[0], hours);
    ips2_format_two_digits(&clock_text[3], minutes);
    ips2_format_two_digits(&clock_text[6], seconds);
    ips2_format_three_digits(ms_text, milliseconds);

    if ((hours != ips2_last_timer_hour) ||
        (minutes != ips2_last_timer_minute) ||
        (seconds != ips2_last_timer_second))
    {
        uint16_t clock_width = ips2_text_width(clock_text, 3U);
        uint16_t clock_x = (uint16_t)((IPS2_WIDTH - clock_width) / 2U);

        ips2_draw_text_opaque(clock_x, 91U, clock_text, 3U,
                              IPS2_COLOR_WHITE, IPS2_COLOR_CARD);
        ips2_last_timer_hour = (uint8_t)hours;
        ips2_last_timer_minute = (uint8_t)minutes;
        ips2_last_timer_second = (uint8_t)seconds;
    }

    ips2_draw_text_opaque(76U, 158U, ms_text, 4U,
                          IPS2_COLOR_CYAN, IPS2_COLOR_CARD_2);

    ips2_fill_rect(30U, 134U, 180U, 5U, IPS2_COLOR_DARK_CYAN);
    if (progress_width != 0U)
    {
        ips2_fill_rect(30U, 134U, progress_width, 5U, IPS2_COLOR_BLUE);
    }
}


/**
 * @brief Render error.
 * @param[in] ble_connected Current BLE connection state.
 */
void ips2_render_error(uint8_t ble_connected)
{
    if (ips2_frame_drawn == 0U)
    {
        ips2_fill_rect(0U, 0U, IPS2_WIDTH, IPS2_HEIGHT, IPS2_COLOR_BG);
        ips2_fill_rect(12U, 6U, 216U, 33U, IPS2_COLOR_CARD);
        ips2_fill_rect(12U, 6U, 5U, 33U, IPS2_COLOR_ERROR);
        ips2_draw_text_centered(14U, "TIME OF DAY", 2U, IPS2_COLOR_SOFT_WHITE);
        ips2_frame_drawn = 1U;
    }

    ips2_fill_rect(20U, 55U, 200U, 112U, IPS2_COLOR_CARD);
    ips2_draw_frame(20U, 55U, 200U, 112U, 1U, IPS2_COLOR_ERROR);
    ips2_draw_text_centered(80U, "RTC", 4U, IPS2_COLOR_ERROR);
    ips2_draw_text_centered(125U, "READ ERROR", 2U,
                            IPS2_COLOR_SOFT_WHITE);
    ips2_draw_text_centered(228U, "B3 - NEXT", 1U, IPS2_COLOR_MUTED);
    ips2_render_ble_status(ble_connected, APP_SCREEN_ASTRONOMY);
}
/**
 * @brief Initialize the module.
 * @param[in] pclka_hz Peripheral clock A frequency in hertz.
 * @return Zero on success; otherwise a negative error code.
 */
int ips2_init(uint32_t pclka_hz)
{
    static const uint8_t porctrl[5] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
    static const uint8_t pwctrl1[2] = { 0xA4, 0xA1 };
    static const uint8_t positive_gamma[14] =
    {
        0xD0, 0x08, 0x0E, 0x09, 0x09, 0x05, 0x31,
        0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34
    };
    static const uint8_t negative_gamma[14] =
    {
        0xD0, 0x08, 0x0E, 0x09, 0x09, 0x15, 0x31,
        0x33, 0x48, 0x17, 0x14, 0x15, 0x31, 0x34
    };
    uint8_t value;

    ips2_configure_pins();
    ips2_cs_high();
    ips2_dc_high();
    ips2_bck_low();

    if (ips2_spi_init(pclka_hz) != 0)
    {
        return -1;
    }

    ips2_rst_low();
    ips2_delay_ms(2U);
    ips2_rst_high();
    ips2_delay_ms(200U);

    ips2_write_command(IPS2_CMD_SLPOUT);
    ips2_delay_ms(120U);

    value = 0x00U;
    ips2_write_command_data(IPS2_CMD_MADCTL, &value, 1U);
    value = 0x05U;
    ips2_write_command_data(IPS2_CMD_COLMOD, &value, 1U);
    ips2_write_command_data(IPS2_CMD_PORCTRL, porctrl, 5U);
    value = 0x35U;
    ips2_write_command_data(IPS2_CMD_GCTRL, &value, 1U);
    value = 0x3AU;
    ips2_write_command_data(IPS2_CMD_VCOMS, &value, 1U);
    value = 0x01U;
    ips2_write_command_data(IPS2_CMD_VDVVRHEN, &value, 1U);
    value = 0x19U;
    ips2_write_command_data(IPS2_CMD_VRHS, &value, 1U);
    value = 0x20U;
    ips2_write_command_data(IPS2_CMD_VDVSET, &value, 1U);
    value = 0x0FU;
    ips2_write_command_data(IPS2_CMD_FRCTR2, &value, 1U);
    ips2_write_command_data(IPS2_CMD_PWCTRL1, pwctrl1, 2U);
    ips2_write_command_data(IPS2_CMD_PVGAMCTRL, positive_gamma, 14U);
    ips2_write_command_data(IPS2_CMD_NVGAMCTRL, negative_gamma, 14U);
    ips2_write_command(IPS2_CMD_INVON);

    ips2_fill_rect(0U, 0U, IPS2_WIDTH, IPS2_HEIGHT, IPS2_COLOR_BG);
    ips2_write_command(IPS2_CMD_DISPON);
    ips2_delay_ms(100U);
    ips2_bck_high();

    ips2_frame_drawn = 0U;
    return 0;
}
