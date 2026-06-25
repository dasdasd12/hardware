/********************************** (C) COPYRIGHT *******************************
 * File Name          : SLEEP.h
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2018/11/12
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
#ifndef __SLEEP_H
#define __SLEEP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * GLOBAL VARIABLES
 */
typedef void (*pfnLowPowerGapProcessCB_t)( void );

/*********************************************************************
 * FUNCTIONS
 */

/**
 * @brief   配置睡眠唤醒的方式   - RTC唤醒，触发模式
 */
extern void HAL_SleepInit(void);

/**
 * @brief   启动睡眠
 *
 * @param   time    - 唤醒的时间点（RTC绝对值）
 *
 * @return  state.
 */
extern uint32_t CH58x_LowPower(uint32_t time);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
