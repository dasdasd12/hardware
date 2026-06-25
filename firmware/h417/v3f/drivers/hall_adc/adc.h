/********************************** (C) COPYRIGHT *******************************
 * File Name          : adc.h
 * Author             : AI Assistant
 * Version            : V1.0.0
 * Date               : 2026/05/18
 * Description        : ADC low-level driver for magnetic axis keyboard scan (V3F)
 *********************************************************************************/
#ifndef __ADC_H__
#define __ADC_H__

#include "ch32h417.h"
#include <stdint.h>

/* ADC resolution */
#define ADC_RESOLUTION     12
#define ADC_MAX_VALUE      ((1U << ADC_RESOLUTION) - 1)

/* Reference voltage in millivolts (typical 3.3V) */
#define ADC_VREF_MV        3300

/* Supported ADC channels for magnetic axis (example mapping, extend as needed) */
#define ADC_CH_HALL_0      ADC_Channel_0   /* PA0 */
#define ADC_CH_HALL_1      ADC_Channel_1   /* PA1 */
#define ADC_CH_HALL_2      ADC_Channel_2   /* PA2 */
#define ADC_CH_HALL_3      ADC_Channel_3   /* PA3 */
#define ADC_CH_HALL_4      ADC_Channel_4   /* PA4 */
#define ADC_CH_HALL_5      ADC_Channel_5   /* PA5 */
#define ADC_CH_HALL_6      ADC_Channel_6   /* PA6 */
#define ADC_CH_HALL_7      ADC_Channel_7   /* PA7 */

/* Function prototypes */
void ADC_Hall_Init(void);
uint16_t ADC_Hall_SingleConvert(uint8_t channel);
void ADC_Hall_DMA_Init(uint16_t *buffer, uint16_t count);
void ADC_Hall_DMA_Start(uint8_t first_channel, uint8_t num_channels);
void ADC_Hall_DMA_Stop(void);
uint8_t ADC_Hall_DMA_IsBusy(void);

#endif /* __ADC_H__ */
