#ifndef V307_FILEXFER_MOCK_H
#define V307_FILEXFER_MOCK_H
#include <stdint.h>
#define __CH32V30X_USBHS_DEVICE_H__
#define __CH585_USBHS_DEVICE_H__
#define __HIGH_CODE
typedef uint16_t (*USBD_EP2_FillCallback)(const uint8_t **data, uint16_t maxlen);
void USBD_EP2_SetFillCallback(USBD_EP2_FillCallback callback);
void USBD_EP2_StartTransfer(void);
void USBD_EP2_StopTransfer(void);
#endif
