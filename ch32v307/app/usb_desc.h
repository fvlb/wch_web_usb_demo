/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.h
 * Description        : USB device/configuration/string/BOS, Microsoft OS 2.0
 *                       and WebUSB Platform Capability descriptors for the
 *                       WinUSB / WebUSB demo (CH592F port of the CH32L103
 *                       firmware in ../../ch32l103).
 *******************************************************************************/
#ifndef APP_USB_DESC_H_
#define APP_USB_DESC_H_

#include "ch32v30x.h"

/******************************************************************************/
/* USB device VID/PID.
 * 0x1209 / 0x0001 is the generic "test" PID from the pid.codes open allocation
 * scheme. Replace with your own registered VID/PID for production use.        */
#define DEF_USB_VID                  0x1209
#define DEF_USB_PID                  0x0001

/******************************************************************************/
#define DEF_USBD_UEP0_SIZE           64      /* endpoint 0 max packet size */

/* Endpoint 1 IN (interrupt): button-press notifications, see button.c.
 * Payload is a single little-endian uint32_t = milliseconds since boot.    */
#define DEF_USBD_UEP1_SIZE           8       /* endpoint 1 max packet size */
#define DEF_UPTIME_EVENT_LEN         4       /* payload bytes actually sent */

/* EP2 DMA capacity. The FS descriptor uses its separate 64-byte MPS. */
#define DEF_USBD_UEP2_SIZE           512u

/******************************************************************************/
/* Vendor-specific control requests (bmRequestType type = Vendor) handled by
 * this device, in addition to the MS OS 2.0 vendor request below.            */
#define VENDOR_REQUEST_SET_LED        0x02   /* host->device, wValue = brightness 0..255 */
#define VENDOR_REQUEST_GET_LED        0x03   /* device->host, returns 1 byte brightness  */
#define VENDOR_REQUEST_GET_VERSION    0x05   /* device->host, returns 3 bytes: major,minor,patch */
#define VENDOR_REQUEST_SET_DEBOUNCE_MS 0x06  /* host->device, wValue = debounce interval in ms, no data stage */
#define VENDOR_REQUEST_GET_BUTTON_RAW 0x07   /* device->host, returns 1 byte: 1=released,0=pressed (diagnostic) */
#define VENDOR_REQUEST_GET_MILLIS 0x08        /* device->host, returns 4 bytes little-endian uint32 = ms since boot (diagnostic) */
#define VENDOR_REQUEST_GET_FILE_SIZE 0x09     /* device->host, returns 4 bytes little-endian uint32 = synthetic file size in bytes */
#define VENDOR_REQUEST_START_FILE_XFER 0x0A   /* host->device, no data: resets the EP2 bulk read offset to 0 and (re)arms EP2 */

/* Vendor code used for the Microsoft OS 2.0 Descriptor request.
 * Must not collide with any standard/class request code, and is reported to
 * the host through the BOS Platform Capability descriptor.                   */
#define MS_OS_20_VENDOR_CODE          0x01

/* wIndex value the host must send together with MS_OS_20_VENDOR_CODE to
 * request the "Microsoft OS 2.0 Descriptor Set" (fixed by the MS OS 2.0 spec) */
#define MS_OS_20_DESCRIPTOR_INDEX     0x07

/* Vendor code used for the WebUSB GET_URL request (WebUSB spec, §5 "Requests").
 * Independent of MS_OS_20_VENDOR_CODE - a device can freely choose distinct
 * (or even identical) vendor codes per Platform Capability; the two requests
 * are told apart by wIndex, which is fixed to 0x0002 for WebUSB GET_URL.     */
#define WEBUSB_VENDOR_CODE            0x04
#define WEBUSB_GET_URL_INDEX          0x02
#define WEBUSB_LANDING_PAGE_INDEX     0x01   /* iLandingPage in the URL descriptor table */

/* Not defined by the CH592 StdPeriphDriver headers. */
#ifndef USB_DESCR_TYP_BOS
#define USB_DESCR_TYP_BOS             0x0F
#endif

/******************************************************************************/
/* external variables (raw descriptor tables), defined in usb_desc.c          */
extern const uint8_t MyDevDescr[];
extern const uint8_t MyCfgDescr_FS[];
extern const uint8_t MyCfgDescr_HS[];
extern const uint8_t MyQuaDescr[];
#define DEF_USBD_CONFIG_FS_DESC_LEN DEF_USBD_CONFIG_DESC_LEN
#define DEF_USBD_CONFIG_HS_DESC_LEN DEF_USBD_CONFIG_DESC_LEN
extern const uint8_t MyLangDescr[];
extern const uint8_t MyManuInfo[];
extern const uint8_t MyProdInfo[];
extern const uint8_t MySerNumInfo[];
extern const uint8_t MyBOSDescr[];
extern const uint8_t MyMSOS20Descr[];
extern const uint8_t MyWebUSBURLDescr[];

/* Descriptor length helpers */
#define DEF_USBD_DEVICE_DESC_LEN     ((uint8_t)MyDevDescr[0])
#define DEF_USBD_CONFIG_DESC_LEN     ((uint16_t)MyCfgDescr_FS[2] + (uint16_t)(MyCfgDescr_FS[3] << 8))
#define DEF_USBD_HS_PACK_SIZE        512u
#define DEF_USBD_FS_PACK_SIZE        64u
#define DEF_USBD_LANG_DESC_LEN       ((uint16_t)MyLangDescr[0])
#define DEF_USBD_MANU_DESC_LEN       ((uint16_t)MyManuInfo[0])
#define DEF_USBD_PROD_DESC_LEN       ((uint16_t)MyProdInfo[0])
#define DEF_USBD_SN_DESC_LEN         ((uint16_t)MySerNumInfo[0])
#define DEF_USBD_BOS_DESC_LEN        ((uint16_t)MyBOSDescr[2] + (uint16_t)(MyBOSDescr[3] << 8))
#define DEF_USBD_MSOS20_DESC_LEN     30u
#define DEF_USBD_WEBUSB_URL_DESC_LEN ((uint8_t)MyWebUSBURLDescr[0])

#endif /* APP_USB_DESC_H_ */
