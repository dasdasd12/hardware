/*
 * Compile the proven product USBHS implementation alongside the WCH USBSS
 * example.  Both descriptor sources use the same generic symbol names, so
 * keep the product descriptor ABI local to this translation unit by renaming
 * those symbols before including the unmodified product sources.
 */
#define MyDevDescr H417_Product_USBHS_MyDevDescr
#define MyCfgDescr_HS H417_Product_USBHS_MyCfgDescr_HS
#define MyCfgDescr_FS H417_Product_USBHS_MyCfgDescr_FS
#define MyHIDReportDesc_HS H417_Product_USBHS_MyHIDReportDesc_HS
#define MyHIDReportDesc_FS H417_Product_USBHS_MyHIDReportDesc_FS
#define MyLangDescr H417_Product_USBHS_MyLangDescr
#define MyManuInfo H417_Product_USBHS_MyManuInfo
#define MyProdInfo H417_Product_USBHS_MyProdInfo
#define MySerNumInfo H417_Product_USBHS_MySerNumInfo
#define MyQuaDesc H417_Product_USBHS_MyQuaDesc
#define MyBOSDesc H417_Product_USBHS_MyBOSDesc
#define TAB_USB_FS_OSC_DESC H417_Product_USBHS_TAB_USB_FS_OSC_DESC
#define TAB_USB_HS_OSC_DESC H417_Product_USBHS_TAB_USB_HS_OSC_DESC

#include "usb_desc.c"
#include "ch32h417_usbhs_device.c"

#undef MyDevDescr
#undef MyCfgDescr_HS
#undef MyCfgDescr_FS
#undef MyHIDReportDesc_HS
#undef MyHIDReportDesc_FS
#undef MyLangDescr
#undef MyManuInfo
#undef MyProdInfo
#undef MySerNumInfo
#undef MyQuaDesc
#undef MyBOSDesc
#undef TAB_USB_FS_OSC_DESC
#undef TAB_USB_HS_OSC_DESC
