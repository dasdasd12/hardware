/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Description        : CH585M minimal connectable BLE HID bring-up
 *******************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "ble_hid.h"
#include "hiddev.h"
#include "kvm_control.h"
#include "usb_cdc_debug.h"

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
        USB_CDC_DebugProcess();
        TMOS_SystemProcess();
    }
}

static void Debug_UART0_Init(void)
{
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
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

static void Debug_UART1_WriteLiteral(const char *text, uint16_t len)
{
    UART1_SendString((uint8_t *)text, len);
}

#define DEBUG_UART1_LITERAL(text) Debug_UART1_WriteLiteral((text), (uint16_t)(sizeof(text) - 1))

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

    Debug_UART0_Init();
    Debug_UART1_Init();
    DEBUG_UART1_LITERAL("\r\nCH585 BLE self-test boot UART1 PA9/PA8\r\n");
    DEBUG_UART1_LITERAL("BLE name: CH585_LX_TEST\r\n");

#ifdef DEBUG
    PRINT("%s\n", VER_LIB);
#endif

    USB_CDC_DebugInit();
    CH58x_BLEInit();
    HAL_Init();
    GAPRole_PeripheralInit();
    HidDev_Init();
    BLE_HID_Init();
    KVM_ControlInit();
    DEBUG_UART1_LITERAL("BLE init done\r\n");

    Main_Circulation();
    return 0;
}
