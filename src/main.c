#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "usb_base_descriptors.h"
#include "usb_requests.h"

#include "usb_descriptors.h"

#include "32u4_usb.h"

#include "usb_std.h"

#include <drivers/uart.h>

#include <stdbool.h>

#if !defined(ATMEGA_XU4_USB_SW_QUEUE_LEN)
#define ATMEGA_XU4_USB_SW_QUEUE_LEN 128
#endif

#define EP0_LEN ATMEGA_XU4_USB_SW_QUEUE_LEN

__attribute__((aligned(4)))
char ep0_buf[EP0_LEN];
queue_t ep0_queue;

// default endpoint 0 handler
usb_std_req_ctx_t ep0_std_ctx = {
    .next = NULL, // TODO fill w/ ACM handler
    .ep_queue = &ep0_queue,
    .state = USB_STD_STATE_SETUP
};

char uart_bufs[2][512] = {0};

int main(void) {
    cli();
    DDRC |= (1 << 7);
    PORTC &= ~(1 << 7); // disable pullup
    configure_uart(115200, uart_bufs[0], uart_bufs[1], 512, 512);

    atmega_xu4_setup_usb();
    // connect setup / control handler to ep0
    queue_init(&ep0_queue, ep0_buf, EP0_LEN);
    usb_ep_set_callback(0, (usb_ep_cb*)usb_std_req_handler, &ep0_std_ctx);

    sei();
    for(;;) {
        PINC |= (1 << 7); // writing logical 1 to PIN toggles PORT (refman. 10.2.2)
        _delay_ms(500);
        // don't remove these... haven't figured out where they're supposed to be yet
        USBCON &= ~_BV(FRZCLK);
        UDCON &= ~_BV(DETACH);
        // heartbeat led

    }
    return 0;
}
