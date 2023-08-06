#pragma once
/**
 * Functions and types required to implement Chapter 9 of the USB 2.0 spec.
 */

#include "usb_data_link.h"

#include <stdint.h>

enum {
    USB_STD_STATE_SETUP,
    USB_STD_STATE_AWAIT_IN,
    USB_STD_STATE_AWAIT_OUT,
    USB_STD_STATE_AWAIT_FLUSH,
};

// TODO should this definition go elsewhere, maybe in a platform-specific hdr?
#if defined(__AVR_ATmega32U4__)
// No-copy optimization for AVR with SW-only queues
#include "queue/queue.h"
struct usb_std_req_ctx_S {
    usb_ep_cb *next;
    queue_t *ep_queue;
    uint8_t state;
    uint8_t addr;
};

#else

struct usb_std_req_ctx_S {
    usb_ep_cb *next;
    uint8_t state;
    uint8_t addr;
};

#endif

typedef struct usb_std_req_ctx_S usb_std_req_ctx_t;

void usb_std_req_handler(usb_std_req_ctx_t *ctx, usb_token_t token);
