#include "system_ch32h417.h"
#include "ch32h417.h"

uint32_t SystemCoreClock = 100000000u;
uint32_t SystemClock = 400000000u;
uint32_t HCLKClock = 100000000u;

void SystemInit(void)
{
    uint32_t flash_temp;

    RCC->CTLR |= (uint32_t)0x00000001;
    RCC->CFGR0 &= (uint32_t)0x305C0000;
    while((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x00)
    {
    }

    RCC->CFGR0 &= (uint32_t)0xFFBFFFFF;
    RCC->PLLCFGR &= (uint32_t)0x7FFFFFFF;
    RCC->CTLR &= (uint32_t)0x6AA6FFFF;
    RCC->CTLR &= (uint32_t)0xFFFBFFFF;
    RCC->PLLCFGR &= (uint32_t)0x0FFFC000;
    RCC->PLLCFGR |= (uint32_t)0x00000004;
    RCC->INTR = 0x00FF0000;
    RCC->CFGR2 &= 0x0C600000;
    RCC->PLLCFGR2 &= 0xFFF0E080;
    RCC->PLLCFGR2 |= 0x00080020;

    RCC->PLLCFGR &= (uint32_t)(~RCC_PLLMUL);
    RCC->PLLCFGR |= (uint32_t)RCC_PLLMUL16;
    RCC->PLLCFGR &= (uint32_t)(~RCC_PLL_SRC_DIV);
    RCC->PLLCFGR |= (uint32_t)RCC_PLL_SRC_DIV1;
    RCC->PLLCFGR &= (uint32_t)(~RCC_PLLSRC);
    RCC->PLLCFGR |= (uint32_t)RCC_PLLSRC_HSI;
    while((RCC->PLLCFGR & (uint32_t)RCC_PLLSRC) != (uint32_t)RCC_PLLSRC_HSI)
    {
    }

    RCC->CTLR |= RCC_PLLON;
    while((RCC->CTLR & RCC_PLLRDY) != (uint32_t)RCC_PLLRDY)
    {
    }

    RCC->PLLCFGR &= (uint32_t)(~RCC_SYSPLL_GATE);
    RCC->PLLCFGR &= (uint32_t)(~RCC_SYSPLL_SEL);
    while((RCC->PLLCFGR & (uint32_t)RCC_SYSPLL_SEL) != (uint32_t)0x00)
    {
    }

    RCC->CFGR0 &= (uint32_t)(~RCC_HPRE);
    RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
    RCC->CFGR0 &= (uint32_t)(~RCC_FPRE);
    RCC->CFGR0 |= (uint32_t)RCC_FPRE_DIV4;

    flash_temp = FLASH->ACTLR;
    flash_temp &= ~((uint32_t)0x3);
    flash_temp |= FLASH_ACTLR_LATENCY_HCLK_DIV2;
    FLASH->ACTLR = flash_temp;

    RCC->PLLCFGR |= (uint32_t)RCC_SYSPLL_GATE;
    RCC->CFGR0 &= (uint32_t)(~RCC_SW);
    RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
    while((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)RCC_SWS_PLL)
    {
    }

    SystemAndCoreClockUpdate();
}

void SystemAndCoreClockUpdate(void)
{
    SystemClock = 400000000u;
    HCLKClock = 100000000u;
    SystemCoreClock = 100000000u;
}
