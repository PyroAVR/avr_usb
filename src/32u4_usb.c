#include "32u4_usb.h"

#include "usb_descriptors.h"

#include "usb_base_descriptors.h"
#include "usb_cdc_descriptors.h"
#include "usb_requests.h"

#include "queue/queue.h"

// for debugging
#include "drivers/uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdbool.h>

#define NUM_EPS 1

#if !defined(ATMEGA_XU4_USB_SW_QUEUE_LEN)
#define ATMEGA_XU4_USB_SW_QUEUE_LEN 128
#endif

#define EP0_LEN ATMEGA_XU4_USB_SW_QUEUE_LEN

#define min(x, y) (((x) > (y)) ? (y):(x))

/**
 * USB driver provides storage only for endpoint zero, which is required for all
 * USB devices. Other drivers or protocol layers may use
 * atmega_xu4_install_ep_handler to register callbacks for other endpoints.
 * Only one handler may be installed per endpoint, so care must be taken to
 * ensure this is the case. Subsequent calls to install_ep_handler will replace
 * previous handlers.
 */

static void handle_setup(usb_ep_ctx_t *ctx);

__attribute__((aligned(4)))
char ep0_buf[EP0_LEN];
queue_t ep0_queue;

// default endpoint 0 handler
usb_ep_ctx_t ep0_handler = {
    .callback = handle_setup,
    .data = &ep0_queue
};

// array of endpoint handlers
static usb_ep_ctx_t *usb_ep_handlers[NUM_EPS];


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

    // connect setup / control handler to ep0
    queue_init(&ep0_queue, ep0_buf, EP0_LEN);
    atmega_xu4_install_ep_handler(0, &ep0_handler);
}

bool atmega_xu4_install_ep_handler(int epnum, usb_ep_ctx_t *handler_ctx) {
    bool r = false;
    if(epnum < NUM_EPS) {
        usb_ep_handlers[epnum] = handler_ctx;
        r = true;
    }

    return r;
}

bool atmega_xu4_ep_flush(int epnum) {
    // TODO most times UENUM will already be set correctly. Avoid spurious
    // instrs by figuring out where to put these.
    UENUM = epnum;
    UEINTX &= ~_BV(TXINI);
    UEINTX &= ~_BV(FIFOCON);
    return (UESTA0X & 0x3);
}
/**
 * Write from the software queue to DPRAM, writing at most
 * min(UECFG1X[EPSIZE], q->size) bytes.
 */
static bool flush_lock = false;
static void flush_queue(char epnum, char flag) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    // TODO need a lookup here for actual count of bytes
    size_t epsize = (UECFG1X & (0x7 << EPSIZE0)) >> EPSIZE0;
    //   not control ep           r/w allowed
    /*
     *if((UECFG0X & (3 << 6)) && !(UEINTX & _BV(RWAL))) {
     *    return;
     *}
     */
    uart_puts((char*)&q->size, 2);
    while(!QUEUE_EMPTY(q)) {
        // FIXME hack for endpoint size
        size_t hw_size = UEBCX;
        if(hw_size < 64) {
            uart_puts("1", 1);
            UEDATX = queue_pop(q);
        }
        //      > 0 non-full banks
        else if(UESTA0X & 0x3) {
            uart_puts("fifo\r\n", 6);
            // clear FIFOCON to switch banks (IN) or TXINI to start ACKing
            // IN packets (SETUP)
            UEINTX &= ~flag;
        }
        else {
            uart_puts("break\r\n", 7);
            // yield until next IN or bank ready
            break;
        }
#if 0
        // clear fifocon bit to swap buffers if enabled.
        // FIXME hack for hw buffer len, need generic size interp
        if(UEBCX == EP0_LEN) {
            // clear txini to enable controller to reply
            UEINTX &= ~_BV(TXINI);
            // multi-bank enabled and    more than 0 non-empty banks
            if((UECFG1X & _BV(EPBK0)) && (UESTA0X & 0x3)) {
                // clear fifocon to switch banks
                UEINTX &= ~_BV(FIFOCON);
            }
            else {
                // yield until next IN or bank ready
                break;
            }
        }
        else {
            UEDATX = queue_pop(q);
        }
#endif
    }
    // FIXME hack to get it to transmit when queue is empty
    UEINTX &= ~_BV(TXINI);
    // disable IN interrupts
    if(QUEUE_EMPTY(q)) {
        UEIENX &= ~_BV(TXINE);
    }
    // flush bank (short packet write at end of xfer)
    // TODO we might not want to do this if the entire transfer made by the
    // application does not fit in the queue: could lead to early-aborts on
    // transfers.
    /*UEINTX &= ~_BV(TXINI);*/
    /*UEINTX = (1<<STALLEDI) | (1<<RXSTPI) | (1<<NAKOUTI) | (1<<RWAL);*/
    /*UEINTX &= ~_BV(TXINI);*/
}

/**
 * Read from a DPRAM buffer into the software queue, reading at most
 * min(UEBCX, q->size) bytes.
 * flag shall be set to the UEINTX flag which is to be cleared in order to not
 * re-trigger an interrupt - either RXSTPI or FIFOCON.
 */
static void fill_queue(char epnum, char flag) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    char c;
    // TODO is this check needed?
    // We should already know the state of the ep by this point.
    //   not control ep           r/w allowed
    if((UECFG0X & (3 << 6)) && !(UEINTX & _BV(RWAL))) {
        return;
    }
    while(!QUEUE_FULL(q)) {
        //  bank not empty
        if(UEBCX > 0) {
            // TODO without the temporary, UEDATX is not read and this loop blocks.
            // wat.
            c = UEDATX;
            queue_push(q, c);
        }
        //      > 0 non-empty banks
        else if(UESTA0X & 0x3) {
            // DPRAM buffer is empty, clear FIFOCON to swap banks (OUT), or
            // RXSTPI to disable interrupt (SETUP)
            // TODO might need to clear RXOUTI here on OUT endpoints, 22.13.1,
            // because RXOUTI is only cleared once in the interrupt handler,
            // not at every FIFOCON toggle
            UEINTX &= ~flag;
        }
        else {
            // all banks empty and sw queue non-full, yield until next OUT
            // or bank ready
            break;
        }
    }
}

void atmega_xu4_ep_stall(int epnum, bool stall_state) {
    UEINTX = epnum;
    if(stall_state) {
        UECONX |= _BV(STALLRQ);
    }
    else {
        UECONX |= _BV(STALLRQC);
    }
}

void write_usb_ep(int epnum) {
}

void read_usb_ep(int epnum) {
}

void handle_setup(usb_ep_ctx_t *ctx) {
    union {
        usb_req_hdr_t hdr;
        usb_req_std_t std;
        usb_req_val_t val;
        usb_req_get_desc_t get_desc;
    } *req;
    if(flush_lock) {
        if(QUEUE_EMPTY(&ep0_queue)) {
            flush_lock = false;
            UEINTX &= ~_BV(TXINE);
        }
        else {
            // enable tx interrupt and exit until queue is flushed.
            UEINTX |= _BV(TXINE);
            return;
        }
    }
    req = ep0_buf;
    // XXX: reset queue write ptr so we overwrite data w/o calling pop
    QUEUE_RESET(&ep0_queue);
    int addr = 0;
    switch(req->hdr.bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            switch(req->get_desc.type) {
                case USB_DESC_DEVICE:
                    uart_puts("device\r\n", 8);
                    for(size_t i = 0; i < sizeof(usb_device_desc_t); i++) {
                        queue_push(&ep0_queue, ((uint_least8_t *)&self_device_desc)[i]);
                    }
                    flush_lock = true;
                    // enable IN interrupts after SETUP
                    UEIENX |= _BV(TXINE);
                    /*write_usb_ep(0);*/
                    /*
                     *mqueue_init(&ep0_out, (uint_least8_t *)&self_device_desc, sizeof(usb_device_desc_t));
                     *write_usb_ep(0, &ep0_out);
                     *UEINTX = ~_BV(TXINI);
                     */
                break;

                case USB_DESC_CONFIGURATION:
                    uart_puts("config\r\n", 7);
                    if(req->std.wLength == sizeof(usb_config_desc_t)) {
                        // send just the config desc first
                        for(size_t i = 0; i < sizeof(usb_config_desc_t); i++) {
                            queue_push(&ep0_queue, ((uint_least8_t *)&self_config_desc)[i]);
                        }
                        flush_lock = true;
                        UEIENX |= _BV(TXINE);
                    }
                    else {
                        // then send entire configuration (repeating the config desc)
                        for(size_t i = 0; i < sizeof(acm_config_desc_t); i++) {
                            queue_push(&ep0_queue, ((uint_least8_t *)&self_config_desc)[i]);
                            if(!ep0_queue.op_ok) {
                                uart_puts("fail\r\n", 6);
                            }
                        }
                        flush_lock = true;
                        UEIENX |= _BV(TXINE);
                    }
                break;

#if 0
                case USB_DESC_STRING:
                // TODO on hold until utf16-le encoding is ready
                    /*
                     *write_usb_ep(0, (char*)str_desc_tab[req->get_desc.index], str_desc_tab[req->get_desc.index]->bLength);
                     *UEINTX = ~_BV(TXINI);
                     */
                break;
#endif
                default:
                    uart_puts("unsupported desc\n", 16);
                break;
            }
        break; // END DESC REQUESTS

        case USB_REQ_SET_ADDRESS:
            uart_puts("addr\n", 5);
            // TODO async this, make it wait on a flag.
            UEINTX = ~_BV(TXINI);
            // wait for IN packet before setting address
            addr = req->std.wValue;
            while(!(UEINTX & _BV(TXINI)));
            UDADDR = addr | _BV(ADDEN);
        break;

        default:
            uart_puts("unsupported req\r\n", 17);
            uart_puts(&ep0_buf, 64);
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
        UEIENX |= _BV(RXSTPE) | _BV(RXOUTI); // enable useful interrupts only
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
    uint8_t eps_to_service = UEINT;
    // bitfield structs are efficient in AVR
    /*
     *union {
     *    struct {
     *        uint8_t ep_type: 2;
     *        uint8_t ctrl_dir: 1;
     *        uint8_t in_xfer: 1;
     *        uint8_t ep_dir: 1;
     *    } fields;
     *    uint8_t value;
     *} ep_xfer_stat;
     */


    UEINT &= ~eps_to_service;
    if(eps_to_service & 1) {
        UENUM = 0;
        uint8_t events = UEINTX;
        /*
         *uint8_t uecfg0x = UECFG0X;
         *ep_xfer_stat.fields.ep_type = (uecfg0x & (3 << 6)) >> 6;
         *ep_xfer_stat.fields.ctrl_dir = UESTA1X & _BV(CTRLDIR);
         *ep_xfer_stat.fields.in_xfer = UEINTX & 1;
         *ep_xfer_stat.fields.ep_dir = uecfg0x & (1);
         */

        if(events & _BV(RXSTPI)) {
            // setup transfer sends host->dev data, but RXOUTI is not triggered.
            // endpoint will contain the request descriptor
            uart_puts("SETUP\r\n", 7);
            fill_queue(0, _BV(RXSTPI));
        }
        else if(events & _BV(TXINI)) {
            // IN transfer
            uart_puts("IN\r\n", 4);
            /*UEINTX &= ~ _BV(TXINI);*/
            // TODO statefully detect if we are doing setup or normal operation,
            // and change the "flag" argument accordingly.
            flush_queue(0, _BV(TXINI));
        }
        if(events & _BV(RXOUTI)) {
            // OUT transfer
            uart_puts("OUT\r\n", 5);
            UEINTX &= ~ _BV(RXOUTI);
            fill_queue(0, _BV(FIFOCON));
        }
        usb_ep_handlers[0]->callback(usb_ep_handlers[0]);
        /*UEINTX &= ~events;*/
    }
}
#if 0
ISR(USB_COM_vect) {
    /*uart_puts("ep\n", 2);*/
    int eps_to_service = UEINT;
    UEINT &= ~eps_to_service;
    if(eps_to_service & 1) {
        // endpoint 0
        UENUM = 0;
        int events = UEINTX;
        /*UEINTX &= ~events;*/
        /*read_usb_ep(0, &ep0_in);*/
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
#endif
