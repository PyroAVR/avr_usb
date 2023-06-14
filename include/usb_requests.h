#pragma once

#include <stdint.h>

typedef struct {
    uint8_t   bmRequestType;
    uint8_t   bRequest;
} usb_req_hdr_t;

typedef struct {
    uint8_t   bmRequestType;
    uint8_t   bRequest;
    uint16_t  wValue;
    uint16_t  wIndex;
    uint16_t  wLength;
} usb_req_std_t;

typedef struct {
    uint8_t   bmRequestType;
    uint8_t   bRequest;
    uint8_t   wValueLSB;
    uint8_t   wValueMSB;
    uint8_t   wIndexLSB;
    uint8_t   wIndexMSB;
    uint8_t   wLengthLSB;
    uint8_t   wLengthMSB;
} usb_req_val_t;

typedef struct {
    uint8_t   bmRequestType;
    uint8_t   bRequest;
    uint8_t   index;
    uint8_t   type;
    uint16_t  language_id;
    uint16_t  length;
} usb_req_get_desc_t;

typedef enum {
    USB_REQ_GET_STATUS = 0,
    USB_REQ_CLEAR_FEATURE = 1,
    USB_REQ_SET_FEATURE = 3,
    USB_REQ_SET_ADDRESS = 5,
    USB_REQ_GET_DESCRIPTOR = 6,
    USB_REQ_SET_DESCRIPTOR = 7,
    USB_REQ_GET_CONFIGURATION = 8,
    USB_REQ_SET_CONFIGURATION = 9,
    USB_REQ_GET_INTERFACE = 10,
    USB_REQ_SET_INTERFACE = 11,
    USB_REQ_SYNCH_FRAME = 12,
} usb_b_req_t;
