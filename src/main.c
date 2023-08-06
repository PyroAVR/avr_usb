#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
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

usb_ep_ctx_t contexts[NUM_EPS] = {
    [0 ... NUM_EPS-1] = {0}
};

// default endpoint 0 handler
usb_std_req_ctx_t ep0_std_ctx = {
    .next = NULL, // TODO fill w/ ACM handler
    .ep_queue = &ep0_queue,
    .state = USB_STD_STATE_SETUP
};

static bool run_handler = false;
static usb_token_t recent_token;
void schedule_handler(void *ctx, usb_token_t tok) {
    (void)ctx;
    // called from isr, no need for atomic
    run_handler = true;
    recent_token = tok;
}

char uart_bufs[2][512] = {0};

int main(void) {
    cli();
    // profiling pins:
    // B4 = USB_COM_ISR
    // B5 = TXINI
    // B6 = RXOUTI
    // B7 = schedule_handler
    DDRB |= 0xF0;
    PORTB &= ~0xF0;
    DDRC |= (1 << 7);
    PORTC &= ~(1 << 7); // disable pullup
    configure_uart(115200, uart_bufs[0], uart_bufs[1], 512, 512);

    // connect setup / control handler to ep0
    atmega_xu4_set_ep_ctx(0, &contexts[0]);
    queue_init(&ep0_queue, ep0_buf, EP0_LEN);
    atmega_xu4_set_ep_queue(0, &ep0_queue);
    /*usb_ep_set_callback(0, (usb_ep_cb*)usb_std_req_handler, &ep0_std_ctx);*/
    usb_ep_set_callback(0, (usb_ep_cb *)schedule_handler, NULL);

    atmega_xu4_start_usb();

    sei();
    for(;;) {
        if(run_handler) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                run_handler = false;
                usb_std_req_handler(&ep0_std_ctx, recent_token);
            }
        }
        asm("sleep");
    }
    return 0;
    for(;;) {
        uart_puts("test\r\n", 6);
        // heartbeat led
        PINC |= (1 << 7); // writing logical 1 to PIN toggles PORT (refman. 10.2.2)
        _delay_ms(500);
        // don't remove these... haven't figured out where they're supposed to be yet
        USBCON &= ~_BV(FRZCLK);
        UDCON &= ~_BV(DETACH);

    }
    return 0;
}
