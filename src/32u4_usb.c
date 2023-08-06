#include "32u4_usb.h"

#include "usb_data_link.h"
#include "queue/queue.h"

#include "usb_descriptors.h"
#include "usb_base_descriptors.h"
#include "usb_cdc_descriptors.h"
#include "usb_requests.h"

// for debugging
#include "drivers/uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>

#include <stdbool.h>
#include <stddef.h>

#define min(x, y) (((x) > (y)) ? (y):(x))
#define is_control_ep() ((UECFG0X & (3 << EPTYPE)) == 0)

static inline void set_flush_lock(uint8_t epnum);
static inline void clear_flush_lock(uint8_t epnum);
static inline bool is_flush_locked(uint8_t epnum);
static void fill_queue(uint8_t epnum);
static void fill_queue_ctrl(uint8_t epnum);
static void flush_queue(uint8_t epnum);
static void flush_queue_ctrl(uint8_t epnum);
static void handle_setup(uint8_t epnum);
static void clock_init(void);
static int usb_convert_iflags(int mask);

// array of endpoint handlers
static volatile usb_ep_ctx_t *usb_ep_handlers[NUM_EPS] = {0};

void atmega_xu4_start_usb(void) {

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

void atmega_xu4_set_ep_queue(char epnum, queue_t *data) {
    QUEUE_RESET(data);
    usb_ep_handlers[epnum]->data = data;
}

void atmega_xu4_set_ep_ctx(char epnum, usb_ep_ctx_t *ctx) {
    usb_ep_handlers[epnum] = ctx;
}

int usb_ep_write(int epnum, const char *buf, size_t len) {
    int bytecount = 0;
    // TODO optmize this to remove the copy: set the queue parameters s/t
    // we read directly from buf in the ISR, and then put back the "normal"
    // buffer when the transfer is complete :)
    queue_t *q = usb_ep_handlers[epnum]->data;
    while(bytecount < min(len, QUEUE_AVAIL(q))) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            queue_push(q, buf[bytecount++]);
        }
    }
    if(bytecount) {
        // enable TXINE
        UENUM = epnum;
        UEIENX |= _BV(TXINE);
    }
    return bytecount;
}

int usb_ep_read(int epnum, char *buf, size_t max_len) {
    int bytecount = 0;
    queue_t *q = usb_ep_handlers[epnum]->data;
    while(bytecount < min(max_len, q->size)) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            buf[bytecount++] = queue_pop(q);
        }
    }
    return bytecount;
}

bool usb_ep_set_callback(int epnum, usb_ep_cb *cb, void *ctx) {
    bool r = false;
    if(epnum < NUM_EPS) {
        usb_ep_handlers[epnum]->callback = cb;
        usb_ep_handlers[epnum]->cb_ctx = ctx;
        r = true;
    }

    return r;
}

bool usb_ep_flush(int epnum) {
    // instrs by figuring out where to put these.
    set_flush_lock(epnum);
    return false; // TODO check NAKINI or sw status flag + if queue is empty
    /*return (UESTA0X & 0x3);*/
}

bool usb_ep_flush_complete(int epnum) {
    // flush lock is only cleared after flush operation is complete
    bool r = !is_flush_locked(epnum);
    /*
     *if(!r) {
     *    UENUM = epnum;
     *    UEIENX |= _BV(TXINE);
     *}
     */
    return r;
}


void usb_ep_set_stall(int epnum, bool stall) {
    UEINTX = epnum;
    if(stall) {
        UECONX |= _BV(STALLRQ);
    }
    else {
        UECONX |= _BV(STALLRQC);
    }
}

void usb_set_addr(uint8_t address) {
    // TRM 22.7 "ADDEN and UADD shall not be written at the same time"
    // ... ok
    UDADDR = address |= _BV(ADDEN);
}
void usb_ep_set_interrupts(int epnum, int mask) {
    int iflags = usb_convert_iflags(mask);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        UENUM = epnum;
        UEIENX |= iflags;
    }
}

void usb_ep_clear_interrupts(int epnum, int mask) {
    int iflags = usb_convert_iflags(mask);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        UENUM = epnum;
        UEIENX &= ~iflags;
    }
}

static int usb_convert_iflags(int mask) {
    int iflags = 0;
    if(mask & USB_INT_SETUP) {
        iflags |= _BV(RXSTPI);
    }
    if(mask & USB_INT_IN) {
        iflags |= _BV(TXINE);
    }
    if(mask & USB_INT_OUT) {
        iflags |= _BV(RXOUTE);
    }
    if(mask & USB_INT_NAKIN) {
        iflags |= _BV(NAKINE);
    }
    if(mask & USB_INT_NAKOUT) {
        iflags |= _BV(NAKOUTE);
    }
    if(mask & USB_INT_FLOW) {
        iflags |= _BV(FLERRE);
    }
    if(mask & USB_INT_STALLRQ) {
        iflags |= _BV(STALLEDE);
    }
    return iflags;
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
    usb_token_t tok = 0;

    UEINT &= ~eps_to_service;
    /*uart_puts("COM\r\n", 5);*/
    for(int i = 0; i < NUM_EPS; i++) {
        tok = 0;
        if(!(eps_to_service & _BV(i))) {
            continue;
        }
        UENUM = i;
        uint8_t events = UEINTX;
        if(events & _BV(RXSTPI)) {
            PORTB |= _BV(PORTB5);
            usb_ep_handlers[i]->flags |= EP_XFER_STATE_SETUP;
            usb_ep_handlers[i]->flags &= ~(EP_XFER_STATE_IN | EP_XFER_STATE_OUT);
            handle_setup(i);
            tok |= SETUP;
            PORTB &= ~_BV(PORTB5);
        }
        else if((events & (_BV(RXOUTI) | _BV(TXINI))) == (_BV(RXOUTI) | _BV(TXINI))) {
            // IN and OUT: IN abort
            PORTB |= _BV(PORTB6);
            UEINTX &= ~(_BV(RXOUTI) | _BV(TXINI));
            usb_ep_handlers[i]->flags &= ~EP_XFER_STATE_SETUP;
            if(usb_ep_handlers[i]->flags & EP_XFER_STATE_IN) {
                usb_ep_handlers[i]->flags &= ~EP_XFER_STATE_IN;
                UEIENX &= ~_BV(TXINE);
                // TRM 22.14.1.1: abort logic
                // NOTE this is also needed for non-control IN xfer
                while((UESTA0X & 0x3)) {
                    // set RXOUTI to kill bank: IN abort
                    do {
                        UEINTX |= _BV(RXOUTI);
                    } while(UEINTX & _BV(RXOUTI)); // wait for bank to be killed
                }
                QUEUE_RESET(usb_ep_handlers[i]->data);
                UERST |= _BV(i);
                UERST &= ~_BV(i);
                UEIENX |= _BV(TXINE);
                UEIENX |= _BV(RXOUTE);
            }
            PORTB &= ~_BV(PORTB6);
        }
        else if(events & _BV(TXINI)) {
            PORTB |= _BV(PORTB4);
            // IN only
            usb_ep_handlers[i]->flags &= ~(EP_XFER_STATE_SETUP | EP_XFER_STATE_OUT);
            usb_ep_handlers[i]->flags |= EP_XFER_STATE_IN;
            flush_queue_ctrl(i);
            tok |= IN;
            PORTB &= ~_BV(PORTB4);
            // TODO non-control IN xfer
        }
        else if(events & _BV(RXOUTI)) {
            PORTB |= _BV(PORTB7);
            // OUT only
            usb_ep_handlers[i]->flags &= ~EP_XFER_STATE_SETUP;
            usb_ep_handlers[i]->flags |= EP_XFER_STATE_OUT;
            fill_queue_ctrl(i);
            tok |= OUT;
            PORTB &= ~_BV(PORTB7);
        }
        else {
            // NAKINI, NAKOUTI, STALLEDI
            usb_ep_handlers[i]->flags &= ~(EP_XFER_STATE_SETUP | EP_XFER_STATE_OUT | EP_XFER_STATE_IN);
            uart_puts("OTHER\r\n", 7);
        }
        if(usb_ep_handlers[i]->callback != NULL) {
            usb_ep_handlers[i]->callback(usb_ep_handlers[i]->cb_ctx, tok);
        }
    }
}

static inline void set_flush_lock(uint8_t epnum) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        usb_ep_handlers[epnum]->flags |= (EP_FLUSH);
        UENUM = epnum;
        UEIENX |= _BV(TXINE);
    }
}

static inline void clear_flush_lock(uint8_t epnum) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        usb_ep_handlers[epnum]->flags &= ~(EP_FLUSH);
    }
}
static inline bool is_flush_locked(uint8_t epnum) {
    // single-byte access, no atomic needed here
    return usb_ep_handlers[epnum]->flags & (EP_FLUSH) ? 1:0;
}

/**
 * Read the contents of a SETUP packet. The SETUP PID identifies the start of
 * a control transfer, and the request must fit in the buffer size w/o banking.
 * By the USB 2.0 spec, this is 64 bytes for full and high-speed devices, and
 * 8 bytes for a low-speed device.
 */
static inline void handle_setup(uint8_t epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    QUEUE_RESET(q);
    char c;
    if(QUEUE_AVAIL(q) < UEBCX) {
        // not enough space in the sw queue - stall to indicate failure to host
        usb_ep_set_stall(epnum, true);
    }
    else {
        while(UEBCX) {
            // TODO without the temporary, UEDATX is not read and this loop blocks.
            // wat.
            c = UEDATX;
            queue_push(q, c);
        }
        // clear RXSTPI to allow IN / OUT requests to be ACK'd
        // 22.12.2: clear all flags on setup recv
        UEINTX &= ~(_BV(RXSTPI) | _BV(RXOUTI) | _BV(TXINI));
    }
}

/**
 * Fill queue logic for CONTROL endpoints (no FIFOCON)
 */
static void fill_queue_ctrl(uint8_t epnum) {
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
        else {
            // all banks empty and sw queue non-full, yield until next OUT
            // or bank ready
            UEINTX &= ~_BV(RXOUTI); // clear to prevent further interrupts
            break;
        }
    }
}

/**
 * Read from a DPRAM buffer into the software queue, reading at most
 * min(UEBCX, q->size) bytes.
 * flag shall be set to the UEINTX flag which is to be cleared in order to not
 * re-trigger an interrupt - either RXSTPI or FIFOCON.
 */
static void fill_queue(uint8_t epnum) {
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
 * Flush queue logic for control endpoints (no FIFOCON)
 */
static void flush_queue_ctrl(uint8_t epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    // sizes defined in TRM 22.18.2, UECFG1X section
    size_t epsize = (UECFG1X & (0x7 << EPSIZE0)) >> EPSIZE0;
    epsize = 1 << (epsize + 3);
    size_t bytes_written = 0;
    while(!QUEUE_EMPTY(q)) {
        // space available in fifo
        if(UEBCX < epsize) {
            UEDATX = queue_pop(q);
            bytes_written++;
        }
    }
    if(bytes_written == epsize) {
        // ack IN if hw fifo is full
        UEINTX &= ~_BV(TXINI);
    }
    if((is_flush_locked(epnum) && QUEUE_EMPTY(q)) || bytes_written == 0) {
        // clear flush lock
        clear_flush_lock(0);
        // send data currently in buffer
        UEINTX &= ~_BV(TXINI);
        // disable IN interrupts
        UEIENX &= ~_BV(TXINE);
    }
}
/**
 * Write from the software queue to DPRAM, writing at most
 * min(UECFG1X[EPSIZE], q->size) bytes.
 */
static void flush_queue(uint8_t epnum) {
    queue_t *q = usb_ep_handlers[epnum]->data;
    UENUM = epnum;
    // sizes defined in TRM 22.18.2, UECFG1X section
    size_t epsize = (UECFG1X & (0x7 << EPSIZE0)) >> EPSIZE0;
    epsize = 1 << (epsize + 3);
    size_t bytes_written = 0;
    while(!QUEUE_EMPTY(q)) {
        // space available in fifo
        if(UEBCX < epsize) {
            UEDATX = queue_pop(q);
            bytes_written++;
        }
        //      > 0 non-full banks: non-control endpoints w/ multibank
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
    if(bytes_written == epsize) {
        // clear txini if usb buffer is full
        UEINTX &= ~_BV(TXINI);
    }
    // disable IN interrupts
    if(QUEUE_EMPTY(q)) {
        UEIENX &= ~_BV(TXINE);
        // clear flush lock
        clear_flush_lock(0);
    }
}

static void clock_init(void) {
    // 96MHz USB clock
    PLLCSR = 0x12;
    PLLFRQ = _BV(PLLUSB) | _BV(PLLTM1) | 0xA;
}
