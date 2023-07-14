#pragma once

#include "usb_base_descriptors.h"
#include "usb_cdc_descriptors.h"

extern usb_device_desc_t self_device_desc;

extern usb_string_desc_t self_manuf_desc;
extern usb_string_desc_t self_product_desc;
extern usb_string_desc_t self_sernum_desc;

extern usb_string_desc_t *str_desc_tab[];

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface;
} custom_cdc_union_t;

typedef struct {
    usb_config_desc_t self_config_desc;
    usb_assoc_desc_t iad;
    // begin CDC Communication interface
    usb_interface_desc_t if_comm;
    // begin ACM Functional Descriptor
    usb_cdc_header_func_desc_t cdc_header;
    usb_cdc_call_mgmt_desc_t cdc_call_mgmt;
    usb_cdc_acm_func_desc_t acm_desc;
    custom_cdc_union_t cdc_union;
    // end
    usb_endpoint_desc_t mgmt_ep;
    // TODO what does this do?
    //usb_endpoint_desc_t notification_ep;
    // end
    // begin CDC Data interface
    usb_interface_desc_t if_data;
    usb_endpoint_desc_t bulk_in;
    usb_endpoint_desc_t bulk_out;
    //end
} acm_config_desc_t;

extern acm_config_desc_t self_config_desc;
