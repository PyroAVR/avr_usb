#include "32u4_usb.h"

#include "usb_descriptors.h"

#include "usb_base_descriptors.h"
#include "usb_cdc_descriptors.h"
#include "usb_requests.h"

#include "monoqueue.h"

// for debugging
#include "drivers/uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdbool.h>

#define min(x, y) (((x) > (y)) ? (y):(x))

__attribute__((aligned(4)))
char ep_buf[64] = {0};

mqueue_t ep0_in, ep0_out;

static void clock_init(void) {
    // 96MHz USB clock
    PLLCSR = 0x12;
    PLLFRQ = _BV(PLLUSB) | _BV(PLLTM1) | 0xA;
}


void atmega_xu4_setup_usb(void) {

    clock_init();

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

void write_usb_ep(int epnum, mqueue_t *queue) {
    UENUM = epnum;
    // TODO this is broken: halts CPU, but is called in usb interrupt, so
    // interrupts are halted too. Also doesn't toggle txini and fifocon correctly
    while(!MQUEUE_EMPTY(queue)) {
        // clear fifocon bit to swap buffers if enabled.
        //  multi-bank enabled       bank full 
        if((UECFG1X & _BV(EPBK0)) && !UEBCX && ((UESTA0X & 0x3) < 2)) {
            // clear txini to enable controller to reply
            UEINTX = ~_BV(TXINI);
            // clear fifocon to indicate bank switch
            UEINTX &= ~_BV(FIFOCON);
        }
        else {
            // stall indefinitely for IN transfer from host
            // TODO this locks the CPU, need to relinquish control
            // interrupt routine should handle this when CPU is free, eg.
            // this function sets up the transfer and exits, but doesn't wait
            // for it to complete.
            continue;
        }
        UEDATX = mqueue_pop(queue);
    }
    /*UEINTX = (1<<STALLEDI) | (1<<RXSTPI) | (1<<NAKOUTI) | (1<<RWAL);*/
    /*UEINTX &= ~_BV(TXINI);*/
}

void read_usb_ep(int epnum, mqueue_t *queue) {
    UENUM = epnum;
    size_t len = 0;
    // TODO sizeof ep_buf
    while(len < min(64, UEBCX)) {
        ep_buf[len] = UEDATX;
        len++;
    }
    mqueue_init(queue, (const uint_least8_t *)ep_buf, len);
    /*UEINTX = 0x6B;*/
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
                    /*uart_puts("device\n", 7);*/
                    mqueue_init(&ep0_out, (uint_least8_t *)&self_device_desc, sizeof(usb_device_desc_t));
                    write_usb_ep(0, &ep0_out);
                    UEINTX = ~_BV(TXINI);
                break;

                case USB_DESC_CONFIGURATION:
                    if(!have_sent_config_hdr) {
                        // send just the config desc first
                        mqueue_init(&ep0_out, ((uint_least8_t *)&self_config_desc), sizeof(usb_config_desc_t));
                        write_usb_ep(0, &ep0_out);
                        have_sent_config_hdr = true;
                    }
                    else {
                        // then send entire configuration (repeating the config desc)
                        mqueue_init(&ep0_out, ((uint_least8_t *)&self_config_desc), sizeof(acm_config_desc_t));
                        write_usb_ep(0, &ep0_out);
                    }
                    UEINTX = ~_BV(TXINI);
                break;

                case USB_DESC_STRING:
                // TODO on hold until utf16-le encoding is ready
                    /*
                     *write_usb_ep(0, (char*)str_desc_tab[req->get_desc.index], str_desc_tab[req->get_desc.index]->bLength);
                     *UEINTX = ~_BV(TXINI);
                     */
                break;

                default:
                    /*uart_puts("unsupported desc\n", 16);*/
                break;
            }
        break;

        case USB_REQ_SET_ADDRESS:
            UEINTX = ~_BV(TXINI);
            // wait for IN packet before setting address
            addr = req->std.wValue;
            while(!(UEINTX & _BV(TXINI)));
            UDADDR = addr | _BV(ADDEN);
            /*uart_puts("addr\n", 5);*/
        break;


        default:
            /*uart_puts("unsupported req\n", 15);*/
            break;
    }
}

// USB general interrupt
ISR(USB_GEN_vect) {
    if(UDINT & _BV(EORSTI)) {
        // usb reset
        UDINT &= ~_BV(EORSTI);
        /*uart_puts("reset\n", 6);*/

        // enable only ep 0
        UENUM = 0;
        UECONX |= (1 << EPEN);
        UECFG0X = 0; // control type, direction is OUT (rx from our perspective)
        UECFG1X = _BV(EPSIZE0) | _BV(EPSIZE1) | _BV(ALLOC); // 64 byte buffer
        UEIENX |= _BV(RXSTPE); // enable setup interrupts only
        if(!(UESTA0X & _BV(CFGOK))) {
            /*uart_puts("failed\n", 7);*/
            return;
        }
        UERST = 0;
    }
    if(USBINT & _BV(VBUSTI)) {
        USBINT &= ~_BV(VBUSTI);
        /*UDIEN |= _BV(WAKEUPE) | _BV(SUSPE);*/
        UDCON &= ~_BV(DETACH);
        /*uart_puts("vbus\n", 5);*/
    }
    if(UDINT & _BV(WAKEUPI)) {
        UDINT &= ~_BV(WAKEUPI);
        USBCON &= ~_BV(FRZCLK);
        /*uart_puts("wake\n", 5);*/
    }
    if(UDINT & _BV(SUSPI)) {
        UDINT &= ~_BV(SUSPI);
        USBCON &= ~_BV(FRZCLK);
        /*uart_puts("susp\n", 5);*/
    }
}

// USB communication / USB endpoint interrupt
ISR(USB_COM_vect) {
    /*uart_puts("ep\n", 2);*/
    int eps_to_service = UEINT;
    UEINT &= ~eps_to_service;
    if(eps_to_service & 1) {
        // endpoint 0
        UENUM = 0;
        int events = UEINTX;
        /*UEINTX &= ~events;*/
        read_usb_ep(0, &ep0_in);
        if(events & (1 << RXSTPI)) {
            // setup packet
            UEINTX &= ~_BV(RXSTPI);
            /*uart_puts("setup\n", 6);*/
            UEIENX |= _BV(TXINE) | _BV(RXOUTE);
            handle_setup(0);
        }
        if(events & (1 << RXOUTI)) {
            // out packet, prepare to get data
            /*uart_puts("out\n", 4);*/
        }
        if(events & (1 << TXINI)) {
            // in packet, prepare to send data
            /*uart_puts("in\n", 3);*/
            /*UEINTX &= ~_BV(FIFOCON);*/
        }

    }
}
