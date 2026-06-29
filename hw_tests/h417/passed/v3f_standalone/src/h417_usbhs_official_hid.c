#include "h417_common.h"

#include <string.h>

#include "ch32h417_gpio.h"
#include "ch32h417_usbhs_device.h"
#include "usbd_compatibility_hid.h"

#ifndef H417_USBHS_DISABLE_SWD
#define H417_USBHS_DISABLE_SWD 0
#endif

enum
{
    H417_USBHS_OFFICIAL_PHASE_INIT = 40,
    H417_USBHS_OFFICIAL_PHASE_ENABLED = 41,
    H417_USBHS_OFFICIAL_PHASE_ENUMERATED = 42,
};

uint8_t HID_Report_Buffer[DEF_USBD_HS_PACK_SIZE + 1];
volatile uint16_t Data_Pack_Max_Len = 0;
volatile uint16_t Head_Pack_Len = 0;
volatile uint8_t HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

static void h417_usbhs_official_var_init(void)
{
    memset((void *)&RingBuffer_Comm, 0, sizeof(RingBuffer_Comm));
    memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
    Data_Pack_Max_Len = 0;
    Head_Pack_Len = 0;
    HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;
}

static void h417_usbhs_official_optional_disable_swd(void)
{
#if H417_USBHS_DISABLE_SWD
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOB, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
#endif
}

void h417_usbhs_official_hid_run(void)
{
    h417_status_phase(H417_USBHS_OFFICIAL_PHASE_INIT, 0u);
    h417_usbhs_official_var_init();
    h417_usbhs_official_optional_disable_swd();

    USBHS_Device_Init(ENABLE);
    h417_status_phase(H417_USBHS_OFFICIAL_PHASE_ENABLED, 0u);

    while(1)
    {
        g_h417_status.cycle++;
        g_h417_status.last_item =
            ((uint32_t)USBHSD->INT_FG << 24) |
            ((uint32_t)USBHSD->INT_ST << 16) |
            ((uint32_t)USBHSD->BASE_MODE << 8) |
            (uint32_t)USBHSD->CONTROL;

        if(USBHS_DevEnumStatus != 0u)
        {
            h417_status_pass(H417_USBHS_OFFICIAL_PHASE_ENUMERATED);
        }
        else
        {
            h417_status_phase(H417_USBHS_OFFICIAL_PHASE_ENABLED,
                              ((uint32_t)USBHS_DevSpeed << 8) | USBHS_DevConfig);
        }

        h417_delay_cycles(200000u);
    }
}
