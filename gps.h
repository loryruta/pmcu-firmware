#ifndef GPS_H_
#define GPS_H_

#include <string.h>

#include "uart.h"
#include "uart_hub.h"

PMCU_Error gps_read_sentence(const char *sentence_type, char *buffer) { // i.e. sentence_type = $GPGGA,
    size_t i;

    __pmcu_handle(uart_read_until_string(UART_A0, sentence_type, NULL, NULL, 10));
    __pmcu_handle(uart_read_until_string(UART_A0, "*", buffer, 64, 10)); // a sentence shouldn't be longer than 64 chars

    for (i = 0; buffer[i] != '*'; i++);
    buffer[i] = '\0';

    return PMCU_OK;
}

#endif
