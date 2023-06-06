#pragma once
/**
 * AVR UART driver
 */

/**
 * Prepare the UART for interrupt-based communications
 * @param baud baud rate
 * @param rxb buffer to store received bytes in
 * @param txb buffer in which bytes to send will be placed
 * @param rx_sz size in bytes of rxb
 * @param tx_sz size in bytes of txb
 */
void configure_uart(
        unsigned long baud,
        char *rxb, char *txb,
        int rx_sz, int tx_sz
);

/**
 * Write a stream of data to the uart
 */
void uart_puts(char *data, int len);

/**
 * Non-blocking version of uart_puts
 */
int uart_puts_noblock(char *data, int len);

/**
 * Read a stream from the uart without blocking for non-present data.
 * Returns the number of bytes written into buf, which will be less than or
 * equal to len.
 */
int uart_gets(char *buf, int len);
