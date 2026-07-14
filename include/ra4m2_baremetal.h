/**
 * @file ra4m2_baremetal.h
 * @brief Shared RA4M2 register definitions used by the local bare-metal drivers.
 */

#ifndef RA4M2_BAREMETAL_H
#define RA4M2_BAREMETAL_H

#include "project_platform.h"

/* PmnPFS fields. */
#define PFS_PODR_HIGH                    (1UL << 0)
#define PFS_PDR_OUTPUT                   (1UL << 2)
#define PFS_PCR_PULLUP                   (1UL << 4)
#define PFS_NCODR_OPEN_DRAIN             (1UL << 6)
#define PFS_ISEL_IRQ                     (1UL << 14)
#define PFS_PMR_PERIPHERAL               (1UL << 16)
#define PFS_PSEL(value)                  ((uint32_t)(value) << 24)

/* IIC register bits. */
#define IIC_ICCR1_IICRST                 (1U << 6)
#define IIC_ICCR1_ICE                    (1U << 7)
#define IIC_ICCR2_ST                     (1U << 1)
#define IIC_ICCR2_RS                     (1U << 2)
#define IIC_ICCR2_SP                     (1U << 3)
#define IIC_ICCR2_TRS                    (1U << 5)
#define IIC_ICCR2_BBSY                   (1U << 7)
#define IIC_ICSR2_STOP                   (1U << 3)
#define IIC_ICSR2_NACKF                  (1U << 4)
#define IIC_ICSR2_RDRF                   (1U << 5)
#define IIC_ICSR2_TEND                   (1U << 6)
#define IIC_ICSR2_TDRE                   (1U << 7)
#define IIC_ICMR3_NF_MASK                (0x03U)
#define IIC_ICMR3_ACKBT                  (1U << 3)
#define IIC_ICMR3_ACKWP                  (1U << 4)
#define IIC_ICMR3_WAIT                   (1U << 6)
#define IIC_ICFER_NFE                    (1U << 5)
#define IIC_ICFER_SCLE                   (1U << 6)
#define IIC_ICMR1_CKS_SHIFT              (4U)

/* AGT and ICU fields. */
#define AGT_AGTCR_TCSTF                  (1U << 1)
#define AGT_AGTCR_STATUS_FLAGS           (0xF0U)
#define AGT_AGTCR_START_TIMER            (0xF1U)
#define AGT_AGTMR1_TCK_PCLKB_DIV_1       (0x00U)
#define AGT_AGTMR1_TCK_PCLKB_DIV_8       (0x01U)
#define AGT_AGTMR1_TCK_PCLKB_DIV_2       (0x03U)
#define ICU_IELSR_IR                     (1UL << 16)
#define ICU_IRQCR_FALLING_EDGE           (0x00U)
#define AGT0_MSTPCRD_BIT                 (3U)

/* SCI fields shared by SCI0 SPI and SCI3 UART. */
#define SCI_SCR_RE                       (1U << 4)
#define SCI_SCR_TE                       (1U << 5)
#define SCI_SCR_RIE                      (1U << 6)
#define SCI_SSR_TEND                     (1U << 2)
#define SCI_SSR_PER                      (1U << 3)
#define SCI_SSR_FER                      (1U << 4)
#define SCI_SSR_ORER                     (1U << 5)
#define SCI_SSR_RDRF                     (1U << 6)
#define SCI_SSR_TDRE                     (1U << 7)
#define SCI_SCMR_SMIF                    (1U << 0)
#define SCI_SCMR_SINV                    (1U << 2)
#define SCI_SCMR_SDIR                    (1U << 3)
#define SCI_SCMR_CHR1                    (1U << 4)
#define SCI_SIMR1_IICM                   (1U << 0)
#define SCI_SPMR_CKPOL                   (1U << 6)
#define SCI_SPMR_CKPH                    (1U << 7)
#define SCI_SMR_CKS_MASK                 (0x03U)
#define SCI_SMR_STOP                     (1U << 3)
#define SCI_SMR_PM                       (1U << 4)
#define SCI_SMR_PE                       (1U << 5)
#define SCI_SMR_CHR                      (1U << 6)
#define SCI_SMR_CM                       (1U << 7)
#define SCI_SEMR_BRME                    (1U << 2)
#define SCI_SEMR_ABCSE                   (1U << 3)
#define SCI_SEMR_ABCS                    (1U << 4)
#define SCI_SEMR_BGDM                    (1U << 6)
#define SCI_FCR_FM                       (1U << 0)

/** @brief Register view for a 16-bit AGT channel. */
typedef struct
{
    volatile uint16_t AGT;
    volatile uint16_t AGTCMA;
    volatile uint16_t AGTCMB;
    volatile uint8_t reserved0[2];
    volatile uint8_t AGTCR;
    volatile uint8_t AGTMR1;
    volatile uint8_t AGTMR2;
    volatile uint8_t reserved1;
    volatile uint8_t AGTIOC;
    volatile uint8_t AGTISR;
    volatile uint8_t AGTCMSR;
    volatile uint8_t AGTIOSEL;
} ra4m2_agt16_registers_t;

/** @brief Register view shared by RA SCI channels used in this example. */
typedef struct
{
    volatile uint8_t smr;
    volatile uint8_t brr;
    volatile uint8_t scr;
    volatile uint8_t tdr;
    volatile uint8_t ssr;
    volatile uint8_t rdr;
    volatile uint8_t scmr;
    volatile uint8_t semr;
    volatile uint8_t snfr;
    volatile uint8_t simr1;
    volatile uint8_t simr2;
    volatile uint8_t simr3;
    volatile uint8_t sisr;
    volatile uint8_t spmr;
    volatile uint16_t tdrhl;
    volatile uint16_t rdrhl;
    volatile uint8_t mddr;
    volatile uint8_t dccr;
    volatile uint16_t fcr;
    volatile uint16_t fdr;
    volatile uint16_t lsr;
    volatile uint16_t cdr;
    volatile uint8_t sptr;
} ra4m2_sci_registers_t;

#endif /* RA4M2_BAREMETAL_H */
