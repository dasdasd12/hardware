/********************************** (C) COPYRIGHT *******************************
 * File Name          : adc.c
 * Author             : AI Assistant
 * Version            : V1.0.0
 * Date               : 2026/05/18
 * Description        : ADC low-level driver for magnetic axis keyboard scan (V3F)
 *                      - Single-channel software trigger for calibration / debug
 *                      - DMA burst mode for matrix scan frame acquisition
 *********************************************************************************/

#include "adc.h"

/* Internal flag: ADC calibration done */
static volatile uint8_t adc_calibrated = 0;

/* DMA buffer pointer (valid after ADC_Hall_DMA_Init) */
static uint16_t *adc_dma_buffer = 0;
static uint16_t adc_dma_count = 0;
static volatile uint8_t adc_dma_busy = 0;

/*********************************************************************
 * @fn      ADC_USBHS_PLL_Init
 *
 * @brief   Ensure USBHS PLL is ready for ADC clock source.
 *          V3F does not initialize USBHS PLL by default.
 *
 * @return  none
 */
static void ADC_USBHS_PLL_Init(void)
{
    /* If already ready, skip */
    if ((RCC->CTLR & RCC_USBHS_PLLRDY) == RCC_USBHS_PLLRDY)
    {
        return;
    }

    /* Select HSI as USBHS PLL clock source */
    RCC->PLLCFGR2 &= (uint32_t)((uint32_t) ~(RCC_USBHSPLL_REFSEL));
    RCC->PLLCFGR2 &= (uint32_t)((uint32_t) ~(RCC_USBHSPLLSRC));
    RCC->PLLCFGR2 |= (uint32_t)RCC_USBHSPLLSRC_HSI;
    while ((RCC->PLLCFGR2 & (uint32_t)RCC_USBHSPLLSRC) != (uint32_t)0x01)
    {
    }

    /* Enable USBHS PLL */
    RCC->CTLR |= (uint32_t)RCC_USBHS_PLLON;
    while ((RCC->CTLR & (uint32_t)RCC_USBHS_PLLRDY) != (uint32_t)RCC_USBHS_PLLRDY)
    {
    }
}

/*********************************************************************
 * @fn      ADC_Hall_Init
 *
 * @brief   Initialize ADC1 for magnetic axis hall sensor sampling.
 *          Clock: USBHSPLL / 36 (safe speed for calibration).
 *          Mode: Independent, single conversion, right aligned, 12-bit.
 *
 * @return  none
 */
void ADC_Hall_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};

    ADC_USBHS_PLL_Init();

    /* Enable ADC1 and GPIOA clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_ADC1 | RCC_HB2Periph_GPIOA, ENABLE);

    /* Configure ADC clock from USBHS PLL, divide by 36 for calibration */
    RCC_ADCCLKConfig(RCC_ADCCLKSource_USBHSPLL);
    RCC_ADCUSBHSPLLCLKAsSourceConfig(RCC_USBHS_Div36);

    /* Reset ADC1 */
    ADC_DeInit(ADC1);

    /* Independent mode, single channel, software trigger, right align */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* Disable low power mode for higher speed */
    ADC_LowPowerModeCmd(ADC1, DISABLE);

    /* Enable ADC1 */
    ADC_Cmd(ADC1, ENABLE);

    /* Disable output buffer (recommended for hall sensors) */
    ADC_BufferCmd(ADC1, DISABLE);

    /* Calibration */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    adc_calibrated = 1;
}

/*********************************************************************
 * @fn      ADC_Hall_SingleConvert
 *
 * @brief   Software-trigger single conversion on specified channel.
 *          Blocks until EOC (with timeout).
 *
 * @param   channel - ADC_Channel_0 .. ADC_Channel_17
 *
 * @return  12-bit ADC value (0..4095), or 0xFFFF on timeout
 */
uint16_t ADC_Hall_SingleConvert(uint8_t channel)
{
    uint32_t timeout;

    if (!adc_calibrated)
    {
        return 0xFFFF;
    }

    /* Configure regular channel: rank 1, sample time mode 5 */
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_CyclesMode5);

    /* Clear EOC */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);

    /* Start conversion */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* Wait for EOC with timeout (~1M cycles @100MHz ~10ms) */
    timeout = 0x100000;
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
    {
        if (--timeout == 0)
        {
            return 0xFFFF;
        }
    }

    return ADC_GetConversionValue(ADC1);
}

/*********************************************************************
 * @fn      ADC_Hall_DMA_Init
 *
 * @brief   Initialize DMA1 Channel1 for ADC1 burst transfer.
 *          Request ID 0x78 via DMAMUX Channel1.
 *
 * @param   buffer - destination RAM buffer (halfword array)
 * @param   count  - number of halfwords to transfer per burst
 *
 * @return  none
 */
void ADC_Hall_DMA_Init(uint16_t *buffer, uint16_t count)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    if (buffer == 0 || count == 0)
    {
        return;
    }

    adc_dma_buffer = buffer;
    adc_dma_count = count;

    /* Enable DMA1 clock */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);

    /* De-init channel */
    DMA_DeInit(DMA1_Channel1);

    /* Peripheral -> Memory, halfword, increment memory */
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = count;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    /* DMAMUX: ADC1 request = 0x78 on DMA1 Channel1 -> Mux Channel1 */
    DMA_MuxChannelConfig(DMA_MuxChannel1, 0x78);
}

/*********************************************************************
 * @fn      ADC_Hall_DMA_Start
 *
 * @brief   Start a DMA burst acquisition of N consecutive channels
 *          starting from first_channel.
 *          This is a simplified version: scan mode should be enabled
 *          and channels configured via ADC_RegularChannelConfig for
 *          each rank before calling.  Here we configure one channel
 *          and rely on the caller to re-trigger for each MUX column.
 *
 * @param   first_channel - starting ADC channel
 * @param   num_channels  - number of channels (reserved for future scan mode)
 *
 * @return  none
 */
void ADC_Hall_DMA_Start(uint8_t first_channel, uint8_t num_channels)
{
    (void)num_channels; /* reserved for scan mode sequencing */

    if (adc_dma_buffer == 0 || adc_dma_count == 0)
    {
        return;
    }

    adc_dma_busy = 1;

    /* Clear DMA TC flag */
    DMA_ClearFlag(DMA1, DMA1_FLAG_TC1);

    /* Enable DMA channel */
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* Enable ADC DMA request */
    ADC_DMACmd(ADC1, ENABLE);

    /* Configure the channel */
    ADC_RegularChannelConfig(ADC1, first_channel, 1, ADC_SampleTime_CyclesMode5);

    /* Trigger conversion */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/*********************************************************************
 * @fn      ADC_Hall_DMA_Stop
 *
 * @brief   Stop DMA and ADC DMA request.
 *
 * @return  none
 */
void ADC_Hall_DMA_Stop(void)
{
    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    adc_dma_busy = 0;
}

/*********************************************************************
 * @fn      ADC_Hall_DMA_IsBusy
 *
 * @brief   Check whether a DMA transfer is in progress.
 *
 * @return  1 if busy, 0 if idle
 */
uint8_t ADC_Hall_DMA_IsBusy(void)
{
    if (!adc_dma_busy)
    {
        return 0;
    }

    /* Poll DMA TC flag */
    if (DMA_GetFlagStatus(DMA1, DMA1_FLAG_TC1) != RESET)
    {
        adc_dma_busy = 0;
        DMA_ClearFlag(DMA1, DMA1_FLAG_TC1);
        return 0;
    }

    return 1;
}
