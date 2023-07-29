#include <drivers/uart.h>
#include <queue/queue.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/**
 * Convenience macro to enable transmission - enables interrupt on UDRE0 = 1
 */
#define uart_en_tx() (UCSR1B |= (1 << UDRIE1))


static queue_t uart_rx, uart_tx;

void configure_uart(
        unsigned long baud,
        char *rxb, char *txb,
        int rx_sz, int tx_sz
) {
    cli();
    // configure fifos
    queue_init(&uart_rx, rxb, rx_sz);
    queue_init(&uart_tx, txb, tx_sz);

    // UART, no parity, 1 stop bit, 8 bit char, + polarity
    UCSR1C &= ~(1 << UMSEL11);
    UCSR1C &= ~(1 << UMSEL10); 
    // RXi enable, TX ready i enable, RX enable, TX enable
    //(1 << UDRIE0) | 
    UCSR1B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN1);
    // U2X0 = 1 (double data rate)
    UCSR1A |= (1 << U2X1);
    // eqn on page 146 of atmega328p manual
    int ubrr = F_CPU/((((UCSR1A & (1 << U2X1)) ? 8UL:16UL)*baud))-1UL;
    // from nongnu avr-gcc faq...
    /*
     *int ubrr = ((F_CPU + baud * 8L)/(baud * 16L) - 1);
     */
    UBRR1H = (ubrr >> 8);
    UBRR1L = (ubrr & 0xff);
    sei();
}

void uart_puts(char *data, int len) {
    int i = 0;
    while(i < len) {
        i += uart_puts_noblock(data + i, len);
    }
}

int uart_puts_noblock(char *data, int len) {
    int i = 0;
    while(i < len) {
        queue_push(&uart_tx, data[i]);
        if(!uart_tx.op_ok) {
            break;
        }
        i++;
    }
    uart_tx.op_ok = true;
    uart_en_tx();
    return i;
}

int uart_gets(char *buf, int len) {
    int i = 0;
    if(len == 0 || !uart_rx.op_ok) {
        goto done;
    }
    while(i < len && uart_rx.size > 0) {
        char c = queue_pop(&uart_rx);
        buf[i] = c;
        i++;
    }
done:
    return i;
}

/******************************************************************************/

/**
 * Receive byte interrupt
 */

ISR(USART1_RX_vect) {
    // TODO this may not actually read UDR1, try using a temporary variable.
    queue_push(&uart_rx, UDR1);
}


/*
 * ready-to-send byte interrupt
 */
ISR(USART1_UDRE_vect) {
    char c = queue_pop(&uart_tx);
    if(QUEUE_EMPTY(&uart_tx) || !uart_tx.op_ok) {
        UCSR1B &= ~(1 << UDRIE1); // disable txi
        uart_tx.op_ok = true;
    }
    else {
        UDR1 = c;
    }
}
