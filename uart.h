#ifndef UART_H_
#define UART_H_

#include <msp430.h>

#include <stdlib.h>
#include <stdint.h>

#include "circular_buffer.h"
#include "timer.h"
#include "error.h"

circular_buffer uart_read_buf_a0;
circular_buffer uart_read_buf_a1;

/*
 * A type representing the UART module.
 * On MSP430F5529 are available 2 USCI modules: A0 and A1.
 * - A0 ends on 3.3 (TX) and 3.4 (RX) pins.
 * - A1 ends on 4.4 (TX) and 4.5 (RX) pins.
 */
typedef uint16_t uart_module;

#define UART_A0 0x05C0
#define UART_A1 0x0600

#define UART_CTL1  0x00
#define UART_CTL0  0x01
#define UART_BR0   0x06
#define UART_BR1   0x07
#define UART_MCTL  0x08
#define UART_STAT  0x0A
#define UART_RXBUF 0x0C
#define UART_TXBUF 0x0E
#define UART_ABCTL 0x10
#define UART_RTCTL 0x12
#define UART_RRCTL 0x13
#define UART_IE    0x1C
#define UART_IFG   0x1D
#define UART_IV    0x1E

#define UART_REGISTER(module, offset) *((volatile unsigned char *) (module + offset))

/*
 * A type representing the UART settings.
 * A single byte that contains all configurable parts of UART communication (matching with UCAxCTL0 register).
 * The last 3 bits (lsb) are used for baud-rate configuration.
 */
typedef uint8_t uart_settings;

#define UART_PARITY_ENABLED UCPEN
#define UART_EVEN_PARITY    UCPAR
#define UART_MSB_FIRST      UCMSB
#define UART_7BITS_DATA     UC7BIT
#define UART_STOP_BIT       UCSPB

#define UART_BAUD_RATE_9600_ACLK_32KHZ  0b000
#define UART_BAUD_RATE_9600_SMCLK_1MHZ  0b001
#define UART_BAUD_RATE_9600_SMCLK_12MHZ 0b010

#define UART_BAUD_RATE_115200_SMCLK_12MHZ 0b100
#define UART_BAUD_RATE_115200_SMCLK_1MHZ  0b101

#define UART_WRITE_TIMEOUT 10
#define UART_READ_TIMEOUT  10

typedef void (*uart_rx_listener)(unsigned char);

/*
 * Sets up the given UART module.
 */
void uart_setup(uart_module module, uart_settings settings);

/*
 * Writes the byte out of the given UART module.
 */
void uart_write(uart_module module, uint8_t byte);

/*
 * Writes the bytes buffer out of the given UART module.
 */
void uart_write_buffer(uart_module module, const uint8_t *buffer, size_t buffer_length);

/*
 * Writes the string out of the given UART module.
 * Returns the number of bytes written, < of strlen(string) if timed out.
 */
int uart_write_string(uart_module module, const char *string);

PMCU_Error uart_read(uart_module module, uint8_t *byte, uint32_t timeout_delay);

PMCU_Error uart_read_buffer(uart_module module, uint8_t *buffer, size_t buffer_length, uint32_t timeout_delay);

PMCU_Error uart_match_string(uart_module module, const char *sample, uint32_t timeout_delay);

PMCU_Error uart_read_until_string(uart_module module, const char *sample, char *buffer, size_t buffer_length, uint32_t timeout_delay);

void uart_subscribe_rx_listener(uart_module module, uart_rx_listener listener);

#endif
