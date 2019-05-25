#include <msp430.h>

void uart_hub_init() {
    P4DIR |= BIT2;
    P4SEL &= ~BIT2;
    P4OUT &= ~BIT2;

    P4DIR |= BIT1;
    P4SEL &= ~BIT1;
    P4OUT &= ~BIT1;
}

void uart_hub_select(unsigned int device) {
    P4OUT = (P4OUT & 0b11111001) | (device << 1 & 0b00000110);
}
