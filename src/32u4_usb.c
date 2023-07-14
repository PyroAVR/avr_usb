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
#include <util/atomic.h>

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

static inline void set_flush_lock(int epnum);
static inline void clear_flush_lock(int epnum);
static inline bool is_flush_locked(int epnum);

__attribute__((aligned(4)))
char ep0_buf[EP0_LEN];
queue_t ep0_queue;

// default endpoint 0 handler
usb_ep_ctx_t ep0_handler = {
    .callback = handle_setup,
    .data = &ep0_queue,
    .flags = 0
};

// array of endpoint handlers
static volatile usb_ep_ctx_t *usb_ep_handlers[NUM_EPS];


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
    usb_ep_handlers[epnum]->flags |= (EP_FLUSH);
    // TODO most times UENUM will already be set correctly. Avoid spurious
    // instrs by figuring out where to put these.
    UENUM = epnum;
    UEINTX &= ~_BV(TXINI);
    UEINTX &= ~_BV(FIFOCON);
    UEIENX |= _BV(TXINE); // enable in interrupt
    return (UESTA0X & 0x3);
}

/**
 * Write from the software queue to DPRAM, writing at most
 * min(UECFG1X[EPSIZE], q->size) bytes.
 */
static void flush_queue(char epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    /*UEINTX &= ~_BV(TXINI);*/
    // sizes defined in TRM 22.18.2, UECFG1X section
    size_t epsize = (UECFG1X & (0x7 << EPSIZE0)) >> EPSIZE0;
    epsize = 1 << (epsize + 3);
    while(!QUEUE_EMPTY(q)) {
        // space available in fifo
        if(UEBCX < epsize) {
            UEDATX = queue_pop(q);
        }
        //      > 0 non-full banks
        else if(UESTA0X & 0x3) {
            // clear FIFOCON to switch banks (IN) or TXINI to start ACKing
            // IN packets (SETUP)
            // also clear TXINI to prevent additional interrupts
            // TODO clearing fifocon on control is supposedly incorrect
            // fw behavior: TRM 22.12 paragraph 2
            // TODO will clearing TXINI if the queue is empty, but
            // the entire transfer is not complete (eg. application fw is not
            // ready w/ the entire transfer / entire transfer does not fit in
            // the buffer) cause a short packet, and does that short packet
            // end the transfer prematurely?
            // USB section 5.8.3 seems to suggest this is the case... FIXME
            // this can probably be fixed by only clearing txini if the
            // remaining bytes in the queue are >= epsize. Only if the flush
            // lock is set do we forcibly clear txini.
            UEINTX &= ~_BV(FIFOCON);
            UEINTX &= ~_BV(TXINI); // two writes to give hw time to set txini
        }
        else {
            // queue not empty, but USB fifo is full
            // yield until next IN or bank ready
            break;
        }
    }
    // FIXME hack to get it to transmit when queue is empty
    UEINTX &= ~_BV(TXINI);
    // disable IN interrupts
    if(QUEUE_EMPTY(q)) {
        UEIENX &= ~_BV(TXINE);
        // clear flush lock
        clear_flush_lock(0);
    }
}

/**
 * Read from a DPRAM buffer into the software queue, reading at most
 * min(UEBCX, q->size) bytes.
 * flag shall be set to the UEINTX flag which is to be cleared in order to not
 * re-trigger an interrupt - either RXSTPI or FIFOCON.
 */
static void fill_queue(char epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    char c;
    while(!QUEUE_FULL(q)) {
        //  bank not empty
        if(UEBCX > 0) {
            // TODO without the temporary, UEDATX is not read and this loop blocks.
            // wat.
            c = UEDATX;
            queue_push(q, c);
        }
        // OUT ep and > 0 non-empty banks: TRM 22.13 OUT EP management
        else if(UESTA0X & 0x3) {
            // DPRAM buffer is empty, clear FIFOCON to swap banks
            // clear RXOUTI again to acknowledge interrupt, TRM 22.13.1,
            // avoids spurious interrupts if there is room in the queue and
            // there are non-empty banks remaining.
            UEINTX &= ~_BV(FIFOCON);
            UEINTX &= ~_BV(RXOUTI); // two writes to give hw time to set rxouti
        }
        else {
            // all banks empty and sw queue non-full, yield until next OUT
            // or bank ready
            UEINTX &= ~_BV(RXOUTI); // clear to prevent further interrupts
            break;
        }
    }
}

/**
 * Read the contents of a SETUP packet. The SETUP PID identifies the start of
 * a control transfer, and the request must fit in the buffer size w/o banking.
 * By the USB 2.0 spec, this is 64 bytes for full and high-speed devices, and
 * 8 bytes for a low-speed device.
 */
static inline void handle_control(char epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    QUEUE_RESET(q);
    char c;
    if(q->cap < UEBCX) {
        // not enough space in the sw queue - stall to indicate failure to host
        atmega_xu4_ep_stall(epnum, true);
    }
    else {
        while(UEBCX) {
            // TODO without the temporary, UEDATX is not read and this loop blocks.
            // wat.
            c = UEDATX;
            queue_push(q, c);
        }
        // clear RXSTPI to allow IN / OUT requests to be ACK'd
        UEINTX &= ~_BV(RXSTPI);
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

void handle_setup(usb_ep_ctx_t *ctx) {
    union {
        usb_req_hdr_t hdr;
        usb_req_std_t std;
        usb_req_val_t val;
        usb_req_get_desc_t get_desc;
    } *req;
    if(is_flush_locked(0)) {
        return;
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
                    set_flush_lock(0);
                    // enable IN interrupts after SETUP
                break;

                case USB_DESC_CONFIGURATION:
                    uart_puts("config\r\n", 8);
                    if(req->std.wLength == sizeof(usb_config_desc_t)) {
                        // send just the config desc first
                        for(size_t i = 0; i < sizeof(usb_config_desc_t); i++) {
                            queue_push(&ep0_queue, ((uint_least8_t *)&self_config_desc)[i]);
                        }
                        set_flush_lock(0);
                    }
                    else {
                        // then send entire configuration (repeating the config desc)
                        for(size_t i = 0; i < sizeof(acm_config_desc_t); i++) {
                            queue_push(&ep0_queue, ((uint_least8_t *)&self_config_desc)[i]);
                        }
                        set_flush_lock(0);
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

        case USB_REQ_SET_CONFIGURATION:
            // TODO handle actual configuration, this just ACKs the req.
            UEINTX = ~_BV(TXINI);
        break;

        case USB_REQ_GET_STATUS:
            // TODO endpoint vs. interface status
            // TODO actual rm-wake and self-power status
            // this indicates no rm-wake and bus-powered.
            queue_push(&ep0_queue, 0);
            queue_push(&ep0_queue, 0);
            set_flush_lock(0);
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

    UEINT &= ~eps_to_service;
    if(eps_to_service & 1) {
        UENUM = 0;
        uint8_t events = UEINTX;
        /*bool is_control_ep = UECFG0X & (3 << 6) ? false : true;*/
        /*
         *uint8_t uecfg0x = UECFG0X;
         *ep_xfer_stat.fields.ep_type = (uecfg0x & (3 << 6)) >> 6;
         *ep_xfer_stat.fields.ctrl_dir = UESTA1X & _BV(CTRLDIR);
         *ep_xfer_stat.fields.in_xfer = UEINTX & 1;
         *ep_xfer_stat.fields.ep_dir = uecfg0x & (1);
         */
#if 0
        // TODO this exits if r/w is not allowed on non-control endpoints...
        // but that flag is based on the current bank (I think).
        // might be best not to early-exit until all banks have been checked.
        //   not control ep           r/w allowed
        if(!is_control_ep && !(UEINTX & _BV(RWAL))) {
            return;
        }
#endif
        if(events & _BV(RXSTPI)) {
            // setup transfer sends host->dev data, but RXOUTI is not triggered.
            // endpoint will contain the request descriptor
            uart_puts("SETUP\r\n", 7);
            handle_control(0);
        }
        else if(events & _BV(TXINI)) {
            // IN transfer
            uart_puts("IN\r\n", 4);
            // clear TXINI for control endpoints, else FIFOCON
            // TODO flush also needs to repeatedly clear TXINI for non-setup
            // transactions.
            // may still need to statefully track setup, re:
            //  - txini vs. fifocon clearing (or both)
            //  - abort stage for IN packets during setup (control transfer)
            //  - whether or not to clear FIFOCON on OUT transactions
            //      - CONTROL endpoints are not allowed to use FIFOCON/RWAL or
            //        double-buffering, for some reason.
            flush_queue(0);
        }
        if(events & _BV(RXOUTI)) {
            // OUT transfer
            uart_puts("OUT\r\n", 5);
            fill_queue(0);
        }
        usb_ep_handlers[0]->callback(usb_ep_handlers[0]);
        /*UEINTX &= ~events;*/
    }
}

static inline void set_flush_lock(int epnum) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        usb_ep_handlers[epnum]->flags |= (EP_FLUSH);
        UENUM = epnum;
        UEIENX |= _BV(TXINE);
    }
}

static inline void clear_flush_lock(int epnum) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        usb_ep_handlers[epnum]->flags &= ~(EP_FLUSH);
    }
}
static inline bool is_flush_locked(int epnum) {
    // single-byte access, no atomic needed here
    return usb_ep_handlers[epnum]->flags & (EP_FLUSH) ? 1:0;
}
