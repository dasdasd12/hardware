#include "h417_common.h"

#include <string.h>

#include "ch32h417_usbfs_device.h"
#include "usbd_compatibility_hid.h"
#include "system_ch32h417.h"

enum
{
    H417_USBFS_OFFICIAL_PHASE_INIT = 30,
    H417_USBFS_OFFICIAL_PHASE_ENABLED = 31,
    H417_USBFS_OFFICIAL_PHASE_ENUMERATED = 32,
};

uint8_t HID_Report_Buffer[64];
volatile uint8_t HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;

static void h417_usbfs_official_var_init(void)
{
    memset((void *)&RingBuffer_Comm, 0, sizeof(RingBuffer_Comm));
    memset((void *)HID_Report_Buffer, 0, sizeof(HID_Report_Buffer));
    HID_Set_Report_Flag = SET_REPORT_DEAL_OVER;
}

void Delay_Us(uint32_t n)
{
    while(n--)
    {
        h417_delay_cycles(SystemCoreClock / 4000000u);
    }
}

void Delay_Ms(uint32_t n)
{
    while(n--)
    {
        Delay_Us(1000u);
    }
}

void h417_usbfs_official_hid_run(void)
{
    h417_status_phase(H417_USBFS_OFFICIAL_PHASE_INIT, 0u);
    h417_usbfs_official_var_init();

    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);
    h417_status_phase(H417_USBFS_OFFICIAL_PHASE_ENABLED, 0u);

    while(1)
    {
        g_h417_status.cycle++;
        g_h417_status.last_item =
            ((uint32_t)USBFSD->INT_FG << 24) |
            ((uint32_t)USBFSD->INT_ST << 16) |
            ((uint32_t)USBFSD->BASE_CTRL << 8) |
            (uint32_t)USBFSD->UDEV_CTRL;

        if(USBFS_DevEnumStatus != 0u)
        {
            h417_status_pass(H417_USBFS_OFFICIAL_PHASE_ENUMERATED);
        }
        else
        {
            h417_status_phase(H417_USBFS_OFFICIAL_PHASE_ENABLED, USBFS_DevConfig);
        }

        h417_delay_cycles(200000u);
    }
}
