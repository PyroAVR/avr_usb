#pragma once

#include "usb_data_link.h"
#include "queue/queue.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * Using the usb_ep_read/write functions implemented by this driver will cause
 * the data to be copied to/from the specified buffer.
 * To avoid additional copies of dynamic data, disable the appropriate events
 * and directly call queue_push/queue_pop where the circular buffer
 * functionality is required. If the data is known to be linear (eg. control
 * transfers), use QUEUE_RESET after a direct read of the queue data, or set
 * queue->buffer, queue->rd, queue->cap, and queue->size to enable the driver
 * to read directly from an external buffer.
 */

#if defined(__AVR_ATmega32U4__)
#define NUM_EPS 6
#endif

typedef struct usb_ep_ctx_S usb_ep_ctx_t;

typedef enum {
    EP_FLUSH = 1,
} usb_ep_flags;

struct usb_ep_ctx_S {
    // callbacks will run in the ISR context / with its priority.
    // Do not put long-running code here.
    usb_ep_cb *callback;
    void *cb_ctx;
    queue_t *data;
    // do not remove volatile: will remove atomic access guarantee
    volatile char flags;
};

void atmega_xu4_start_usb(void);

void atmega_xu4_set_ep_queue(char epnum, queue_t *data);

/**
 * Set storage location for the given handler.
 * The driver allocates sufficient static data to cover a pointer to a single
 * handler for each endpoint.
 */
void atmega_xu4_set_ep_ctx(char epnum, usb_ep_ctx_t *ctx);
