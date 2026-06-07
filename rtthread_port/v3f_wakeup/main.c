#include "ch32h417.h"
#include "adc.h"

#define V5F_START_ADDR      0x00010000u
#define V3F_BOOT_TRACE_BASE ((volatile uint32_t *)0x20178000u)
#define V3F_BOOT_MAGIC      0x56334642u /* "V3FB" */

/* Shared-SRAM USBSS-PLL-ready flag polled by V5F before touching USBSSD.
   Magic chosen to be obvious in `mdw 0x20178020 1`. Distinct from boot
   trace block above. */
#define V3F_USBSS_FLAG_ADDR ((volatile uint32_t *)0x20178020u)
#define V3F_USBSS_READY     0xABCD1234u
#define V3F_USBSS_FAILED    0xDEADBEEFu

/* Sized to ~50 ms at V3F 100 MHz; official examples poll without bound. */
#define V3F_USBSS_PLL_TIMEOUT 0x00200000u

#ifndef V3F_ENABLE_ADC_INIT
#define V3F_ENABLE_ADC_INIT 0
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
    V3F_STAGE_IDLE_LOOP,
};

/* Dummy to satisfy system_ch32h417.c without pulling full SPL */
void GPIO_IPD_Unused(void) {}

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

int main(void)
{
    V3F_BOOT_TRACE_BASE[4] = 0;
    *V3F_USBSS_FLAG_ADDR = 0;
    V3F_BootTrace(V3F_STAGE_ENTER_MAIN);

    /* Configure the system clock from the 25 MHz HSE before waking V5F. */
    SystemInit();
    V3F_BootTrace(V3F_STAGE_SYSTEM_INIT_DONE);

    /* Enable PWR clock so STOP mode can be entered correctly */
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    V3F_BootTrace(V3F_STAGE_PWR_CLOCK_DONE);

    /* Wake V5F core: start execution at 0x00010000 */
    NVIC_WakeUp_V5F(V5F_START_ADDR);
    V3F_BootTrace(V3F_STAGE_V5F_WAKE_DONE);

    /* EVT examples set SCTLR bit 4 after wake-up; required for V5F
       debug module activation and proper dual-core hand-off. */
    NVIC->SCTLR |= 1 << 4;
    V3F_BootTrace(V3F_STAGE_SCTLR_DEBUG_DONE);

#if V3F_ENABLE_ADC_INIT
    /* Initialize ADC1 for magnetic axis hall sensor acquisition.
       This is the low-level foundation; continuous matrix scan
       will replace the idle loop below in the next iteration. */
    ADC_Hall_Init();
    V3F_BootTrace(V3F_STAGE_ADC_DONE);
#endif

    /* USBSS PHY/PLL bring-up: per WCH USBSS examples this MUST run on V3F
       (USBSS PLL/PHY domain). V5F-side CherryUSB polls the magic at
       V3F_USBSS_FLAG_ADDR before any USBSSD register access. */
    V3F_BootTrace(V3F_STAGE_USBSS_PLL_BEGIN);
    if (V3F_USBSS_PLL_Init())
    {
        *V3F_USBSS_FLAG_ADDR = V3F_USBSS_READY;
        V3F_BootTrace(V3F_STAGE_USBSS_PLL_DONE);
    }
    else
    {
        *V3F_USBSS_FLAG_ADDR = V3F_USBSS_FAILED;
        V3F_BootTrace(V3F_STAGE_USBSS_PLL_TIMEOUT);
    }

    /* TODO: Magnetic axis scan engine
       - Configure MUX GPIOs
       - Set up DMA ring buffer in RAM_SHARED (0x20178000)
       - 1 kHz scan loop: select row -> ADC burst -> store frame -> IPC notify V5F
       Temporary root-cause verification test: keep V3F awake while checking
       whether V3F sleep causes USBSS PLL lock timeout. */
    while (1)
    {
        V3F_BOOT_TRACE_BASE[1] = V3F_STAGE_IDLE_LOOP;
        V3F_BOOT_TRACE_BASE[4]++;
        __NOP();
    }

    return 0;
}
