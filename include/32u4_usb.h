#pragma once
#include "queue/queue.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct usb_ep_ctx_S usb_ep_ctx_t;
typedef void (usb_ep_cb)(usb_ep_ctx_t *ctx);

/**
 * Minimum data required by USB driver to connect SW to an endpoint.
 * Any data needed for the software may be attached to this struct, eg:
 *     typedef struct {
 *         usb_ep_ctx_t header;
 *         char *my_data;
 *         uint32_t my_flags;
 *     } my_sw_ep_t;
 *     my_sw_ep_t ep;
 *     atmega_xu4_install_ep_handler(2, (usb_ep_ctx_t *)&ep);
 */
struct usb_ep_ctx_S {
    // callbacks will run in the ISR context / with its priority.
    // Do not put long-running code here.
    usb_ep_cb *callback;
    queue_t *data;
};

//TODO all epnum can be char instead of int.

void atmega_xu4_setup_usb(void);

bool atmega_xu4_install_ep_handler(int epnum, usb_ep_ctx_t *handler_ctx);

void atmega_xu4_ep_stall(int epnum, bool stall_state);

/**
 * Clear TXINI and then FIFOCON.
 * Returns true if NBUSYBK > 0.
 * If there are no filled banks, calling this function results in undefined
 * behavior.
 * suggested use for large transfers:
 *     while(!atmega_xu4_ep_flush(epnum));
 */
bool atmega_xu4_ep_flush(int epnum);
