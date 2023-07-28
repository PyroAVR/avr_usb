#pragma once
/**
 * Public interface for a USB data link layer driver.
 * Hardware-specific implementations MUST implement all functions declared in
 * this header according to their specifications. HW-specific implementations
 * MAY add additional functionality in order to enable software optimizations.
 *
 * TODO:
 *  - support multiple instances of a USB device peripheral. Do such chips
 *    exist?
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    IN = 1,
    OUT = 2,
    SETUP = 4,
    STALL = 8,
    NAK = 16, // set in conjunction with IN or OUT
} usb_token_t;

typedef void (usb_ep_cb)(void *ctx, usb_token_t token);

/**
 * Write data into the IN FIFO for the given endpoint
 * return value: bytes written from buf
 */
int usb_ep_write(int epnum, char *buf, size_t len);

/**
 * Read data up to the specified length from the OUT FIFO of the given endpoint.
 * return value: bytes read into buf
 */
int usb_ep_read(int epnum, char *buf, size_t max_len);

/**
 * Trigger a flush operation and return whether it has completed.
 * Whether this call blocks is implementation-defined
 * return value: true if flush is done
 */
bool usb_ep_flush(int epnum);

/**
 * Query whether an endpoint has been flushed (IN transfer complete).
 * Conditions: Data written as of last call to usb_ep_flush() has been
 * transmitted and ZLP or short packet have been relayed to the hardware.
 * return value: true if endpoint flush is complete.
 */
bool usb_ep_flush_complete(int epnum);

/**
 * Determine if an OUT transfer is done.
 * Conditions: Most recently received OUT packet has length < endpoint size
 * return value: true if all data for the active transfer is available locally.
 */
bool usb_ep_data_ready(int epnum);

/**
 * Set the callback for the given endpoint. The provided function will be
 * called with the provided context on all interrupt events enabled for the
 * associated endpoint.
 * return value: true on success, false for invalid arguments
 */
bool usb_ep_set_callback(int epnum, usb_ep_cb *cb, void *ctx);

/**
 * Set or clear a stall on the given endpoint. A STALL packet will be returned
 * for any requests to the associated endpoint after the stall condition is set,
 * and until the stall condition is cleared.
 */
void usb_ep_set_stall(int epnum, bool stall);

/**
 * Set enablement of IN and OUT interrupts for the endpoint. Once interrupts
 * have been disabled for the endpoint, only the local software or a reset will
 * be able to re-enable them.
 *
 * IN interrupts shall be disabled when no data is available for transmission to
 * the host. NAK will be sent in response to IN for the associated endpoint.
 *
 * OUT interrupts shall be disabled when the local software is unable to buffer
 * or process additional data. NAK will be sent in response to OUT for the
 * associated endpoint.
 *
 * Interrupts shall be re-enabled for the endpoint when processing may continue.
 */
void usb_ep_set_interrupts(int epnum, char int_flags);

/**
 * Set the address of the USB peripheral
 */
void usb_set_addr(uint8_t address);
