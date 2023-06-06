#include <usb_enumerate.h>
#include <usb_descriptors.h>

usb_device_desc_t self_device_desc = {
    .bLength = sizeof(usb_device_desc_t),
    .bDescriptorType = USB_DESC_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = 2, // CDC device
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize = 64,
    .idVendor = 0x0403,
    .idProduct = 0x6010,
    .bcdDevice = 0x0000,
    // these are the descriptor indices for string descs for these fields
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x3,
    .bNumConfigurations = 1
};

usb_config_desc_t self_config_desc = {
    .bLength = sizeof(usb_config_desc_t),
    .bDescriptorType = USB_DESC_CONFIGURATION,
    .wTotalLength = sizeof(self_acm_desc) + sizeof(self_acm_desc),
    .bNumInterfaces = 1, // acm interace
    .bConfigurationValue = 1,
    .iConfiguration = 4, // USB ACM interface (string desc)
    .bmAttributes = 0x80, // no self-power, no rm wake
    .bMaxPower = 50, // 100mA peak power
};
// do not separate this definition from the header above. They must be adjacent
// for the indexing logic to work
usb_interface_desc_t self_acm_desc = {
    .bLength = sizeof(usb_interface_desc_t),
    .bDescriptorType = USB_DESC_INTERFACE,
    .bInterfaceNumber = 0,
    .bInterfaceClass = 2, // communications (not data) interface class
    .bInterfaceSubClass = 2, // ACM device
    .bInterfaceProtocol = 0, // protocol not specified
    .iInterface = 4 // USB ACM interface (string desc)
};


#define manuf_str "Aperture Unlimited"
#define product_str "Portal Device"
#define sernum_str "8580"
#define usb_serial_str "USB ACM interface"


usb_string_desc_t self_manuf_desc = {
    .bLength = sizeof(manuf_str),
    .bDescriptorType = USB_DESC_STRING,
    .bString = manuf_str
};

usb_string_desc_t self_product_desc = {
    .bLength = sizeof(product_str),
    .bDescriptorType = USB_DESC_STRING,
    .bString = product_str
};

usb_string_desc_t self_sernum_desc = {
    .bLength = sizeof(sernum_str),
    .bDescriptorType = USB_DESC_STRING,
    .bString = sernum_str
};
usb_string_desc_t self_usb_serial_desc = {
    .bLength = sizeof(usb_serial_str),
    .bDescriptorType = USB_DESC_STRING,
    .bString = usb_serial_str,
};

usb_string_desc_t *str_desc_tab[] = {
    &self_manuf_desc,
    &self_product_desc,
    &self_sernum_desc,
    &self_usb_serial_desc,
};
