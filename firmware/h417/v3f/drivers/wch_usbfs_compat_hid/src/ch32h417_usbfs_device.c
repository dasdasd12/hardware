/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32h417_usbhs_device.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/05/28
* Description        : This file provides all the USBHS firmware functions.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32h417_usbfs_device.h"
#include "usb_desc.h"
#include "ch32h417_usb.h"
#include "usbd_compatibility_hid.h"
/*******************************************************************************/
/* Variable Definition */


/* Global */
const uint8_t    *pUSBFS_Descr;

/* Setup Request */
volatile uint8_t  USBFS_SetupReqCode;
volatile uint8_t  USBFS_SetupReqType;
volatile uint16_t USBFS_SetupReqValue;
volatile uint16_t USBFS_SetupReqIndex;
volatile uint16_t USBFS_SetupReqLen;

/* USB Device Status */
volatile uint8_t  USBFS_DevConfig;
volatile uint8_t  USBFS_DevAddr;
volatile uint8_t  USBFS_DevSleepStatus;
volatile uint8_t  USBFS_DevEnumStatus;

/* HID Class Command */
volatile uint8_t USBFS_HidIdle;
volatile uint8_t USBFS_HidProtocol;

#if V3F_ENABLE_USBFS_CDC_DEBUG
uint8_t USBFS_CDC_LineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
volatile uint16_t USBFS_CDC_ControlLineState;
#endif

/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t USBFS_EP0_Buf[DEF_USBD_UEP0_SIZE];   
__attribute__ ((aligned(4))) uint8_t USBFS_EP2_Buf[DEF_USB_EP2_FS_SIZE]; 
#if V3F_ENABLE_USBFS_CDC_DEBUG
__attribute__ ((aligned(4))) uint8_t USBFS_EP3_Buf[DEF_USB_EP3_FS_SIZE];
__attribute__ ((aligned(4))) uint8_t USBFS_EP4_Buf[DEF_USB_EP4_FS_SIZE];
#endif

/* Ring buffer */
RING_BUFF_COMM  RingBuffer_Comm;
__attribute__ ((aligned(4))) uint8_t Data_Buffer[DEF_RING_BUFFER_SIZE];

/******************************************************************************/
/* Function declarations */
void USBFS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USBFS_RCC_Init
 *
 * @brief   Initializes the usbfs clock configuration.
 *
 * @return  none
 */
void USBFS_RCC_Init(void)
{
    if((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBHS)
    {
        /* Initialize USBHS 480M PLL */
        RCC_USBHS_PLLCmd(DISABLE);
        RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) ? RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
        RCC_USBHSPLLReferConfig(RCC_USBHSPLLRefer_25M);
        RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
        RCC_USBHS_PLLCmd(ENABLE);
        while (!(RCC->CTLR & RCC_USBHS_PLLRDY));
    }
    RCC_USBFSCLKConfig(RCC_USBFSCLKSource_USBHSPLL);
    RCC_USBFS48ClockSourceDivConfig(RCC_USBFS_Div10);
    RCC_HBPeriphClockCmd(RCC_HBPeriph_OTG_FS, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
}

/*********************************************************************
 * @fn      USBFS_Device_Endp_Init
 *
 * @brief   Initializes USB device endpoints.
 *
 * @return  none
 */
void USBFS_Device_Endp_Init( void )
{
#if V3F_ENABLE_USBFS_CDC_DEBUG
    USBFSD->UEP4_1_MOD = USBFS_UEP1_RX_EN | USBFS_UEP4_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_TX_EN | USBFS_UEP3_TX_EN;
#else
    USBFSD->UEP4_1_MOD = USBFS_UEP1_RX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_TX_EN;
#endif

    USBFSD->UEP0_DMA = (uint32_t)USBFS_EP0_Buf;
    USBFSD->UEP1_DMA = (uint32_t)Data_Buffer;
    USBFSD->UEP2_DMA = (uint32_t)USBFS_EP2_Buf;
#if V3F_ENABLE_USBFS_CDC_DEBUG
    USBFSD->UEP3_DMA = (uint32_t)USBFS_EP3_Buf;
    USBFSD->UEP4_DMA = (uint32_t)USBFS_EP4_Buf;
#endif

    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP1_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFSD->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK;
#if V3F_ENABLE_USBFS_CDC_DEBUG
    USBFSD->UEP3_TX_LEN = 0U;
    USBFSD->UEP4_TX_LEN = 0U;
    USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFSD->UEP4_TX_CTRL = USBFS_UEP_T_RES_NAK;
#endif
}

/*********************************************************************
 * @fn      USBFS_Device_Init
 *
 * @brief   Initializes USB device.
 *
 * @return  none
 */
void USBFS_Device_Init( FunctionalState sta )
{
    if( sta )
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFS_Device_Endp_Init( );
        USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
        USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
        NVIC_EnableIRQ( USBFS_IRQn );
    }
    else
    {
        USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
        Delay_Us( 10 );
        USBFSD->BASE_CTRL = 0x00;
        NVIC_DisableIRQ( USBFS_IRQn );
    }
}

#if V3F_ENABLE_USBFS_CDC_DEBUG
uint8_t USBFS_CDC_Debug_IsOpen(void)
{
    return (uint8_t)((USBFS_DevEnumStatus != 0U) ? 1U : 0U);
}

uint8_t USBFS_CDC_Debug_Send(const uint8_t *data, uint16_t len)
{
    uint16_t n;

    if((data == 0) || (len == 0U) || (USBFS_CDC_Debug_IsOpen() == 0U))
    {
        return 0U;
    }
    if((USBFSD->UEP3_TX_CTRL & USBFS_UEP_T_RES_MASK) != USBFS_UEP_T_RES_NAK)
    {
        return 0U;
    }

    n = (len > DEF_USB_EP3_FS_SIZE) ? DEF_USB_EP3_FS_SIZE : len;
    memcpy(USBFS_EP3_Buf, data, n);
    USBFSD->UEP3_DMA = (uint32_t)USBFS_EP3_Buf;
    USBFSD->UEP3_TX_LEN = n;
    USBFSD->UEP3_TX_CTRL =
        (USBFSD->UEP3_TX_CTRL & (uint8_t)(~USBFS_UEP_T_RES_MASK)) |
        USBFS_UEP_T_RES_ACK;
    return 1U;
}
#endif

/*********************************************************************
 * @fn      USBFS_IRQHandler
 *
 * @brief   This function handles HD-FS exception.
 *
 * @return  none
 */
void USBFS_IRQHandler( void )
{
    uint8_t  intflag, intst, errflag;
    uint16_t len;

    intflag = USBFSD->INT_FG;
    intst   = USBFSD->INT_ST;

    if( intflag & USBFS_UIF_TRANSFER )
    {
        switch( intst & USBFS_UIS_TOKEN_MASK )
        {
            /* data-in stage processing */
            case USBFS_UIS_TOKEN_IN:
                switch( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    /* end-point 0 data in interrupt */
                    case USBFS_UIS_TOKEN_IN | DEF_UEP0:
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                        }
						
                        if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                        {
                            /* Non-standard request endpoint 0 Data upload */
                        }
                        else
                        {
                            switch( USBFS_SetupReqCode )
                            {
                                case USB_GET_DESCRIPTOR:
                                    len = USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                                    memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                                    USBFS_SetupReqLen -= len;
                                    pUSBFS_Descr += len;
                                    USBFSD->UEP0_TX_LEN = len;
                                    USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                                    break;

                                case USB_SET_ADDRESS:
                                    USBFSD->DEV_ADDR = (USBFSD->DEV_ADDR & USBFS_UDA_GP_BIT) | USBFS_DevAddr;
                                    break;

                                default:
                                        break;
                            }
                        }
                        break;



                        /* end-point 2 data in interrupt */
                        case USBFS_UIS_TOKEN_IN | DEF_UEP2:
                            USBFSD->UEP2_TX_CTRL = (USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
							USBFSD->UEP2_TX_CTRL ^= USBFS_UEP_T_TOG;
                            break;

#if V3F_ENABLE_USBFS_CDC_DEBUG
                        case USBFS_UIS_TOKEN_IN | DEF_UEP3:
                            USBFSD->UEP3_TX_CTRL =
                                (USBFSD->UEP3_TX_CTRL & (uint8_t)(~USBFS_UEP_T_RES_MASK)) |
                                USBFS_UEP_T_RES_NAK;
                            USBFSD->UEP3_TX_CTRL ^= USBFS_UEP_T_TOG;
                            break;

                        case USBFS_UIS_TOKEN_IN | DEF_UEP4:
                            USBFSD->UEP4_TX_CTRL =
                                (USBFSD->UEP4_TX_CTRL & (uint8_t)(~USBFS_UEP_T_RES_MASK)) |
                                USBFS_UEP_T_RES_NAK;
                            USBFSD->UEP4_TX_CTRL ^= USBFS_UEP_T_TOG;
                            break;
#endif


                    default :
                        break;
                }
                break;

            /* data-out stage processing */
            case USBFS_UIS_TOKEN_OUT:
                switch( intst & ( USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK ) )
                {
                    /* end-point 0 data out interrupt */
                    case USBFS_UIS_TOKEN_OUT | DEF_UEP0:
                            if( intst & USBFS_UIS_TOG_OK )
                            {
                                if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                                {
#if V3F_ENABLE_USBFS_CDC_DEBUG
                                    if(USBFS_SetupReqCode == CDC_SET_LINE_CODING)
                                    {
                                        uint16_t copy_len = USBFS_SetupReqLen;
                                        if(copy_len > sizeof(USBFS_CDC_LineCoding))
                                        {
                                            copy_len = sizeof(USBFS_CDC_LineCoding);
                                        }
                                        memcpy(USBFS_CDC_LineCoding, USBFS_EP0_Buf, copy_len);
                                        USBFS_SetupReqLen = 0U;
                                        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
                                    }
                                    else
#endif
                                    if (( USBFS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_CLASS)
                                    {
                                        switch( USBFS_SetupReqCode )
                                        {
                                            case HID_SET_REPORT:
                                                memcpy(&HID_Report_Buffer[0],USBFS_EP0_Buf,DEF_USBD_UEP0_SIZE);
                                                HID_Set_Report_Flag = SET_REPORT_WAIT_DEAL;
                                                USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                }
                                else
                                {
                                    /* Standard request end-point 0 Data download */
                                    /* Add your code here */
                                }

                                if( USBFS_SetupReqLen == 0 )
                                {
                                    USBFSD->UEP0_TX_LEN  = 0;
                                    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                                }
                            }
                            break;

                    /* end-point 1 data out interrupt */
                    case USBFS_UIS_TOKEN_OUT | DEF_UEP1:
                        if ( intst & USBFS_UIS_TOG_OK )
                        {
                            /* Write In Buffer */
                            USBFSD->UEP1_RX_CTRL ^= USBFS_UEP_R_TOG;
                            RingBuffer_Comm.PackLen[RingBuffer_Comm.LoadPtr] = USBFSD->RX_LEN;
                            RingBuffer_Comm.LoadPtr ++;
                            if(RingBuffer_Comm.LoadPtr == DEF_Ring_Buffer_Max_Blks)
                            {
                                RingBuffer_Comm.LoadPtr = 0;
                            }
                            USBFSD->UEP1_DMA = (uint32_t)(&Data_Buffer[(RingBuffer_Comm.LoadPtr) * DEF_USBD_FS_PACK_SIZE]);
                            RingBuffer_Comm.RemainPack ++;
                            if(RingBuffer_Comm.RemainPack >= DEF_Ring_Buffer_Max_Blks-DEF_RING_BUFFER_REMINE)
                            {
                                USBFSD->UEP1_RX_CTRL = (USBFSD->UEP1_RX_CTRL & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK;
                                RingBuffer_Comm.StopFlag = 1;
                            }
                        }
                        break;
                    default:
                        break;

                }
                break;

            /* Setup stage processing */
            case USBFS_UIS_TOKEN_SETUP:
                USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_NAK;
                USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_NAK;
                /* Store All Setup Values */
                USBFS_SetupReqType  = pUSBFS_SetupReqPak->bRequestType;
                USBFS_SetupReqCode  = pUSBFS_SetupReqPak->bRequest;
                USBFS_SetupReqLen   = pUSBFS_SetupReqPak->wLength;
                USBFS_SetupReqValue = pUSBFS_SetupReqPak->wValue;
                USBFS_SetupReqIndex = pUSBFS_SetupReqPak->wIndex;
                len = 0;
                errflag = 0;
                if ( ( USBFS_SetupReqType & USB_REQ_TYP_MASK ) != USB_REQ_TYP_STANDARD )
                {
                    if (( USBFS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_CLASS)
                    {
                        switch( USBFS_SetupReqCode )
                        {
#if V3F_ENABLE_USBFS_CDC_DEBUG
                            case CDC_GET_LINE_CODING:
                                if(USBFS_SetupReqIndex == 0x00)
                                {
                                    memcpy(USBFS_EP0_Buf, USBFS_CDC_LineCoding, sizeof(USBFS_CDC_LineCoding));
                                    len = sizeof(USBFS_CDC_LineCoding);
                                    if(USBFS_SetupReqLen > len)
                                    {
                                        USBFS_SetupReqLen = len;
                                    }
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case CDC_SET_LINE_CODING:
                                if(USBFS_SetupReqIndex != 0x00)
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case CDC_SET_LINE_CTLSTE:
                                if(USBFS_SetupReqIndex == 0x00)
                                {
                                    USBFS_CDC_ControlLineState = USBFS_SetupReqValue;
                                    USBFS_SetupReqLen = 0U;
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case CDC_SEND_BREAK:
                                if(USBFS_SetupReqIndex == 0x00)
                                {
                                    USBFS_SetupReqLen = 0U;
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;
#endif

                            case HID_SET_REPORT:
                                break;

                            case HID_GET_REPORT:
                                if( USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE )
                                {
                                    len = DEF_USBD_UEP0_SIZE;
                                    memcpy(USBFS_EP0_Buf,&HID_Report_Buffer[0],DEF_USBD_UEP0_SIZE);
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case HID_SET_IDLE:
                                if( USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE )
                                {
                                    USBFS_HidIdle = USBFS_EP0_Buf[ 3 ];
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case HID_SET_PROTOCOL:
                                if( USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE )
                                {
                                    USBFS_HidProtocol = USBFS_EP0_Buf[ 2 ];
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;

                            case HID_GET_IDLE:
                                if( USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE )
                                {
                                    USBFS_EP0_Buf[ 0 ] = USBFS_HidIdle;
                                    len = 1;
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;
                            case HID_GET_PROTOCOL:
                                if( USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE )
                                {
                                    USBFS_EP0_Buf[ 0 ] = USBFS_HidProtocol;
                                    len = 1;
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                                break;
                            default:
                                errflag = 0xFF;
                                break;
                        }
                    }
                }
                else
                {
                    /* usb standard request processing */
                    switch( USBFS_SetupReqCode )
                    {
                        /* get device/configuration/string/report/... descriptors */
                        case USB_GET_DESCRIPTOR:
                            switch( (uint8_t)(USBFS_SetupReqValue>>8) )
                            {
                                /* get usb device descriptor */
                                case USB_DESCR_TYP_DEVICE:
                                    pUSBFS_Descr = MyDevDescr;
                                    len = DEF_USBD_DEVICE_DESC_LEN;
                                    break;

                                /* get usb configuration descriptor */
                                case USB_DESCR_TYP_CONFIG:
                                    pUSBFS_Descr = MyCfgDescr;
                                    len = DEF_USBD_CONFIG_DESC_LEN;
									break;
                              /* get usb report descriptor */
                              case USB_DESCR_TYP_REPORT:
                                    if (USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE)
                                    {
                                        pUSBFS_Descr = MyHIDReportDesc;
                                        len = DEF_USBD_REPORT_DESC_LEN;
                                    }
                                    else
                                    {
                                        errflag = 0xFF;
                                    }
                                    break;
                                /* get hid descriptor */
                                case USB_DESCR_TYP_HID:
                                    if (USBFS_SetupReqIndex == DEF_USBD_HID_INTERFACE)
                                    {
                                        pUSBFS_Descr = &MyCfgDescr[DEF_USBD_HID_DESC_OFFSET];
                                        len = 0x09;
                                    }
                                    else
                                    {
                                        errflag = 0xFF;
                                    }
                                    break;

                                /* get usb string descriptor */
                                case USB_DESCR_TYP_STRING:
                                    switch( (uint8_t)(USBFS_SetupReqValue&0xFF) )
                                    {
                                        /* Descriptor 0, Language descriptor */
                                        case DEF_STRING_DESC_LANG:
                                            pUSBFS_Descr = MyLangDescr;
                                            len = DEF_USBD_LANG_DESC_LEN;
                                            break;

                                        /* Descriptor 1, Manufacturers String descriptor */
                                        case DEF_STRING_DESC_MANU:
                                            pUSBFS_Descr = MyManuInfo;
                                            len = DEF_USBD_MANU_DESC_LEN;
                                            break;

                                        /* Descriptor 2, Product String descriptor */
                                        case DEF_STRING_DESC_PROD:
                                            pUSBFS_Descr = MyProdInfo;
                                            len = DEF_USBD_PROD_DESC_LEN;
                                            break;

                                        /* Descriptor 3, Serial-number String descriptor */
                                        case DEF_STRING_DESC_SERN:
                                            pUSBFS_Descr = MySerNumInfo;
                                            len = DEF_USBD_SN_DESC_LEN;
                                            break;

                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                    break;
  
                                default :
                                    errflag = 0xFF;
                                    break;
                            }

                            /* Copy Descriptors to Endp0 DMA buffer */
                            if( USBFS_SetupReqLen>len )
                            {
                                USBFS_SetupReqLen = len;
                            }
                            len = (USBFS_SetupReqLen >= DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                            memcpy( USBFS_EP0_Buf, pUSBFS_Descr, len );
                            pUSBFS_Descr += len;
                            break;

                        /* Set usb address */
                        case USB_SET_ADDRESS:
                            USBFS_DevAddr = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            break;

                        /* Get usb configuration now set */
                        case USB_GET_CONFIGURATION:
                            USBFS_EP0_Buf[ 0 ] = USBFS_DevConfig;
                            if( USBFS_SetupReqLen > 1 )
                            {
                                USBFS_SetupReqLen = 1;
                            }
                            break;

                        /* Set usb configuration to use */
                        case USB_SET_CONFIGURATION:
                            USBFS_DevConfig = (uint8_t)( USBFS_SetupReqValue & 0xFF );
                            USBFS_DevEnumStatus = 0x01;
                            break;

                        /* Clear or disable one usb feature */
                        case USB_CLEAR_FEATURE:
                            if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                /* clear one device feature */
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_REMOTE_WAKEUP )
                                {
                                    /* clear usb sleep status, device not prepare to sleep */
                                    USBFS_DevSleepStatus &= ~0x01;
                                }
                            }
                            else if ( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_ENDP_HALT )
                                {
                                    switch( (uint8_t)(USBFS_SetupReqIndex&0xFF) )
                                    {
                                        case ( DEF_UEP_OUT | DEF_UEP1 ):
                                            /* Set End-point 1 OUT ACK */
                                            USBFSD->UEP1_RX_CTRL =  USBFS_UEP_R_RES_ACK;
                                            break;

                                        case ( DEF_UEP_IN | DEF_UEP2 ):
                                            /* Set End-point 2 IN NAK */
                                            USBFSD->UEP2_TX_CTRL =  USBFS_UEP_T_RES_NAK;
                                            break;

#if V3F_ENABLE_USBFS_CDC_DEBUG
                                        case ( DEF_UEP_IN | DEF_UEP3 ):
                                            USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK;
                                            break;

                                        case ( DEF_UEP_IN | DEF_UEP4 ):
                                            USBFSD->UEP4_TX_CTRL = USBFS_UEP_T_RES_NAK;
                                            break;
#endif

                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                            }
                            else
                            {
                                errflag = 0xFF;
                            }
                            break;

                        /* set or enable one usb feature */
                        case USB_SET_FEATURE:
                            if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                /* Set Device Feature */
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_REMOTE_WAKEUP )
                                {
                                    if( MyCfgDescr[ 7 ] & 0x20 )
                                    {
                                        /* Set Wake-up flag, device prepare to sleep */
                                        USBFS_DevSleepStatus |= 0x01;
                                    }
                                    else
                                    {
                                        errflag = 0xFF;
                                    }
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                            }
                            else if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                /* Set End-point Feature */
                                if( (uint8_t)( USBFS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_ENDP_HALT )
                                {

                                    switch( (uint8_t)(USBFS_SetupReqIndex&0xFF) )
                                    {
                                        case ( DEF_UEP_OUT | DEF_UEP1 ):
                                            USBFSD->UEP1_RX_CTRL = ( USBFSD->UEP1_RX_CTRL & ~USBFS_UEP_R_RES_MASK ) | USBFS_UEP_R_RES_STALL;
                                            break;
	                                    case ( DEF_UEP_IN | DEF_UEP2 ):
                                            USBFSD->UEP2_TX_CTRL = ( USBFSD->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_STALL;
                                            break;

#if V3F_ENABLE_USBFS_CDC_DEBUG
	                                    case ( DEF_UEP_IN | DEF_UEP3 ):
                                            USBFSD->UEP3_TX_CTRL = ( USBFSD->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_STALL;
                                            break;

	                                    case ( DEF_UEP_IN | DEF_UEP4 ):
                                            USBFSD->UEP4_TX_CTRL = ( USBFSD->UEP4_TX_CTRL & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_STALL;
                                            break;
#endif

                                        default:
                                            errflag = 0xFF;
                                            break;
                                    }
                                }
                                else
                                {
                                    errflag = 0xFF;
                                }
                            }
                            else
                            {
                                errflag = 0xFF;
                            }
                            break;

                        /* This request allows the host to select another setting for the specified interface  */
                        case USB_GET_INTERFACE:
                            USBFS_EP0_Buf[0] = 0x00;
                            if ( USBFS_SetupReqLen > 1 )
                            {
                                USBFS_SetupReqLen = 1;
                            }
                            break;

                        case USB_SET_INTERFACE:
                            break;

                        /* host get status of specified device/interface/end-points */
                        case USB_GET_STATUS:
                            USBFS_EP0_Buf[ 0 ] = 0x00;
                            USBFS_EP0_Buf[ 1 ] = 0x00;

                            if ( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                            {
                                if( USBFS_DevSleepStatus & 0x01 )
                                {
                                    USBFS_EP0_Buf[ 0 ] = 0x02;
                                }
                            }
                            else if( ( USBFS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                            {
                                if((uint8_t)(USBFS_SetupReqIndex&0xFF) == ( DEF_UEP_OUT |DEF_UEP1 ))
                                {
                                    if( ( USBFSD->UEP1_RX_CTRL & USBFS_UEP_R_RES_MASK ) == USBFS_UEP_R_RES_STALL )
                                    {
                                        USBFS_EP0_Buf[ 0 ] = 0x01;
                                    }
                                }
                                else if((uint8_t)(USBFS_SetupReqIndex&0xFF) == ( DEF_UEP_IN | DEF_UEP2 ))
                                {
                                    if( ( USBFSD->UEP2_TX_CTRL & USBFS_UEP_T_RES_MASK ) == USBFS_UEP_T_RES_STALL )
                                    {
                                        USBFS_EP0_Buf[ 0 ] = 0x01;
                                    }
                                }
#if V3F_ENABLE_USBFS_CDC_DEBUG
                                else if((uint8_t)(USBFS_SetupReqIndex&0xFF) == ( DEF_UEP_IN | DEF_UEP3 ))
                                {
                                    if( ( USBFSD->UEP3_TX_CTRL & USBFS_UEP_T_RES_MASK ) == USBFS_UEP_T_RES_STALL )
                                    {
                                        USBFS_EP0_Buf[ 0 ] = 0x01;
                                    }
                                }
                                else if((uint8_t)(USBFS_SetupReqIndex&0xFF) == ( DEF_UEP_IN | DEF_UEP4 ))
                                {
                                    if( ( USBFSD->UEP4_TX_CTRL & USBFS_UEP_T_RES_MASK ) == USBFS_UEP_T_RES_STALL )
                                    {
                                        USBFS_EP0_Buf[ 0 ] = 0x01;
                                    }
                                }
#endif
                                else
                                {
                                    errflag = 0xFF;
                                }
                            }
                            else
                            {
                                errflag = 0xFF;
                            }

                            if ( USBFS_SetupReqLen > 2 )
                            {
                                USBFS_SetupReqLen = 2;
                            }

                            break;

                        default:
                            errflag = 0xFF;
                            break;
                    }
                }

                /* errflag = 0xFF means a request not support or some errors occurred, else correct */
                if( errflag == 0xFF)
                {
                    /* if one request not support, return stall */
                    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_STALL;
                    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_STALL;
                }
                else
                {
                    /* end-point 0 data Tx/Rx */
                    if( USBFS_SetupReqType & DEF_UEP_IN )
                    {
                        len = ( USBFS_SetupReqLen > DEF_USBD_UEP0_SIZE )? DEF_USBD_UEP0_SIZE : USBFS_SetupReqLen;
                        USBFS_SetupReqLen -= len;
                        USBFSD->UEP0_TX_LEN  = len;
                        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                    }
                    else
                    {
                        if( USBFS_SetupReqLen == 0 )
                        {
                            USBFSD->UEP0_TX_LEN  = 0;
                            USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG|USBFS_UEP_T_RES_ACK;
                        }
                        else
                        {
                            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG|USBFS_UEP_R_RES_ACK;
                        }
                    }
                }
                break;

            default :
                break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }
    else if( intflag & USBFS_UIF_BUS_RST )
    {
        /* usb reset interrupt processing */
        USBFS_DevConfig = 0;
        USBFS_DevAddr = 0;
        USBFS_DevSleepStatus = 0;
        USBFS_DevEnumStatus = 0;
#if V3F_ENABLE_USBFS_CDC_DEBUG
        USBFS_CDC_ControlLineState = 0U;
#endif
        USBFSD->DEV_ADDR = 0;
        USBFS_Device_Endp_Init( );
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    }
    else if( intflag & USBFS_UIF_SUSPEND )
    {
        /* usb suspend interrupt processing */
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
        Delay_Us(10);
        if ( USBFSD->MIS_ST & USBFS_UMS_SUSPEND )
        {
            USBFS_DevSleepStatus |= 0x02;
            if( USBFS_DevSleepStatus == 0x03 )
            {
                /* Handling usb sleep here */
            }
        }
        else
        {
            USBFS_DevSleepStatus &= ~0x02;
        }

    }
    else
    {
        /* other interrupts */
        USBFSD->INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      USBFS_Send_Resume
 *
 * @brief   USBFS device sends wake-up signal to host
 *
 * @return  none
 */
void USBFS_Send_Resume( void )
{
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    Delay_Ms( 8 );
    USBFSD->UDEV_CTRL ^= USBFS_UD_LOW_SPEED;
    Delay_Ms( 1 );
}
