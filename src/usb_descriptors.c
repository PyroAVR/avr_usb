#include <usb_base_descriptors.h>
#include <usb_descriptors.h>

usb_device_desc_t self_device_desc = {
    .bLength = sizeof(usb_device_desc_t),
    .bDescriptorType = USB_DESC_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = 2, // CDC device
    .bDeviceSubClass = 2, // abstract control model
    .bDeviceProtocol = 0,
    .bMaxPacketSize = 64,
    .idVendor = 0x0401,
    .idProduct = 0x6010,
    .bcdDevice = 0x0000,
    // these are the descriptor indices for string descs for these fields
    // TODO: disabled until utf16le encoding is ready
    .iManufacturer = 0x00,
    .iProduct = 0x00,
    .iSerialNumber = 0x0,
    .bNumConfigurations = 1
};

acm_config_desc_t self_config_desc = {
    .self_config_desc = {
        .bLength = sizeof(usb_config_desc_t),
        .bDescriptorType = USB_DESC_CONFIGURATION,
        .wTotalLength = sizeof(self_config_desc),
        .bNumInterfaces = 2, // acm interace
        .bConfigurationValue = 1,
        // TODO disabled until utf16le encoding is ready
        .iConfiguration = 0, // USB ACM interface (string desc)
        .bmAttributes = 0x80, // no self-power, no rm wake
        .bMaxPower = 50, // 100mA peak power
    },
    .iad = {
        .bLength = sizeof(usb_assoc_desc_t),
        .bDescriptortype = USB_DESC_INTERFACE_ASSOC,
        .bFirstInterface = 0,
        .bInterfaceCount = 2, // FIXME I have no idea what this is counting
        .bFunctionClass = 2, // USB CDC
        .bFunctionSubClass = 2, // ACM
        .bFunctionProtocol = 1, // AT commands, not sure if we want this
        .iFunction = 0 // str desc disabled
    },
    .cdc_header = {
        .bLength = sizeof(usb_cdc_header_func_desc_t),
        .bDescriptorType = USB_CDC_DESC_CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_DESC_HEADER_FUNCTIONAL,
        .bcdCDC = 0x0110 // USBCDC1.1
    },
    .acm_desc = {
        .bLength = sizeof(usb_cdc_acm_func_desc_t),
        .bDescriptorType = USB_CDC_DESC_CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_DESC_ACM,
        .bmCapabilities = 0xF // support all
    },
    .cdc_union = {
        .bLength = sizeof(custom_cdc_union_t),
        .bDescriptorType = USB_CDC_DESC_CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_DESC_UNION,
        .bControlInterface = 0,
        .bSubordinateInterface = 1 // data interface comes after comms
    },
    .cdc_call_mgmt = {
        .bLength = sizeof(usb_cdc_call_mgmt_desc_t),
        .bDescriptorType = USB_CDC_DESC_CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_DESC_CALL_MGMT,
        .bmCapabilities = 3, // support all
        .bDataInterface = 1, // data interface comes after comms TODO if I make this 0, do we not multiplex?
    },
    .if_comm = {
        .bLength = sizeof(usb_interface_desc_t),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = 2, // communications (not data) interface class
        .bInterfaceSubClass = 2, // ACM device
        .bInterfaceProtocol = 0, // protocol not specified
        // TODO disabled until utf16le encoding is ready
        .iInterface = 0 // USB ACM interface (string desc)
    },
    .mgmt_ep = {
        .bLength = sizeof(usb_endpoint_desc_t),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = 1,
        .bmAttributes = USB_EP_ATTRS_CTRL | USB_EP_ATTRS_NO_SYNC | USB_EP_ATTRS_DATA,
        .wMaxPacketSize = 64,
        .bInterval = 10 // FIXME not an isochronous ep
    },
    /*
     *.notification_ep = {
     *    .bLength = sizeof(usb_endpoint_desc_t),
     *    .bDescriptorType = USB_DESC_ENDPOINT,
     *    .bEndpointAddress = 1,
     *    .bmAttributes = ,
     *    .wMaxPacketSize = 64, // TODO how small can I make this? only 832 bytes dpram
     *    .bInterval = 
     *},
     */
    .if_data = {
        .bLength = sizeof(usb_interface_desc_t),
        .bDescriptorType = USB_DESC_INTERFACE,
        .bInterfaceNumber = 1,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0xA,  // data interface class
        .bInterfaceSubClass = 0, // not used
        .bInterfaceProtocol = 0, // not specified
        .iInterface = 0
    },
    .bulk_in = {
        .bLength = sizeof(usb_endpoint_desc_t),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = (2 | USB_EP_DIR_IN),
        .bmAttributes = USB_EP_ATTRS_BULK | USB_EP_ATTRS_NO_SYNC | USB_EP_ATTRS_DATA,
        .wMaxPacketSize = 64,
        .bInterval = 0
    },
    .bulk_out = {
        .bLength = sizeof(usb_endpoint_desc_t),
        .bDescriptorType = USB_DESC_ENDPOINT,
        .bEndpointAddress = (3 | USB_EP_DIR_OUT),
        .bmAttributes = USB_EP_ATTRS_BULK | USB_EP_ATTRS_NO_SYNC | USB_EP_ATTRS_DATA,
        .wMaxPacketSize = 64,
        .bInterval = 0
    }
};


#define langids_str "\x04\x09"
#define manuf_str "A\0p\0e\0r\0t\0u\0r\0e\0 \0U\0n\0l\0i\0m\0i\0t\0e\0d\0"
#define product_str "Portal Device"
#define sernum_str "8580"
#define usb_serial_str "USB ACM interface"

usb_string_desc_t langids_desc = {
    .bLength = sizeof(langids_str) + 2,
    .bDescriptorType = USB_DESC_STRING,
    .bString = langids_str
};

usb_string_desc_t self_manuf_desc = {
    .bLength = sizeof(manuf_str) + 2,
    .bDescriptorType = USB_DESC_STRING,
    .bString = manuf_str
};

usb_string_desc_t self_product_desc = {
    .bLength = sizeof(product_str) + 2,
    .bDescriptorType = USB_DESC_STRING,
    .bString = product_str
};

usb_string_desc_t self_sernum_desc = {
    .bLength = sizeof(sernum_str) + 2,
    .bDescriptorType = USB_DESC_STRING,
    .bString = sernum_str
};
usb_string_desc_t self_usb_serial_desc = {
    .bLength = sizeof(usb_serial_str) + 2,
    .bDescriptorType = USB_DESC_STRING,
    .bString = usb_serial_str,
};

usb_string_desc_t *str_desc_tab[] = {
    &langids_desc,
    &self_manuf_desc,
    &self_product_desc,
    &self_sernum_desc,
    &self_usb_serial_desc,
};
