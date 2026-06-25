/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main_broadcaster_selftest.c
 * Description        : CH585M BLE advertiser self-test with UART1 boot banner.
 *******************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "broadcaster.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

__HIGH_CODE
__attribute__((noinline))
static void Main_Circulation(void)
{
    while(1)
    {
        TMOS_SystemProcess();
    }
}

static void Debug_UART1_Init(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOPinRemap(DISABLE, RB_PIN_UART1);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
    UART1_BaudRateCfg(115200);
}

static void Debug_UART1_WriteLiteral(const char *text)
{
    const char *p = text;
    while(*p)
    {
        p++;
    }
    UART1_SendString((uint8_t *)text, (uint16_t)(p - text));
}

int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif

    Debug_UART1_Init();
    Debug_UART1_WriteLiteral("\r\nCH585 BLE advertiser self-test boot\r\n");
    Debug_UART1_WriteLiteral("UART1: PA9 TX / PA8 RX, 115200\r\n");
    Debug_UART1_WriteLiteral("BLE name: CH585_LX_ADV1\r\n");

    CH58x_BLEInit();
    HAL_Init();
    GAPRole_BroadcasterInit();
    Broadcaster_Init();

    Debug_UART1_WriteLiteral("BLE advertiser init done\r\n");
    Main_Circulation();
    return 0;
}
