#include "CH58x_common.h"
#include <string.h>

#define USB_HS_CONFIG_RESP_LEN 64

void Config_ProcessUSBCommand(const uint8_t *cmd_buf, uint8_t *resp_buf)
{
    (void)cmd_buf;

    if(resp_buf)
    {
        memset(resp_buf, 0, USB_HS_CONFIG_RESP_LEN);
        resp_buf[0] = 0xA5;
        resp_buf[1] = 0x5A;
    }
}
