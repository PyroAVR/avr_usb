#pragma once

#include <stdint.h>

typedef struct {
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

typedef struct {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint16_t  wTotalLength;
    uint8_t   bNumInterfaces;
    uint8_t   bConfigurationValue;
    uint8_t   iConfiguration;
    uint8_t   bmAttributes;
    uint8_t   bMaxPower;
} usb_config_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bString[];
} usb_string_desc_t;

typedef struct {
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

typedef struct {
    uint8_t   bLength;
    uint8_t   bDescriptorType;
    uint8_t   bEndpointAddress;
    uint8_t   bmAttributes;
    uint16_t  wMaxPacketSize;
    uint8_t   bInterval;
} usb_endpoint_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptortype;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} usb_assoc_desc_t;

typedef enum {
    USB_DESC_DEVICE = 1,
    USB_DESC_CONFIGURATION = 2,
    USB_DESC_STRING = 3,
    USB_DESC_INTERFACE = 4,
    USB_DESC_ENDPOINT = 5,
    USB_DESC_DEVICE_QUAL = 6,
    USB_DESC_OTHER_SPEED_CONF = 7,
    USB_DESC_INTERFACE_POWER = 8,
    USB_DESC_OTG = 9,
    USB_DESC_DEBUG = 10,
    USB_DESC_INTERFACE_ASSOC = 11,
} usb_desc_type_t;

// Table 9-13 in USB2.0
typedef enum {
    USB_EP_ATTRS_CTRL = 0,
    USB_EP_ATTRS_ISO = 1,
    USB_EP_ATTRS_BULK = 2,
    USB_EP_ATTRS_INT = 3,

    USB_EP_ATTRS_NO_SYNC = 0,
    USB_EP_ATTRS_ASYNC = (1 << 2),
    USB_EP_ATTRS_ADAPTIVE = (2 << 2),
    USB_EP_ATTRS_SYNC = (3 << 2),

    USB_EP_ATTRS_DATA = 0,
    USB_EP_ATTRS_FEEDBACK = (1 << 4),
    USB_EP_ATTRS_IMPL_FEEDBACK = (2 << 4),
    USB_EP_ATTRS_RESERVED = (3 << 4),
} usb_ep_attrs_t;

typedef enum {
    USB_EP_DIR_OUT = 0,
    USB_EP_DIR_IN = (1 << 7)
} usb_ep_dir_t;
