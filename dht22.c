#include "dht22.h"

#include <stdlib.h>
#include <stdint.h>

#include "timer.h"

typedef enum {
    DHT22_IDLE,
    DHT22_ACK,
    DHT22_STREAM,
} dht22_State;

dht22_State dht22_state;
uint16_t dht22_timestamp;
uint64_t dht22_stream;

PMCU_Error dht22_decode_stream(uint64_t stream, uint8_t *buffer) {
    buffer[0] = (uint8_t) ((stream & 0xff00000000) >> (8 * 4));
    buffer[1] = (uint8_t) ((stream & 0x00ff000000) >> (8 * 3));
    buffer[2] = (uint8_t) ((stream & 0x0000ff0000) >> (8 * 2));
    buffer[3] = (uint8_t) ((stream & 0x000000ff00) >> 8);

    buffer[4] = (uint8_t) (stream & 0x00000000ff);

    if ((uint8_t) (buffer[0] + buffer[1] + buffer[2] + buffer[3]) != buffer[4]) {
        return DHT22_WRONG_CHECKSUM;
    } else {
        return PMCU_OK;
    }
}

PMCU_Error dht22_read(uint8_t *buffer) {
    timer_Task timeout;
    PMCU_Error error;

    if (dht22_state != DHT22_IDLE) {
        return DHT22_OPERATION_NOT_ALLOWED;
    }

    dht22_stream = 1;
    dht22_state = DHT22_IDLE;

    // low signal of 1ms
    P1DIR |= BIT2;
    P1SEL &= ~BIT2;
    P1OUT &= ~BIT2;
    __delay_cycles(13000);

    dht22_state = DHT22_ACK;

    // sets as input, that will make the signal go high
    P1REN |= BIT2;
    P1DIR &= ~BIT2;
    P1SEL |= BIT2;
    P1OUT |= BIT2;

    // the timer will count every 325ns
    dht22_timestamp = TA0R;
    TA0CTL = TASSEL__SMCLK | MC__CONTINOUS | ID__4;
    TA0CCTL1 = CAP | CM_1 | CCIS_0 | SCS | CCIE; // caputres on rising edge

    timer_task_start(&timeout, 3); // after 3s goes timeout

    while (1) { // loops until all data is read or timeout is reached
        if (dht22_stream & 0x0000010000000000 || timeout.satisfied) {
            break;
        }
    }

    timer_task_cancel(&timeout);

    TA0CCTL1 &= ~CAP;
    TA0CTL = MC_0;

    if (timeout.satisfied) {
        dht22_state = DHT22_IDLE;
        return DHT22_TIMEOUT;
    }

    if ((error = dht22_decode_stream(dht22_stream, buffer)) != PMCU_OK) {
        return error;
    }

    dht22_state = DHT22_IDLE;

    return PMCU_OK;
}

#pragma vector=TIMER0_A1_VECTOR  // interrupt vector for TA0CCR1 to TA0CCR4
__interrupt void dht22_on_tick (void) {
    TA0CCTL1 &= ~CCIFG;

    switch (dht22_state) {
    case DHT22_ACK:
        dht22_state = DHT22_STREAM;
        TA0CCTL1 = CAP | CM_1 | CCIS_0 | SCS | CCIE; // captures on rising edge
        break;

    case DHT22_STREAM:
        // if was capturing on falling edge, measure is finished
        if (TA0CCTL1 & CM_2) {
            unsigned int delta;

            delta = TA0CCR1 - dht22_timestamp;
            TA0CCTL1 = CAP | CM_1 | CCIS_0 | SCS | CCIE;

            dht22_stream <<= 1;

            // The timer counts every 325.6ns.
            // A logical 1, in dht22 protocol, corresponds to 70us.
            // So 70us / 325.6ns = 214.98 timer cycles, which rounded is 214.
            if (delta >= 214) {
                dht22_stream |= 1;
            }
        } else {
            dht22_timestamp = TA0CCR1;
            TA0CCTL1 = CAP | CM_2 | CCIS_0 | SCS | CCIE; // captures on falling edge
        }
        break;
    }
}

