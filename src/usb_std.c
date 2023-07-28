#include <usb_std.h>

#include "usb_data_link.h"
#include "usb_requests.h"
#include "usb_descriptors.h"

// DEBUG
#include "drivers/uart.h"
#include "avr/io.h"

static void handle_std_request(usb_std_req_ctx_t *ctx, usb_req_t *req);
static void handle_get_desc(usb_std_req_ctx_t *ctx, usb_req_t *req);

void usb_std_req_handler(usb_std_req_ctx_t *ctx, usb_token_t token) {
    usb_req_t *req;
    /*uart_puts("std: ", 5);*/
    switch(ctx->state) {
        case USB_STD_STATE_AWAIT_FLUSH:
            uart_puts("await flush\r\n", 13);
            if(!usb_ep_flush_complete(0)) {
                // with ideal driver configuration, this should never be hit -
                // handlers are only called when their requested event is avail.
                uart_puts(" still\r\n", 8);
                return;
            }
            else {
                // return to setup state post-flush: a completed flush implies
                // the next packet will be SETUP.
                uart_puts("to setup\r\n", 10);
                ctx->state = USB_STD_STATE_SETUP;
            }
        break;

        case USB_STD_STATE_SETUP:
            /*uart_puts("setup\r\n", 7);*/
#if defined(__AVR_ATmega32U4__)
            // directly access buffer: control transfers always reset the queue
            req = ctx->ep_queue->buffer;
            // reset queue write ptr so we overwrite data w/o calling pop
            QUEUE_RESET(ctx->ep_queue);
#else
            int size = usb_ep_read(0, ep0_buf, sizeof(ep0_buf));
            if(size < sizeof(ep0_buf)) {
                // protocol invalid: setup packet is always eight bytes.
                return;
            }
#endif
            handle_std_request(ctx, req);
        break;

        case USB_STD_STATE_AWAIT_IN_ADDR:
            uart_puts("await addr", 10);
            if(token & IN) {
                uart_puts(" set\r\n", 6);
                UENUM = 0;
                UEINTX &= ~_BV(TXINI);
                /*usb_ep_flush(0);*/
                usb_set_addr(ctx->addr);
                QUEUE_RESET(ctx->ep_queue);
            }
            else {
                uart_puts(" not set\r\n", 10);
            }
            // else, called for some other event - do not set address, but return
            // to default state
            ctx->state = USB_STD_STATE_SETUP;
            return;
        break;

        case USB_STD_STATE_AWAIT_IN:
        case USB_STD_STATE_AWAIT_OUT:
        default:
            uart_puts("default\r\n", 9);
        break;
    }
}

static void handle_std_request(usb_std_req_ctx_t *ctx, usb_req_t *req) {
    // TEMP
    char device_status[2] = {0, 0};
    switch(req->hdr.bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            handle_get_desc(ctx, req);
        break;

        case USB_REQ_SET_ADDRESS:
            uart_puts("addr\n", 5);
            // wait for IN packet before setting address
            ctx->addr = req->std.wValue;
            usb_ep_set_interrupts(0, USB_INT_IN);
            ctx->state = USB_STD_STATE_AWAIT_IN_ADDR;
        break;

        case USB_REQ_SET_CONFIGURATION:
            // TODO handle actual configuration, this just ACKs the req.
            uart_puts("set conf\r\n", 10);
            // should ack even if empty... not sure. TODO
            usb_ep_flush(0);
            /*UEINTX = ~_BV(TXINI);*/
            /*configure_acm_bulk();*/
        break;

        case USB_REQ_GET_STATUS:
            // TODO endpoint vs. interface status
            // TODO actual rm-wake and self-power status
            // this indicates no rm-wake and bus-powered.
            uart_puts("status\r\n", 8);
            usb_ep_write(0, device_status, 2);
            usb_ep_flush(0);
            ctx->state = USB_STD_STATE_AWAIT_FLUSH;
        break;

        default:
            uart_puts("unsupported req\r\n", 17);
            /*uart_puts(&ep0_buf, 64);*/
            break;
    }
}

static void handle_get_desc(usb_std_req_ctx_t *ctx, usb_req_t *req) {
    switch(req->get_desc.type) {
        case USB_DESC_DEVICE:
            uart_puts("device\r\n", 8);
            usb_ep_write(0, &self_device_desc, sizeof(usb_device_desc_t));
            usb_ep_set_interrupts(0, USB_INT_IN);
            usb_ep_flush(0);
            ctx->state = USB_STD_STATE_AWAIT_FLUSH;
        break;

        case USB_DESC_CONFIGURATION:
            uart_puts("config\r\n", 8);
            // length will vary: first req. is for the config desc, then
            // for the entire configuration.
            usb_ep_write(0, &self_config_desc, req->std.wLength);
            usb_ep_flush(0);
            ctx->state = USB_STD_STATE_AWAIT_FLUSH;
#if 0
            if(req->std.wLength == sizeof(usb_config_desc_t)) {
                // send just the config desc first
                /*
                 *for(size_t i = 0; i < sizeof(usb_config_desc_t); i++) {
                 *    queue_push(ctx->ep_queue, ((uint_least8_t *)&self_config_desc)[i]);
                 *}
                 */
                usb_ep_flush(0);
            }
            else {
                // then send entire configuration (repeating the config desc)
                /*
                 *for(size_t i = 0; i < sizeof(acm_config_desc_t); i++) {
                 *    queue_push(&ep0_queue, ((uint_least8_t *)&self_config_desc)[i]);
                 *}
                 */
                usb_ep_flush(0);
            }
#endif
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
}
