#include "uart.h"

#include "circular_buffer.h"
#include "string.h"

uart_rx_listener uart_rx_listener_a0 = NULL;
uart_rx_listener uart_rx_listener_a1 = NULL;

// UART_BAUD_RATE_9600_ACLK_32KHZ
void uart_set_baud_rate_9600_aclk_32khz(uart_module module) {
    UART_REGISTER(module, UART_CTL1) |= UCSSEL_1;
    UART_REGISTER(module, UART_BR0) = 3;
    UART_REGISTER(module, UART_BR1) = 0;
    UART_REGISTER(module, UART_MCTL) = UCBRS_3 + UCBRF_0;
}

// UART_BAUD_RATE_9600_SMCLK_1MHZ
void uart_set_baud_rate_9600_smclk_1mhz(uart_module module) {
    UART_REGISTER(module, UART_CTL1) |= UCSSEL_2;
    UART_REGISTER(module, UART_BR0) = 109;
    UART_REGISTER(module, UART_BR1) = 0;
    UART_REGISTER(module, UART_MCTL) = UCBRS_1 + UCBRF_0;
}

// UART_BAUD_RATE_9600_SMCLK_12MHZ
void uart_set_baud_rate_9600_smclk_12mhz(uart_module module) {
    UART_REGISTER(module, UART_CTL1) |= UCSSEL_2;
    UART_REGISTER(module, UART_BR0) = 226;
    UART_REGISTER(module, UART_BR1) = 4;
    UART_REGISTER(module, UART_MCTL) = UCBRS_0 + UCBRF_0;
}

// UART_BAUD_RATE_115200_SMCLK_1MHZ
void uart_set_baud_rate_115200_smclk_1mhz(uart_module module) {
    UART_REGISTER(module, UART_CTL1) |= UCSSEL_2;
    UART_REGISTER(module, UART_BR0) = 8;
    UART_REGISTER(module, UART_BR1) = 0;
    UART_REGISTER(module, UART_MCTL) = UCBRS_6 + UCBRF_0;
}

// UART_BAUD_RATE_115200_SMCLK_12MHZ
void uart_set_baud_rate_115200_smclk_12mhz(uart_module module) {
    UART_REGISTER(module, UART_CTL1) |= UCSSEL_2;
    UART_REGISTER(module, UART_BR0) = 104;
    UART_REGISTER(module, UART_BR1) = 0;
    UART_REGISTER(module, UART_MCTL) = UCBRS_1 + UCBRF_0;
}

void uart_setup(uart_module module, uart_settings settings) {
    circular_buffer_init(module == UART_A0 ? &uart_read_buf_a0 : &uart_read_buf_a1);

    // Halts state-machine operation
    UART_REGISTER(module, UART_CTL1) |= UCSWRST;

    // Applies settings
    UART_REGISTER(module, UART_CTL0) = settings & 0xF8;

    // Sets baud-rate
    switch (settings & 0x07) {
    default:
    case UART_BAUD_RATE_9600_ACLK_32KHZ:
        uart_set_baud_rate_9600_aclk_32khz(module);
        break;

    case UART_BAUD_RATE_9600_SMCLK_1MHZ:
        uart_set_baud_rate_9600_smclk_1mhz(module);
        break;

    case UART_BAUD_RATE_9600_SMCLK_12MHZ:
        uart_set_baud_rate_9600_smclk_12mhz(module);
        break;

    case UART_BAUD_RATE_115200_SMCLK_1MHZ:
        uart_set_baud_rate_115200_smclk_1mhz(module);
        break;

    case UART_BAUD_RATE_115200_SMCLK_12MHZ:
        uart_set_baud_rate_115200_smclk_12mhz(module);
        break;
    }

    // Enables communication pins
    switch (module) {
    default:
    case UART_A0:
        P3SEL |= BIT3 + BIT4;
        break;
    case UART_A1:
        P4SEL |= BIT4 + BIT5;
        break;
    }

    // Starts state-machine
    UART_REGISTER(module, UART_CTL1) &= ~UCSWRST;

    // Enables receiving interrupts
    UART_REGISTER(module, UART_IE) |= UCRXIE;
}

void uart_write(uart_module module, uint8_t byte) {
    while (!(UART_REGISTER(module, UART_IFG) & UCTXIFG));
    UART_REGISTER(module, UART_TXBUF) = byte;
}

void uart_write_buffer(uart_module module, const uint8_t *buffer, size_t buffer_length) {
    size_t i;
    for (i = 0; i < buffer_length; i++) {
        uart_write(module, buffer[i]);
    }
}

int uart_write_string(uart_module module, const char *string) {
    while (*string != '\0') {
        uart_write(module, *string);
        string++;
    }
    return 0;
}

PMCU_Error uart_read(uart_module module, uint8_t *byte, uint32_t timeout_delay) {
    circular_buffer *r_buf;
    timer_Task timeout;

    r_buf = module == UART_A0 ? &uart_read_buf_a0 : &uart_read_buf_a1;

    if (timeout_delay) {
        timer_task_start(&timeout, timeout_delay);
    }

    while (1) {
        if ((r_buf->count > 0) || (timeout_delay && timeout.satisfied)) {
            break;
        }
    }

    if (timeout_delay) {
        timer_task_cancel(&timeout);

        if (timeout.satisfied) {
            return UART_TIMEOUT_ERROR;
        }
    }

    circular_buffer_read(r_buf, byte);
    return PMCU_OK;
}

PMCU_Error uart_read_buffer(uart_module module, uint8_t *buffer, size_t buffer_length, uint32_t timeout_delay) {
    size_t i;
    PMCU_Error error;

    for (i = 0; i < buffer_length; i++) {
        if ((error = uart_read(module, &buffer[i], timeout_delay)) != PMCU_OK) {
            return error;
        }
    }
    return PMCU_OK;
}

PMCU_Error uart_match_string(uart_module module, const char *sample, uint32_t timeout_delay) {
    PMCU_Error error;

    char byte;
    while (*sample != '\0') {
        if ((error = uart_read(module, (uint8_t *) &byte, timeout_delay)) != PMCU_OK) {
            return error;
        }
        if (byte != *sample) {
            return UART_UNEXPECTED_BYTE_ERROR;
        }
        sample++;
    }
    return PMCU_OK;
}

PMCU_Error uart_read_until_string(uart_module module, const char *sample, char *buffer, size_t buffer_length, uint32_t timeout_delay) {
    size_t i, j;
    char fallback;
    PMCU_Error err;

    if (!buffer) {
       buffer = &fallback;
       buffer_length = 0;
    }
    i = 0;
    j = 0;
    while (1) {
       if ((err = uart_read(module, (uint8_t *) &buffer[i], timeout_delay)) != PMCU_OK) {
           return err;
       }
       if (buffer[i] == sample[j]) {
           j++;
       } else if (j > 0) {
           j = 0;
       }
       if (sample[j] == '\0') {
           break;
       }
       if (buffer_length) { // if the buffer exists
           ++i;
           if (i >= buffer_length) {
               return UART_BUFFER_TOO_SMALL_ERROR;
           }
       }
    }
    return PMCU_OK;
}

void uart_subscribe_rx_listener(uart_module module, uart_rx_listener listener) {
    uart_rx_listener *rx_listener = (module == UART_A0) ? &uart_rx_listener_a0 : &uart_rx_listener_a1;
    *rx_listener = listener;
}

#pragma vector=USCI_A0_VECTOR
__interrupt void uart_on_a0_rx() {
    if (UCA0IFG & UCRXIFG) {
        volatile unsigned char tmp = UCA0RXBUF;
        circular_buffer_write(&uart_read_buf_a0, tmp);
        if (uart_rx_listener_a0) {
            uart_rx_listener_a0(tmp);
        }
    }
}

#pragma vector=USCI_A1_VECTOR
__interrupt void uart_on_a1_rx() {
    if (UCA1IFG & UCRXIFG) {
        volatile unsigned char tmp = UCA1RXBUF;
        circular_buffer_write(&uart_read_buf_a1, tmp);
        if (uart_rx_listener_a1) {
            uart_rx_listener_a1(tmp);
        }
    }
}
