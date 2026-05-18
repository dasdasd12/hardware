#include "ch32h417.h"

/* Dummy to satisfy system_ch32h417.c without pulling full SPL */
void GPIO_IPD_Unused(void) {}

/* Symbols required by core_riscv.h __WFE() when linked with ch32h417_pwr.c */
volatile uint32_t WFE_MASK = 0;
volatile uint32_t WFE_WkupSource = 0;

int main(void)
{
    /* Configure the system clock from the 25 MHz HSE before waking V5F. */
    SystemInit();

    /* Enable PWR clock so STOP mode can be entered correctly */
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);

    /* Wake V5F core: start execution at 0x00010000 */
    NVIC_WakeUp_V5F(0x00010000);

    /* EVT examples set SCTLR bit 4 after wake-up; required for V5F
       debug module activation and proper dual-core hand-off. */
    NVIC->SCTLR |= 1 << 4;

    /* V3F stays in a simple idle loop to avoid STOP mode issues
       that can interfere with V5F wake-up and debug attachment. */
    while (1)
    {
        __WFI();
    }

    return 0;
}
