#include "h417_usbfs_cdc_diag.h"

#include <string.h>

#include "ch32h417_usbfs_device.h"
#include "ch32h417_usbfs_hid_nkro.h"
#include "ch32h417_usbhs_hid_nkro.h"

extern volatile uint16_t USBFS_CDC_ControlLineState;

static uint8_t s_last_dtr;
static uint8_t s_advance_pending;

void h417_usbfs_cdc_diag_init(void)
{
    /* Preserve the proven product order, including UTMI and USBHS setup. */
    ch32h417_usbhs_hid_nkro_init();
    ch32h417_usbfs_hid_nkro_init();
    NVIC_SetPriority(USBHS_IRQn, 0x00U);
    NVIC_SetPriority(USBFS_IRQn, 0x80U);
    s_last_dtr = 0U;
    s_advance_pending = 0U;
}

void h417_usbfs_cdc_diag_poll_control(void)
{
    uint8_t dtr = (uint8_t)(USBFS_CDC_ControlLineState & 0x0001U);

    if((dtr != 0U) && (s_last_dtr == 0U))
    {
        s_advance_pending = 1U;
    }
    s_last_dtr = dtr;
}

uint8_t h417_usbfs_cdc_diag_take_advance(void)
{
    uint8_t pending = s_advance_pending;

    s_advance_pending = 0U;
    return pending;
}

uint8_t h417_usbfs_cdc_diag_ready(void)
{
    return USBFS_CDC_Debug_IsOpen();
}

h417_usbfs_cdc_diag_command_t h417_usbfs_cdc_diag_take_command(void)
{
    uint32_t irq_was_enabled;
    uint8_t packet_index;
    uint8_t packet_length;
    h417_usbfs_cdc_diag_command_t command = H417_USBFS_CDC_DIAG_COMMAND_NONE;

    if(USBFS_RingBuffer_Comm.RemainPack == 0U)
    {
        return H417_USBFS_CDC_DIAG_COMMAND_NONE;
    }

    irq_was_enabled = NVIC_GetStatusIRQ(USBFS_IRQn);
    NVIC_DisableIRQ(USBFS_IRQn);

    if(USBFS_RingBuffer_Comm.RemainPack != 0U)
    {
        packet_index = USBFS_RingBuffer_Comm.DealPtr;
        packet_length = USBFS_RingBuffer_Comm.PackLen[packet_index];
        if((packet_length >= 5U) &&
           (memcmp(&USBFS_Data_Buffer[packet_index * DEF_USBD_FS_PACK_SIZE],
                   "START",
                   5U) == 0))
        {
            command = H417_USBFS_CDC_DIAG_COMMAND_START;
        }
        else if((packet_length >= 4U) &&
                (memcmp(&USBFS_Data_Buffer[packet_index * DEF_USBD_FS_PACK_SIZE],
                        "NEXT",
                        4U) == 0))
        {
            command = H417_USBFS_CDC_DIAG_COMMAND_NEXT;
        }

        USBFS_RingBuffer_Comm.PackLen[packet_index] = 0U;
        USBFS_RingBuffer_Comm.DealPtr++;
        if(USBFS_RingBuffer_Comm.DealPtr == DEF_Ring_Buffer_Max_Blks)
        {
            USBFS_RingBuffer_Comm.DealPtr = 0U;
        }
        USBFS_RingBuffer_Comm.RemainPack--;

        if((USBFS_RingBuffer_Comm.StopFlag != 0U) &&
           (USBFS_RingBuffer_Comm.RemainPack <
            (DEF_Ring_Buffer_Max_Blks - DEF_RING_BUFFER_REMINE)))
        {
            USBFS_RingBuffer_Comm.StopFlag = 0U;
            USBFSD->UEP1_RX_CTRL =
                (USBFSD->UEP1_RX_CTRL & ~USBFS_UEP_R_RES_MASK) |
                USBFS_UEP_R_RES_ACK;
        }
    }

    if(irq_was_enabled != 0U)
    {
        NVIC_EnableIRQ(USBFS_IRQn);
    }
    return command;
}

uint8_t h417_usbfs_cdc_diag_try_write(const uint8_t *data, uint16_t length)
{
    uint32_t irq_was_enabled;
    uint8_t sent;

    irq_was_enabled = NVIC_GetStatusIRQ(USBFS_IRQn);
    NVIC_DisableIRQ(USBFS_IRQn);
    sent = USBFS_CDC_Debug_Send(data, length);
    if(irq_was_enabled != 0U)
    {
        NVIC_EnableIRQ(USBFS_IRQn);
    }
    return sent;
}

uint8_t h417_usbfs_cdc_diag_tx_idle(void)
{
    if(USBFS_CDC_Debug_IsOpen() == 0U)
    {
        return 0U;
    }
    return (uint8_t)(((USBFSD->UEP3_TX_CTRL & USBFS_UEP_T_RES_MASK) ==
                      USBFS_UEP_T_RES_NAK) ? 1U : 0U);
}

void h417_usbfs_cdc_diag_quiesce_usbhs(void)
{
    NVIC_DisableIRQ(USBHS_IRQn);
    NVIC_ClearPendingIRQ(USBHS_IRQn);
    USBHSD->INT_EN = 0U;
    USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
    RCC_HBPeriphClockCmd(RCC_HBPeriph_USBHS, DISABLE);
}
