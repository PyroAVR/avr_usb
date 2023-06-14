#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "usb_base_descriptors.h"
#include "usb_requests.h"

#include "usb_descriptors.h"

#include <drivers/uart.h>

#include <stdbool.h>

#define min(x, y) (((x) > (y)) ? (y):(x))

char uart_bufs[2][64] = {0};
__attribute__((aligned(4)))
char ep_buf[64] = {0};

typedef enum {
    SETUP,
    IN,
    OUT
} ep_state_t;

ep_state_t ep0_state;


void read_usb_ep(int epnum, char *buf, int len) {
    UENUM = epnum;
    for(int i = 0; i < min(len, UEBCX); i++) {
        buf[i] = UEDATX;
    }
    /*UEINTX = 0x6B;*/
}

void write_usb_ep(int epnum, char *buf, int len) {
    UENUM = epnum;
    for(int i = 0; i < len; i++) {
        UEDATX = buf[i];
    }
    /*UEINTX = (1<<STALLEDI) | (1<<RXSTPI) | (1<<NAKOUTI) | (1<<RWAL);*/
    /*UEINTX &= ~_BV(TXINI);*/
}
void setup_usb(void) {
    USBCON &= ~_BV(OTGPADE);
    UHWCON = 1; // enable USB I/O pad 3.3V regulator
    USBCON = (1 << USBE); // enable module & VBUS pres. detect
    UDIEN |= _BV(EORSTE);

    // initiate connection to host by connecting pullups
    USBCON = (1 << USBE) | (1 << OTGPADE) | (1 << VBUSTE); // enable module & VBUS pres. detect


    UDCON |= _BV(DETACH);
    _delay_ms(10);
    UDCON &= ~_BV(DETACH);

}

void clock_init(void) {
    // 96MHz USB clock
    PLLCSR = 0x12;
    PLLFRQ = _BV(PLLUSB) | _BV(PLLTM1) | 0xA;
}

int main(void) {
    cli();
    clock_init();
    DDRC |= (1 << 7);
    PORTC &= ~(1 << 7); // disable pullup
    configure_uart(115200, uart_bufs[0], uart_bufs[1], 64, 64);
    setup_usb();
    sei();
    for(;;) {
        USBCON &= ~_BV(FRZCLK);
        UDCON &= ~_BV(DETACH);
        PINC |= (1 << 7);

    }
    return 0;
}

static bool have_sent_config_hdr = false;
void handle_setup(int epnum) {
    union {
        usb_req_hdr_t hdr;
        usb_req_std_t std;
        usb_req_val_t val;
        usb_req_get_desc_t get_desc;
    } *req = ep_buf;
    int addr = 0;
    switch(req->hdr.bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            switch(req->get_desc.type) {
                case USB_DESC_DEVICE:
                    write_usb_ep(0, (char*)&self_device_desc, sizeof(usb_device_desc_t));
                    UEINTX = ~_BV(TXINI);
                break;

                case USB_DESC_CONFIGURATION:
                    if(!have_sent_config_hdr) {
                        write_usb_ep(0, ((char*)&self_config_desc), sizeof(usb_config_desc_t));
                        have_sent_config_hdr = true;
                    }
                    else {
                        // FIXME we need chain transfers here for larger descriptors, ep will get full
                        write_usb_ep(0, ((char*)&self_config_desc), 0x12);
                    }
                    UEINTX = ~_BV(TXINI);
                break;

                case USB_DESC_STRING:
                    write_usb_ep(0, (char*)str_desc_tab[req->get_desc.index], str_desc_tab[req->get_desc.index]->bLength);
                    UEINTX = ~_BV(TXINI);
                break;

                default:
                    uart_puts("unsupported desc\n", 16);
                break;
            }
        break;

        case USB_REQ_SET_ADDRESS:
            UEINTX = ~_BV(TXINI);
            // wait for IN packet before setting address
            addr = req->std.wValue;
            while(!(UEINTX & _BV(TXINI)));
            UDADDR = addr | _BV(ADDEN);
            uart_puts("addr\n", 5);
        break;


        default:
            uart_puts("unsupported req\n", 15);
            break;
    }
}

// USB general interrupt
ISR(USB_GEN_vect) {
    if(UDINT & _BV(EORSTI)) {
        // usb reset
        UDINT &= ~_BV(EORSTI);
        uart_puts("reset\n", 6);

        // enable only ep 0
        UENUM = 0;
        UECONX |= (1 << EPEN);
        UECFG0X = 0; // control type, direction is OUT (rx from our perspective)
        UECFG1X = _BV(EPSIZE0) | _BV(EPSIZE1) | _BV(ALLOC); // 64 byte buffer
        UEIENX |= _BV(RXSTPE); // enable setup interrupts only
        if(!(UESTA0X & _BV(CFGOK))) {
            uart_puts("failed\n", 7);
            return;
        }
        UERST = 0;
    }
    if(USBINT & _BV(VBUSTI)) {
        USBINT &= ~_BV(VBUSTI);
        /*UDIEN |= _BV(WAKEUPE) | _BV(SUSPE);*/
        UDCON &= ~_BV(DETACH);
        uart_puts("vbus\n", 5);
    }
    if(UDINT & _BV(WAKEUPI)) {
        UDINT &= ~_BV(WAKEUPI);
        USBCON &= ~_BV(FRZCLK);
        uart_puts("wake\n", 5);
    }
    if(UDINT & _BV(SUSPI)) {
        UDINT &= ~_BV(SUSPI);
        USBCON &= ~_BV(FRZCLK);
        uart_puts("susp\n", 5);
    }
}

// USB communication / USB endpoint interrupt
ISR(USB_COM_vect) {
    uart_puts("ep\n", 2);
    int eps_to_service = UEINT;
    UEINT &= ~eps_to_service;
    if(eps_to_service & 1) {
        // endpoint 0
        UENUM = 0;
        int events = UEINTX;
        /*UEINTX &= ~events;*/
        read_usb_ep(0, ep_buf, 64);
        if(events & (1 << RXSTPI)) {
            // setup packet
            UEINTX &= ~_BV(RXSTPI);
            uart_puts("setup\n", 6);
            UEIENX |= _BV(TXINE) | _BV(RXOUTE);
            handle_setup(0);
        }
        if(events & (1 << RXOUTI)) {
            // out packet, prepare to get data
            uart_puts("out\n", 4);
        }
        if(events & (1 << TXINI)) {
            // in packet, prepare to send data
            uart_puts("in\n", 3);
            /*UEINTX &= ~_BV(FIFOCON);*/
        }

    }
}
