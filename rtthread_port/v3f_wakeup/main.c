#include "ch32h417.h"
#include "ch32h417_usb.h"
#include "ch32h417_dbgmcu.h"
#include "debug.h"
#include "adc.h"

#if defined(V3F_USBSS_OFFICIAL_OWNER) && (V3F_USBSS_OFFICIAL_OWNER != 0)
#include "ch32h417_usbss_device.h"
#include "ch32h417_usbhs_device.h"
#endif

#define V5F_START_ADDR      0x00010000u
#define V3F_BOOT_TRACE_BASE ((volatile uint32_t *)0x20178000u)
#define V3F_BOOT_MAGIC      0x56334642u /* "V3FB" */

/* Shared-SRAM USBSS-PLL-ready flag polled by V5F before touching USBSSD.
   Magic chosen to be obvious in `mdw 0x20178020 1`. Distinct from boot
   trace block above. */
#define V3F_USBSS_FLAG_ADDR ((volatile uint32_t *)0x20178020u)
#define V3F_USBSS_READY     0xABCD1234u
#define V3F_USBSS_FAILED    0xDEADBEEFu
#define V3F_USBFS_CFGR2_MASK  (RCC_USBFSSRC | RCC_USBFSDIV)
#define V3F_USBFS_CFGR2_48M   (RCC_USBFSSRC_USBHSPLL | RCC_USBFSDIV_DIV10)
#define V3F_USBSS_PHY_CFG_CR  (*((__IO uint32_t *)0x400341F8U))
#define V3F_USBSS_PHY_CFG_DAT (*((__IO uint32_t *)0x400341FCU))

/* Sized to ~50 ms at V3F 100 MHz; official examples poll without bound. */
#define V3F_USBSS_PLL_TIMEOUT 0x00200000u
#define V3F_USBSS_LINK_PROBE_DELAY 0x00100000u
#define V3F_USBSS_SWJ_DELAY_LOOPS 16u
#define V3F_USBSS_SWJ_DELAY_CYCLES 50000000u

#ifndef V3F_ENABLE_ADC_INIT
#define V3F_ENABLE_ADC_INIT 0
#endif

#ifndef V3F_ENABLE_USBFS_CLOCK_INIT
#define V3F_ENABLE_USBFS_CLOCK_INIT 0
#endif

enum
{
    V3F_STAGE_ENTER_MAIN = 1,
    V3F_STAGE_SYSTEM_INIT_DONE,
    V3F_STAGE_PWR_CLOCK_DONE,
    V3F_STAGE_V5F_WAKE_DONE,
    V3F_STAGE_SCTLR_DEBUG_DONE,
    V3F_STAGE_ADC_DONE,
    V3F_STAGE_USBSS_PLL_BEGIN,
    V3F_STAGE_USBSS_PLL_DONE,
    V3F_STAGE_USBSS_PLL_TIMEOUT,
    V3F_STAGE_USBFS_CLOCK_DONE,
    V3F_STAGE_USBFS_CLOCK_TIMEOUT,
    V3F_STAGE_USBFS_CLOCK_SKIPPED,
    V3F_STAGE_IDLE_LOOP,
    V3F_STAGE_USBSS_LINK_PROBE_DONE,
    V3F_STAGE_USBSS_SWJ_DISABLED,
    V3F_STAGE_USBSS_SWJ_DELAY,
    V3F_STAGE_USBSS_OFFICIAL_OWNER_DONE,
};

/* Symbols required by core_riscv.h __WFE() when linked with ch32h417_pwr.c */
volatile uint32_t WFE_MASK = 0;
volatile uint32_t WFE_WkupSource = 0;

static void V3F_BootTrace(uint32_t stage)
{
    V3F_BOOT_TRACE_BASE[0] = V3F_BOOT_MAGIC;
    V3F_BOOT_TRACE_BASE[1] = stage;
    V3F_BOOT_TRACE_BASE[2] = NVIC->WAKEIP[1];
    V3F_BOOT_TRACE_BASE[3] = NVIC->SCTLR;
}

static void V3F_ClearUsbTrace(void)
{
    uint32_t i;

    for (i = 5U; i <= 23U; i++)
    {
        V3F_BOOT_TRACE_BASE[i] = 0U;
    }
}

/* Bring up the USBSS PLL / PHY clock tree on V3F.
   Mirrors Common/ch32h417_usbss_device.c::USBSS_RCC_Init(ENABLE). The PLL
   refer clock (25 MHz HSE) was already programmed by SystemInit() via
   RCC->PLLCFGR2 |= 0x20. Returns 1 on PLL lock, 0 on timeout. */
static int V3F_USBSS_PLL_Init(void)
{
    uint32_t timeout;

    /* USBSS_PLL_Init(ENABLE): start PLL and wait for lock. Bounded wait so
       a missing crystal / wrong refer config does not hang V3F forever. */
    RCC->CTLR |= (uint32_t)RCC_USBSS_PLLON;
    for (timeout = 0; timeout < V3F_USBSS_PLL_TIMEOUT; timeout++)
    {
        if ((RCC->CTLR & (uint32_t)RCC_USBSS_PLLRDY) == (uint32_t)RCC_USBSS_PLLRDY)
        {
            break;
        }
    }
    if (timeout >= V3F_USBSS_PLL_TIMEOUT)
    {
        return 0;
    }

    /* HBPeriph USBSS clock + PIPE + UTMI + USBSS PLL output */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBSS, ENABLE);
    RCC_PIPECmd(ENABLE);
    RCC_UTMIcmd(ENABLE);
    RCC_USBSS_PLLCmd(ENABLE);
    return 1;
}

static uint32_t V3F_USBSS_PHY_Cfg(uint8_t port_num, uint8_t addr, uint16_t data)
{
    if (port_num != 0U)
    {
        return 0U;
    }

    V3F_USBSS_PHY_CFG_CR = (1UL << 23) | ((uint32_t)addr << 16) | data;
    V3F_USBSS_PHY_CFG_DAT = 0x01U;
    return V3F_USBSS_PHY_CFG_DAT;
}

static void V3F_USBSS_CFG_MOD(void)
{
    V3F_USBSS_PHY_Cfg(0U, 0x03U, 0x7C12U);
    V3F_USBSS_PHY_Cfg(0U, 0x0DU, 0x79AAU);
    V3F_USBSS_PHY_Cfg(0U, 0x15U, 0x4430U);
    V3F_USBSS_PHY_Cfg(0U, 0x13U, 0x0010U);
    *((__IO uint32_t *)0x5003C018U) = 0xB0054000U;

    V3F_BOOT_TRACE_BASE[9] = V3F_USBSS_PHY_CFG_DAT;
    V3F_BOOT_TRACE_BASE[10] = *((__IO uint32_t *)0x5003C018U);
}

static void V3F_USBSS_Disable_SWJ(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOB, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
    V3F_BootTrace(V3F_STAGE_USBSS_SWJ_DISABLED);
}

static void V3F_USBSS_LinkProbe(void)
{
    uint32_t delay;
    uint32_t chip;

    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE |
                       LINK_TOUT_MODE;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;
    chip = (DBGMCU_GetCHIPID() >> 4) & 0x0FU;
    V3F_BOOT_TRACE_BASE[19] = chip;
    V3F_BOOT_TRACE_BASE[20] = USBSSD->LINK_STATUS;

    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
    V3F_BOOT_TRACE_BASE[21] = USBSSD->LINK_STATUS;
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT |
                            LINK_IE_STATE_CHG | LINK_IE_WARM_RST | LINK_IE_TERM_PRES;
    if (chip >= 3U)
    {
        USBSSD->LINK_INT_CTRL |= LINK_IE_RX_SET_FC;
    }
    V3F_BOOT_TRACE_BASE[23] = USBSSD->LINK_INT_CTRL;
    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120U;
    USBSSD->LINK_U1_WKUP_FILTER = 50U;
    USBSSD->LINK_U2_WKUP_FILTER = 0U;
    USBSSD->LINK_U3_WKUP_FILTER = 0U;
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS |
                          USBSS_DMA_EN | USBSS_SETUP_FLOW;
    V3F_USBSS_CFG_MOD();

    V3F_BOOT_TRACE_BASE[11] = USBSSD->LINK_CFG;
    V3F_BOOT_TRACE_BASE[12] = USBSSD->LINK_CTRL;
    V3F_BOOT_TRACE_BASE[13] = USBSSD->LINK_STATUS;

    for (delay = 0U; delay < V3F_USBSS_LINK_PROBE_DELAY; delay++)
    {
        __NOP();
    }

    V3F_BOOT_TRACE_BASE[14] = USBSSD->LINK_STATUS;
    V3F_BOOT_TRACE_BASE[15] = USBSSD->LINK_INT_FLAG;
    V3F_BOOT_TRACE_BASE[16] = USBSSD->LINK_LPM_CR;
    V3F_BOOT_TRACE_BASE[17] = USBSSD->USB_CONTROL;
    V3F_BOOT_TRACE_BASE[18] = USBSSD->USB_STATUS;
    V3F_BootTrace(V3F_STAGE_USBSS_LINK_PROBE_DONE);
}

static int V3F_USBFS_Clock_Init(void)
{
    uint32_t timeout;

    if ((RCC->CTLR & (uint32_t)RCC_USBHS_PLLRDY) == 0U)
    {
        RCC->CTLR |= (uint32_t)RCC_USBHS_PLLON;
        for (timeout = 0; timeout < V3F_USBSS_PLL_TIMEOUT; timeout++)
        {
            if ((RCC->CTLR & (uint32_t)RCC_USBHS_PLLRDY) == (uint32_t)RCC_USBHS_PLLRDY)
            {
                break;
            }
        }
        if (timeout >= V3F_USBSS_PLL_TIMEOUT)
        {
            return 0;
        }
    }

    RCC->CFGR2 = (RCC->CFGR2 & (uint32_t)~V3F_USBFS_CFGR2_MASK) | (uint32_t)V3F_USBFS_CFGR2_48M;
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, ENABLE);

    V3F_BOOT_TRACE_BASE[5] = RCC->CFGR2;
    V3F_BOOT_TRACE_BASE[6] = RCC->CTLR;
    V3F_BOOT_TRACE_BASE[7] = RCC->HBPCENR;
    return ((RCC->CFGR2 & (uint32_t)V3F_USBFS_CFGR2_MASK) == (uint32_t)V3F_USBFS_CFGR2_48M);
}

static void V3F_Delay(uint32_t cycles)
{
    volatile uint32_t i;

    for (i = 0; i < cycles; i++)
    {
        __NOP();
    }
}

static void V3F_USBSS_SWJ_DownloadWindow(void)
{
    uint32_t i;

    V3F_BootTrace(V3F_STAGE_USBSS_SWJ_DELAY);
    for (i = 0U; i < V3F_USBSS_SWJ_DELAY_LOOPS; i++)
    {
        V3F_BOOT_TRACE_BASE[22] = i + 1U;
        V3F_Delay(V3F_USBSS_SWJ_DELAY_CYCLES);
    }
}

#if defined(V3F_USBSS_OFFICIAL_OWNER) && (V3F_USBSS_OFFICIAL_OWNER != 0)
static void V3F_USBSS_OfficialService(void)
{
    if (USBHS_DevEnumStatus != 0U)
    {
        if (RingBuffer_Comm.RemainPack != 0U)
        {
            if ((USBHSD->UEP1_TX_CTRL & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_NAK)
            {
                USBHSD->UEP1_TX_DMA =
                    (uint32_t)&Data_Buffer[(RingBuffer_Comm.DealPtr) * DEF_USBD_HS_PACK_SIZE];
                USBHSD->UEP1_TX_LEN = RingBuffer_Comm.PackLen[RingBuffer_Comm.DealPtr];
                USBHSD->UEP1_TX_CTRL =
                    (USBHSD->UEP1_TX_CTRL & (uint8_t)~USBHS_UEP_T_RES_MASK) |
                    USBHS_UEP_T_RES_ACK;

                NVIC_DisableIRQ(USBHS_IRQn);
                RingBuffer_Comm.RemainPack--;
                RingBuffer_Comm.DealPtr++;
                if (RingBuffer_Comm.DealPtr == DEF_Ring_Buffer_Max_Blks)
                {
                    RingBuffer_Comm.DealPtr = 0;
                }
                NVIC_EnableIRQ(USBHS_IRQn);
            }
        }

        if (RingBuffer_Comm.RemainPack < (DEF_Ring_Buffer_Max_Blks - DEF_RING_BUFFER_RESTART))
        {
            if (RingBuffer_Comm.StopFlag != 0U)
            {
                RingBuffer_Comm.StopFlag = 0;
                USBHSD->UEP1_RX_CTRL =
                    (USBHSD->UEP1_RX_CTRL & (uint8_t)~USBHS_UEP_R_RES_MASK) |
                    USBHS_UEP_R_RES_ACK;
            }
        }
    }
}
#endif

int main(void)
{
    V3F_BOOT_TRACE_BASE[4] = 0;
    V3F_ClearUsbTrace();
    *V3F_USBSS_FLAG_ADDR = 0;
    V3F_BootTrace(V3F_STAGE_ENTER_MAIN);

    /* Minimal boot-core path: keep V3F boring until V5F UART logs are proven.
       USBSS ownership, SWJ remap and ADC scan setup will be reintroduced after
       the dual-core boot path is stable on the real board. */
    SystemInit();
    V3F_BootTrace(V3F_STAGE_SYSTEM_INIT_DONE);

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    V3F_BootTrace(V3F_STAGE_PWR_CLOCK_DONE);

#if V3F_ENABLE_USBFS_CLOCK_INIT
    if (V3F_USBFS_Clock_Init() != 0)
    {
        V3F_BootTrace(V3F_STAGE_USBFS_CLOCK_DONE);
    }
    else
    {
        V3F_BootTrace(V3F_STAGE_USBFS_CLOCK_TIMEOUT);
    }
#else
    V3F_BOOT_TRACE_BASE[5] = RCC->CFGR2;
    V3F_BOOT_TRACE_BASE[6] = RCC->CTLR;
    V3F_BOOT_TRACE_BASE[7] = RCC->HBPCENR;
    V3F_BootTrace(V3F_STAGE_USBFS_CLOCK_SKIPPED);
#endif

    NVIC_WakeUp_V5F(V5F_START_ADDR);
    V3F_BootTrace(V3F_STAGE_V5F_WAKE_DONE);

    NVIC->SCTLR |= 1 << 4;
    V3F_BootTrace(V3F_STAGE_SCTLR_DEBUG_DONE);

    while (1)
    {
        V3F_BOOT_TRACE_BASE[1] = V3F_STAGE_IDLE_LOOP;
        V3F_BOOT_TRACE_BASE[4]++;
        __NOP();
    }

    return 0;
}
