#pragma once

#include "usb_enumerate.h"

extern usb_device_desc_t self_device_desc;

extern usb_string_desc_t self_manuf_desc;
extern usb_string_desc_t self_product_desc;
extern usb_string_desc_t self_sernum_desc;

extern usb_string_desc_t *str_desc_tab[];

extern usb_config_desc_t self_config_desc;
extern usb_interface_desc_t self_acm_desc;
