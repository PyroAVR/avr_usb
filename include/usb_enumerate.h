#pragma once

#include <stdint.h>

typedef struct usb_device_desc_t {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint16_t  bcdUSB;
    uint8_t   bDeviceClass;
    uint8_t   bDeviceSubClass;
    uint8_t   bDeviceProtocol;
    uint8_t   bMaxPacketSize;
    uint16_t  idVendor;
    uint16_t  idProduct;
    uint16_t  bcdDevice;
    uint8_t   iManufacturer;
    uint8_t   iProduct;
    uint8_t   iSerialNumber;
    uint8_t   bNumConfigurations;
} usb_device_desc_t;

typedef struct usb_config_desc_t {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint16_t  wTotalLength;
    uint8_t   bNumInterfaces;
    uint8_t   bConfigurationValue;
    uint8_t   iConfiguration;
    uint8_t   bmAttributes;
    uint8_t   bMaxPower;
} usb_config_desc_t;

typedef struct usb_string_desc_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t* bString;
} usb_string_desc_t;

typedef struct usb_if_desc_t {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint8_t   bInterfaceNumber;
    uint8_t   bAlternateSetting;
    uint8_t   bNumEndpoints;
    uint8_t   bInterfaceClass;
    uint8_t   bInterfaceSubClass;
    uint8_t   bInterfaceProtocol;
    uint8_t   iInterface;
} usb_interface_desc_t;

typedef struct usb_ep_desc_t {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint8_t   bEndpointAddress;
    uint8_t   bmAttributes;
    uint16_t  wMaxPacketSize;
    uint8_t   bInterval;
} usb_endpoint_desc_t;

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

typedef enum {
    USB_DESC_DEVICE = 1,
    USB_DESC_CONFIGURATION = 2,
    USB_DESC_STRING = 3,
    USB_DESC_INTERFACE = 4,
    USB_DESC_ENDPOINT = 5,
    USB_DESC_DEVICE_QUAL = 6,
    USB_DESC_OTHER_SPEED_CONF = 7,
    USB_DESC_INTERFACE_POWER = 8,
} usb_desc_req_type_t;
