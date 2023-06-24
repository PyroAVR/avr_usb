#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "usb_base_descriptors.h"
#include "usb_requests.h"

#include "usb_descriptors.h"
#include "32u4_usb.h"

#include <drivers/uart.h>
#include "monoqueue.h"

#include <stdbool.h>

char uart_bufs[2][64] = {0};

mqueue_t test_queue;

int main(void) {
    cli();
    DDRC |= (1 << 7);
    PORTC &= ~(1 << 7); // disable pullup
    configure_uart(115200, uart_bufs[0], uart_bufs[1], 64, 64);
    atmega_xu4_setup_usb();
    sei();
    char c;
    for(;;) {
        /*uart_puts("hello world\r\n", 13);*/
        /*mqueue_init(&test_queue, "hello world\r\n", 13);*/
        /*while(!MQUEUE_EMPTY(&test_queue)) {*/
            /*c = mqueue_pop(&test_queue);*/
            /*uart_puts(&c, 1);*/
        /*}*/
        PINC |= (1 << 7); // writing logical 1 to PIN toggles PORT (refman. 10.2.2)
        /*_delay_ms(500);*/
        // don't remove these... haven't figured out where they're supposed to be yet
        USBCON &= ~_BV(FRZCLK);
        UDCON &= ~_BV(DETACH);
        // heartbeat led

    }
    return 0;
}
