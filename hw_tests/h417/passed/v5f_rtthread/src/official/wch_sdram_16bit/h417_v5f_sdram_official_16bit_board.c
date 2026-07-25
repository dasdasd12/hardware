#include "ch32h417_fmc.h"
#include "ch32h417_gpio.h"
#include "ch32h417_pwr.h"
#include "ch32h417_rcc.h"

extern void SDRAM_Initialization_Sequence(void);

static void h417_v5f_sdram_official_16bit_gpio_af(GPIO_TypeDef *port,
                                                  uint16_t pin,
                                                  uint8_t pin_source,
                                                  uint8_t gpio_af)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_PinAFConfig(port, pin_source, gpio_af);
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(port, &GPIO_InitStructure);
}

static void h417_v5f_sdram_official_16bit_gpio_config(void)
{
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                          RCC_HB2Periph_GPIOA |
                          RCC_HB2Periph_GPIOB |
                          RCC_HB2Periph_GPIOC |
                          RCC_HB2Periph_GPIOD |
                          RCC_HB2Periph_GPIOE |
                          RCC_HB2Periph_GPIOF,
                          ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_VIO1V8_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VIO3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_VDD3V3_IO_HSLV, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_PD0PD1, ENABLE);

    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF9);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOC, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF3);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOF, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF3);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOF, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOA, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOC, GPIO_Pin_2, GPIO_PinSource2, GPIO_AF15);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_3, GPIO_PinSource3, GPIO_AF1);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF7);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOF, GPIO_Pin_5, GPIO_PinSource5, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOA, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF15);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_6, GPIO_PinSource6, GPIO_AF11);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOB, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_12, GPIO_PinSource12, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_14, GPIO_PinSource14, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOF, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF2);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_7, GPIO_PinSource7, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_9, GPIO_PinSource9, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_11, GPIO_PinSource11, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOC, GPIO_Pin_9, GPIO_PinSource9, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_0, GPIO_PinSource0, GPIO_AF1);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_1, GPIO_PinSource1, GPIO_AF1);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOE, GPIO_Pin_15, GPIO_PinSource15, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_8, GPIO_PinSource8, GPIO_AF12);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOA, GPIO_Pin_13, GPIO_PinSource13, GPIO_AF0);
    h417_v5f_sdram_official_16bit_gpio_af(GPIOD, GPIO_Pin_10, GPIO_PinSource10, GPIO_AF12);
}

void h417_v5f_sdram_official_16bit_init(void)
{
    FMC_SDRAM_InitTypeDef SDRAMInitStructure = {0};
    FMC_SDRAM_TimingTypeDef SDRAM_Timing = {0};

    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    PWR_VIO18ModeCfg(PWR_VIO18CFGMODE_SW);
    PWR_VIO18LevelCfg(PWR_VIO18Level_MODE3);

    RCC_HBPeriphClockCmd(RCC_HBPeriph_FMC, ENABLE);
    h417_v5f_sdram_official_16bit_gpio_config();

    SDRAMInitStructure.FMC_Bank = FMC_Bank5_SDRAM;
    SDRAMInitStructure.FMC_ColumnBitsNumber = FMC_ColumnBitsNumber_9;
    SDRAMInitStructure.FMC_RowBitsNumber = FMC_ROWBitsNumber_13;
    SDRAMInitStructure.FMC_MemoryDataWidth = FMC_MemoryDataWidth_16;
    SDRAMInitStructure.FMC_InternalBankNumber = FMC_InternalBankNumber_4;
    SDRAMInitStructure.FMC_CASLatency = FMC_CASLatency_3CLk;
    SDRAMInitStructure.FMC_WriteProtection = FMC_WriteProtection_Disable;
    SDRAMInitStructure.FMC_SDClockPeriod = 1;
    SDRAMInitStructure.FMC_ReadBurst = FMC_ReadBurst_Disable;
    SDRAMInitStructure.FMC_ReadPipeDelay = FMC_ReadPipeDelay_none;
    SDRAMInitStructure.FMC_PHASE_SEL = 0xa;

    SDRAM_Timing.FMC_LoadToActiveDelay = 2;
    SDRAM_Timing.FMC_ExitSelfRefreshDelay = 8;
    SDRAM_Timing.FMC_SelfRefreshTime = 5;
    SDRAM_Timing.FMC_RowCycleDelay = 6;
    SDRAM_Timing.FMC_WriteRecoveryTime = 2;
    SDRAM_Timing.FMC_RPDelay = 2;
    SDRAM_Timing.FMC_RCDDelay = 2;
    SDRAMInitStructure.FMC_SDRAM_Timing = &SDRAM_Timing;

    FMC_SDRAM_Init(&SDRAMInitStructure);
    FMC_Bank5_6->MISC |= (1 << 15);
    FMC_Bank5_6->MISC |= (1 << 16);
    SDRAM_Initialization_Sequence();

    FMC_Bank1->BTCR[0] |= (1 << 24);
}
