/*******************************************************************************
 * rf_receiver_main.c - CH585 2.4G receiver with USBHS HID keyboard output.
 *
 * This project is a USBHS backup/upgrade path for CH585M_RF_RX_USB.
 * The RF receiver part stays the same; only the USB output layer changes from
 * USB FS Boot Keyboard to USBHS NKRO Keyboard.
 *******************************************************************************/

#include "CH58x_common.h"
#include "CONFIG.h"
#include "HAL.h"
#include "rf_receiver.h"
#include "ch585_usbhs_kbd.h"
#include "usb_hid.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#define USBHS_SELF_TEST_ENABLE 0

#if USBHS_SELF_TEST_ENABLE
#define HID_KEY_MIN    0x04
#define HID_KEY_1      0x1E
#define HID_KEY_ENTER  0x28
#define HID_KEY_H      0x0B
#define HID_KEY_S      0x16
#define HID_NKRO_BYTES 14

static void build_nkro_key_report(uint8_t keycode, uint8_t *report)
{
    for(uint8_t i = 0; i < 16; i++)
    {
        report[i] = 0;
    }

    if(keycode >= HID_KEY_MIN)
    {
        uint8_t bit_index = keycode - HID_KEY_MIN;
        uint8_t byte_index = bit_index / 8;
        uint8_t bit_mask = (uint8_t)(1u << (bit_index % 8));

        if(byte_index < HID_NKRO_BYTES)
        {
            report[2 + byte_index] = bit_mask;
        }
    }
}

static void wait_usbhs_ep1_idle(void)
{
    uint16_t timeout_ms = 50;

    while((USBHS_Endp_Busy[DEF_UEP1] & DEF_UEP_BUSY) && timeout_ms)
    {
        DelayMs(1);
        timeout_ms--;
    }
}

static void usbhs_self_test_tap(uint8_t keycode)
{
    uint8_t report[16];

    build_nkro_key_report(keycode, report);
    USB_HID_SendKeyboard(report);
    wait_usbhs_ep1_idle();

    build_nkro_key_report(0, report);
    USB_HID_SendKeyboard(report);
    wait_usbhs_ep1_idle();

    DelayMs(30);
}

static void usbhs_self_test_once(void)
{
    static uint8_t done = 0;

    if(done || !USBHS_DevEnumStatus)
    {
        return;
    }

    done = 1;
    DelayMs(500);

    usbhs_self_test_tap(HID_KEY_H);
    usbhs_self_test_tap(HID_KEY_S);
    usbhs_self_test_tap(HID_KEY_1);
    usbhs_self_test_tap(HID_KEY_ENTER);
}
#endif

/*******************************************************************************
 * Main_Circulation
 ******************************************************************************/
__HIGH_CODE
__attribute__((noinline))
void Main_Circulation(void)
{
    uint8_t nkro16[16];
    uint16_t consumer_usage;

    while(1)
    {
        TMOS_SystemProcess();
        RF_Receiver_ServiceUsbHsReport();
#if USBHS_SELF_TEST_ENABLE
        usbhs_self_test_once();
#endif

        if(RF_Receiver_GetKbdReport(nkro16))
        {
            USB_HID_SendKeyboard(nkro16);

            if(RF_Receiver_GetConsumer(&consumer_usage))
            {
                USB_HID_SendConsumer(consumer_usage);
            }
        }
    }
}

/*******************************************************************************
 * main
 ******************************************************************************/
int main(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    CH58x_BLEInit();
    HAL_Init();

    USB_HID_Init();
    RF_Receiver_Init();

    Main_Circulation();

    return 0;
}
