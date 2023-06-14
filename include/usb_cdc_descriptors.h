#pragma once

#include <stdint.h>

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t data[];
} usb_cdc_func_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdCDC;
} usb_cdc_header_func_desc_t;

// FIXME need a way to parameterize this for inclusion in a config desc
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterfaces[];
} usb_cdc_union_func_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t iCountryCodeRelDate; // version of country code spec
    uint16_t wCountryCodes[];
} usb_cdc_country_select_func_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmCapabilities;
} usb_cdc_acm_func_desc_t;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} usb_cdc_call_mgmt_desc_t;

typedef enum {
    USB_CDC_DESC_CS_INTERFACE = 0x24,
    USB_CDC_DESC_CS_ENDPOINT = 0x25,
} usb_cdc_desc_type_t;

typedef enum {
    USB_CDC_DESC_HEADER_FUNCTIONAL = 0,
    USB_CDC_DESC_CALL_MGMT = 1,
    USB_CDC_DESC_ACM = 2,
    USB_CDC_DESC_DLM = 3,
    USB_CDC_DESC_UNION = 6,
    USB_CDC_DESC_COUNTRY_SEL = 7,
    // incomplete, see USBCDC1.2 spec doc table 13
} usb_cdc_desc_subtype_t;

typedef enum {
    USB_CDC_ACM_CAP_COMM_FEATURE = 1,
    USB_CDC_ACM_CAP_LINE_STATE = (1 << 1),
    USB_CDC_ACM_CAP_SEND_BREAK = (1 << 2),
    USB_CDC_ACM_CAP_NETWORK_CONN = (1 << 3)
} usb_cdc_acm_cap_t;

typedef enum {
    USB_CDC_CALL_MGMT_CAP_DC = 1, // data class (1) or comm class (0)
    USB_CDC_CALL_MGMT_CAP_CM = (1 << 1), // handles call mgmt
} usb_cdc_call_mgmt_cap_t;
