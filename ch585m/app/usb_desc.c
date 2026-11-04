/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.c
 * Description        : USB device/configuration/string/BOS, Microsoft OS 2.0
 *                       and WebUSB Platform Capability descriptors for the
 *                       WinUSB / WebUSB demo. CH592F port of the descriptors
 *                       in ../../ch32l103/User/usb_desc.c.
 *
 *  Design notes:
 *  -------------
 *  - The device exposes ONE vendor-specific interface (class/sub/proto =
 *    0xFF/0xFF/0xFF) with two IN endpoints: EP1 interrupt (button-press
 *    events) and EP2 bulk (synthetic file download). LED control happens
 *    entirely over EP0 control transfers using vendor requests.
 *
 *  - bcdUSB is set to 0x0210 (USB 2.1) so the host knows to look for a
 *    BOS descriptor (BOS support needs bcdUSB >= 2.01).
 *
 *  - The BOS descriptor is a *container* and can carry any number of
 *    "Platform Capability" sub-descriptors side by side - a host/browser
 *    that doesn't recognize a given capability UUID simply skips it. This
 *    firmware advertises TWO capabilities in the same BOS:
 *
 *      1) Microsoft OS 2.0 (UUID D8DD60DF-4589-4CC7-9CD2-659D9E648A9F):
 *         tells Windows "send me a vendor request with code
 *         MS_OS_20_VENDOR_CODE and wIndex=0x0007 to fetch my MS OS 2.0
 *         descriptor set". That descriptor set (see usbd_winusb.c)
 *         declares the device as compatible with WINUSB.SYS, so Windows
 *         binds it automatically with no .inf file.
 *
 *      2) WebUSB (UUID 3408B638-09A9-47A0-8BFD-A0768815B665, see
 *         https://wicg.github.io/webusb/#webusb-platform-capability-descriptor):
 *         tells WebUSB-capable browsers "send me a vendor request with
 *         code WEBUSB_VENDOR_CODE and wIndex=0x0002 to fetch my landing
 *         page URL".
 *******************************************************************************/

#include "usb_desc.h"
#include "version.h"

/******************************************************************************/
/* Device Descriptor                                                          */
const uint8_t MyDevDescr[] =
{
    0x12,                                               // bLength
    0x01,                                               // bDescriptorType (Device)
    0x10, 0x02,                                         // bcdUSB 2.10 (needed for BOS)
    0x00,                                               // bDeviceClass (per-interface)
    0x00,                                               // bDeviceSubClass
    0x00,                                               // bDeviceProtocol
    DEF_USBD_UEP0_SIZE,                                 // bMaxPacketSize0
    (uint8_t)DEF_USB_VID, (uint8_t)(DEF_USB_VID >> 8),  // idVendor
    (uint8_t)DEF_USB_PID, (uint8_t)(DEF_USB_PID >> 8),  // idProduct
    FW_BCD_DEVICE_LO, FW_BCD_DEVICE_HI,                 // bcdDevice = firmware version (see version.h)
    0x01,                                               // iManufacturer
    0x02,                                               // iProduct
    0x03,                                               // iSerialNumber
    0x01,                                               // bNumConfigurations
};

/******************************************************************************/
/* Configuration Descriptor: Configuration + single vendor Interface with   */
/* two endpoints: EP1 IN (interrupt, button-press events) and EP2 IN (bulk, */
/* synthetic file download for throughput testing). LED control still goes  */
/* entirely over EP0 control transfers.                                     */
const uint8_t MyCfgDescr_FS [] =
{
    /* Configuration Descriptor */
    0x09,                           // bLength
    0x02,                           // bDescriptorType (Configuration)
    0x20, 0x00,                     // wTotalLength = 9 + 9 + 7 + 7 = 32
    0x01,                           // bNumInterfaces
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0x80,                           // bmAttributes (bus powered)
    0x32,                           // bMaxPower = 100 mA

    /* Interface Descriptor - vendor specific, two IN endpoints */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x00,                           // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x02,                           // bNumEndpoints
    0xFF,                           // bInterfaceClass (vendor specific)
    0xFF,                           // bInterfaceSubClass
    0xFF,                           // bInterfaceProtocol
    0x00,                           // iInterface

    /* Endpoint Descriptor - EP1 IN, interrupt, button-press events */
    0x07,                           // bLength
    0x05,                           // bDescriptorType (Endpoint)
    0x81,                           // bEndpointAddress: IN endpoint 1
    0x03,                           // bmAttributes: Interrupt
    DEF_USBD_UEP1_SIZE, 0x00,       // wMaxPacketSize
    0x0A,                           // bInterval: 10ms polling

    /* Endpoint Descriptor - EP2 IN, bulk, synthetic file download */
    0x07,                           // bLength
    0x05,                           // bDescriptorType (Endpoint)
    0x82,                           // bEndpointAddress: IN endpoint 2
    0x02,                           // bmAttributes: Bulk
    0x40, 0x00,                    // FS wMaxPacketSize = 64
    0x00,                           // bInterval: N/A for bulk
};

/* High-speed configuration: identical layout, but the EP2 bulk endpoint
 * uses the 512-byte high-speed maximum packet size. */
const uint8_t MyCfgDescr_HS[] ={
    /* Configuration Descriptor */
    0x09,                           // bLength
    0x02,                           // bDescriptorType (Configuration)
    0x20, 0x00,                     // wTotalLength = 9 + 9 + 7 + 7 = 32
    0x01,                           // bNumInterfaces
    0x01,                           // bConfigurationValue
    0x00,                           // iConfiguration
    0x80,                           // bmAttributes (bus powered)
    0x32,                           // bMaxPower = 100 mA

    /* Interface Descriptor - vendor specific, two IN endpoints */
    0x09,                           // bLength
    0x04,                           // bDescriptorType (Interface)
    0x00,                           // bInterfaceNumber
    0x00,                           // bAlternateSetting
    0x02,                           // bNumEndpoints
    0xFF,                           // bInterfaceClass (vendor specific)
    0xFF,                           // bInterfaceSubClass
    0xFF,                           // bInterfaceProtocol
    0x00,                           // iInterface

    /* Endpoint Descriptor - EP1 IN, interrupt, button-press events */
    0x07,                           // bLength
    0x05,                           // bDescriptorType (Endpoint)
    0x81,                           // bEndpointAddress: IN endpoint 1
    0x03,                           // bmAttributes: Interrupt
    DEF_USBD_UEP1_SIZE, 0x00,       // wMaxPacketSize
    0x0A,                           // bInterval: 10ms polling

    /* Endpoint Descriptor - EP2 IN, bulk, synthetic file download */
    0x07,                           // bLength
    0x05,                           // bDescriptorType (Endpoint)
    0x82,                           // bEndpointAddress: IN endpoint 2
    0x02,                           // bmAttributes: Bulk
    0x00, 0x02,       // wMaxPacketSize
    0x00,                           // bInterval: N/A for bulk
};

/* Device qualifier (required for high-speed operation). */
const uint8_t MyQuaDescr[] =
{
    0x0A,                             // bLength
    0x06,                             // bDescriptorType = DEVICE_QUALIFIER
    0x10, 0x02,                       // bcdUSB = 0x0210 (matches MyDevDescr)
    0x00,                             // bDeviceClass
    0x00,                             // bDeviceSubClass
    0x00,                             // bDeviceProtocol
    DEF_USBD_UEP0_SIZE,               // bMaxPacketSize0
    0x01,                             // bNumConfigurations
    0x00                              // bReserved
};


/******************************************************************************/
/* Language Descriptor (English - US) */
const uint8_t MyLangDescr[] =
{
    0x04, 0x03, 0x09, 0x04
};

/* Manufacturer String: "CH592F" */
const uint8_t MyManuInfo[] =
{
    0x0E,                            // bLength = 2 + 2*6 (UTF-16LE)
    0x03,                            // bDescriptorType = STRING
    'C', 0x00, 'H', 0x00, '5', 0x00, '8', 0x00, '5', 0x00, 'M', 0x00
};

/* Product String: "WinUSB WebUSB LED Demo" */
const uint8_t MyProdInfo[] =
{
    0x2E, 0x03,
    'W', 0, 'i', 0, 'n', 0, 'U', 0, 'S', 0, 'B', 0, ' ', 0,
    'W', 0, 'e', 0, 'b', 0, 'U', 0, 'S', 0, 'B', 0, ' ', 0,
    'L', 0, 'E', 0, 'D', 0, ' ', 0,
    'D', 0, 'e', 0, 'm', 0, 'o', 0
};

/* Serial Number String: "CH585M-DEMO-0001" */
const uint8_t MySerNumInfo[] =
{
    0x22,                            // bLength = 2 + 2*16 (UTF-16LE)
    0x03,                            // bDescriptorType = STRING
    'C', 0x00, 'H', 0x00, '5', 0x00, '8', 0x00, '5', 0x00, 'M', 0x00, '-', 0x00, 'D', 0x00, 'E', 0x00, 'M', 0x00, 'O', 0x00, '-', 0x00, '0', 0x00, '0', 0x00, '0', 0x00, '1', 0x00
};

/******************************************************************************/
/* BOS Descriptor - a container advertising TWO Platform Capabilities side   */
/* by side: Microsoft OS 2.0 and WebUSB. Each is identified by its own UUID  */
/* (little-endian byte order); a host that doesn't recognize a given UUID   */
/* simply ignores that Device Capability sub-descriptor.                    */
const uint8_t MyBOSDescr[] =
{
    /* BOS header */
    0x05,                           // bLength
    0x0F,                           // bDescriptorType (BOS)
    0x39, 0x00,                     // wTotalLength = 5 + 28 + 24 = 57
    0x02,                           // bNumDeviceCaps

    /* Device Capability #1 - Platform (Microsoft OS 2.0) */
    0x1C,                           // bLength = 28
    0x10,                           // bDescriptorType (DEVICE CAPABILITY)
    0x05,                           // bDevCapabilityType (PLATFORM)
    0x00,                           // bReserved
    /* MS OS 2.0 Platform Capability UUID {D8DD60DF-4589-4CC7-9CD2-659D9E648A9F} */
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
    /* Descriptor-specific data */
    0x00, 0x00, 0x03, 0x06,         // dwWindowsVersion = 0x06030000 (>= Win8.1)
    DEF_USBD_MSOS20_DESC_LEN & 0xFF, (DEF_USBD_MSOS20_DESC_LEN >> 8) & 0xFF, // wMSOSDescriptorSetTotalLength
    MS_OS_20_VENDOR_CODE,           // bMS_VendorCode
    0x00,                           // bAltEnumCode

    /* Device Capability #2 - Platform (WebUSB) */
    0x18,                           // bLength = 24
    0x10,                           // bDescriptorType (DEVICE CAPABILITY)
    0x05,                           // bDevCapabilityType (PLATFORM)
    0x00,                           // bReserved
    /* WebUSB Platform Capability UUID {3408B638-09A9-47A0-8BFD-A0768815B665} */
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,
    /* Descriptor-specific data */
    0x00, 0x01,                     // bcdVersion = 1.00
    WEBUSB_VENDOR_CODE,              // bVendorCode
    WEBUSB_LANDING_PAGE_INDEX,       // iLandingPage (index into the WebUSB URL descriptor table)
};

/******************************************************************************/
/* Microsoft OS 2.0 Descriptor Set, returned in response to the vendor       */
/* request { bRequest = MS_OS_20_VENDOR_CODE, wIndex = 0x0007 }.             */
/* Declares the device's function as compatible with WINUSB.SYS.            */
const uint8_t MyMSOS20Descr[] =
{
    /* Microsoft OS 2.0 descriptor set header (Table 10) */
    0x0A, 0x00,                     // wLength = 10
    0x00, 0x00,                     // wDescriptorType = MS_OS_20_SET_HEADER_DESCRIPTOR
    0x00, 0x00, 0x03, 0x06,         // dwWindowsVersion = 0x06030000
    DEF_USBD_MSOS20_DESC_LEN & 0xFF, (DEF_USBD_MSOS20_DESC_LEN >> 8) & 0xFF, // wTotalLength = 30

    /* Microsoft OS 2.0 compatible ID descriptor (Table 13) */
    0x14, 0x00,                     // wLength = 20
    0x03, 0x00,                     // wDescriptorType = MS_OS_20_FEATURE_COMPATIBLE_ID
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,        // compatibleID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // subCompatibleID
};

/******************************************************************************/
/* WebUSB URL Descriptor (WebUSB spec §4.3.1), returned in response to the   */
/* GET_URL vendor request { bRequest = WEBUSB_VENDOR_CODE, wIndex = 0x0002 }.*/
/* Points the browser at the dedicated LED/button control page for this     */
/* firmware, https://fiskov.github.io/webusb-led/index.html.         */
const uint8_t MyWebUSBURLDescr[] =
{
    0x29,                            // bLength = 3 + 38
    0x03,                            // bDescriptorType = WEBUSB_URL_DESCRIPTOR_TYPE
    0x01,                            // bScheme = 1 (https://)
    /* UTF-8 URL, without the scheme prefix, 38 bytes: */
    'f','i','s','k','o','v','.','g','i','t','h','u','b','.','i','o','/',
    'w','e','b','u','s','b','-','l','e','d',
    '/','i','n','d','e','x','.','h','t','m','l'
};
