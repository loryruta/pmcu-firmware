#include "modem.h"

#include "uart.h"
#include "circular_buffer.h"

#include <string.h>

#define MODEM_LOG_LENGTH 96
#define MODEM_SYNC_RETRIALS 10

char modem_buffer[MODEM_LOG_LENGTH];
char modem_log[MODEM_LOG_LENGTH];
size_t modem_ret;

PMCU_Error modem_read(char *buffer) {
    PMCU_Error error;
    char *cp_buf;

    cp_buf = buffer;

    if ((error = uart_match_string(UART_A0, "\r\n", MODEM_COMMAND_TIMEOUT)) != PMCU_OK) {
        return error;
    }

    while (1) {
        uart_read(UART_A0, (uint8_t *) buffer, MODEM_COMMAND_TIMEOUT);
        if (*buffer == '\r') {
            break;
        }
        buffer++;
    }

    uart_read(UART_A0, (uint8_t *) buffer, MODEM_COMMAND_TIMEOUT);
    if (*buffer != '\n') {
        return UART_UNEXPECTED_BYTE_ERROR;
    }

    *buffer = '\0';

    strcpy(modem_log, "MODEM AT< ");
    strcat(modem_log, cp_buf);
    PMCU_log(modem_log);

    return PMCU_OK;
}

PMCU_Error modem_read_and_expect(const char *expected) {
    modem_read(modem_buffer);
    if (strcmp(modem_buffer, expected) != 0) {
        return SIM800L_UNEXPECTED_RESPONSE_ERROR;
    } else {
        return PMCU_OK;
    }
}

void modem_execute(const char *cmd) {
    size_t i;
    i = 0;
    while (cmd[i] != '\0') {
        uart_write(UART_A0, cmd[i]);
        i++;
    }

    strcpy(modem_log, "MODEM AT> ");
    strcat(modem_log, cmd);
    PMCU_log(modem_log);

    uart_write(UART_A0, '\r');
}

PMCU_Error modem_sync() {
    uint8_t tmp;

    /*
     * IMPORTANT:
     * Wait some time before sending any command!
     * If during power up SIM800L receives a command will send out
     * RDY, Call Ready, SIM Ready messages. We want to skip them!
     */
    __delay_cycles(12880000 * 6);

    /* Try to exit AT+CIPSEND state if was in */
    uart_write(UART_A0, 27);

    /* Wait for SIM800L to respond OK to AT command */
    while (1) {
        modem_execute("AT");
        if (uart_read_until_string(UART_A0, "OK\r\n", NULL, NULL, 3) != PMCU_OK) {
            continue;
        }
        break;
    }

    modem_execute("ATZ");
    uart_read_until_string(UART_A0, "OK\r\n", NULL, NULL, MODEM_COMMAND_TIMEOUT);

    /* Force ATE0 */
    while (1) {
        modem_execute("ATE0");
        if ((pmcu_error = uart_read_until_string(UART_A0, "\r", NULL, NULL, MODEM_COMMAND_TIMEOUT)) != PMCU_OK) {
            continue;
        }
        if ((pmcu_error = uart_match_string(UART_A0, "\r\nOK\r\n", MODEM_COMMAND_TIMEOUT)) != PMCU_OK) {
            continue;
        }
        break;
    }

    /* Force AT+CMEE=2 */
    while (1) {
        modem_execute("AT+CMEE=2");
        if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
            continue;
        }
        break;
    }

    /* Force AT+CIPSPRT=1 */
    while (1) {
        modem_execute("AT+CIPSPRT=1");
        if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
            continue;
        }
        break;
    }

    /* Wait until find an available network */
    while (1) {
        __delay_cycles(12288000 * 3);
        modem_execute("AT+CREG?");
        if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
            continue;
        }
        tmp = strcmp(modem_buffer, "+CREG: 0,1") != 0 && strcmp(modem_buffer, "+CREG: 0,5") != 0;
        if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
            continue;
        }
        if (tmp) {
            continue;
        }
        break;
    }

    /* Wait a few seconds to settle */
    __delay_cycles(12288000 * 3);

    return PMCU_OK;
}

PMCU_Error modem_get_imei(char *imei) {
    modem_execute("AT+CGSN");
    if ((pmcu_error = modem_read(imei)) != PMCU_OK) {
        return pmcu_error;
    }
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    return PMCU_OK;
}

PMCU_Error modem_get_location(char *location) {
    modem_execute("AT+CIPGSMLOC=1,1");
    if ((pmcu_error = modem_read(location)) != PMCU_OK) {
        return pmcu_error;
    }
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    return PMCU_OK;
}

PMCU_Error modem_gprs_attach() {
    /* Force AT+CGATT=1 */
    while (1) {
        modem_execute("AT+CGATT=1");
        if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
            continue;
        }
        break;
    }

    // ************************************************** bearer profile creation for at+cipgsmloc

    // closes previous opened bearer profile (if any)
    modem_execute("AT+SAPBR=0,1");
    if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
        return pmcu_error;
    }

    // sets the connection type to gprs
    modem_execute("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // sets apn to "TM"
    modem_execute("AT+SAPBR=3,1,\"APN\",\"TM\"");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // opens the created bearer profile
    modem_execute("AT+SAPBR=1,1");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // queries the created bearer
    modem_execute("AT+SAPBR=2,1");
    if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
        return pmcu_error;
    }
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // ************************************************** pdp context creation for tcp/ip network

    // deactivates current pdp context
    modem_execute("AT+CIPSHUT");
    if ((pmcu_error = modem_read_and_expect("SHUT OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // sets single ip connection
    modem_execute("AT+CIPMUX=0");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // starts task, sets apn, empty username and password
    modem_execute("AT+CSTT=\"TM\",\"\",\"\"");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // brings up wireless connection
    modem_execute("AT+CIICR");
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }

    // gets pdp ip address (should be equal to bearer profile's one)
    modem_execute("AT+CIFSR");
    if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
        return pmcu_error;
    }

    return PMCU_OK;
}

/*
int modem_gprs_detach() {
    // Closes bearer profile
    modem_execute("AT+SAPBR=0,1");
    modem_read(modem_log, MODEM_LOG_LENGTH);

    // Deactivates PDP context
    modem_execute("AT+CIPSHUT");
    modem_read(modem_log, MODEM_LOG_LENGTH);

    // Deattaches GPRS
    modem_execute("AT+CGATT=0");
    modem_read(modem_log, MODEM_LOG_LENGTH);

    return MODEM_OK;
}
*/

PMCU_Error modem_tcp_connect(const char *host, const char *port) {
    modem_execute("AT+CIPCLOSE");
    if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
        return pmcu_error;
    }

    // connects to the tcp server
    strcpy(modem_buffer, "AT+CIPSTART=\"TCP\",\"");
    strcat(modem_buffer, host);
    strcat(modem_buffer, "\",\"");
    strcat(modem_buffer, port);
    strcat(modem_buffer, "\"");

    modem_execute(modem_buffer);
    if ((pmcu_error = modem_read_and_expect("OK")) != PMCU_OK) {
        return pmcu_error;
    }
    if ((pmcu_error = modem_read_and_expect("CONNECT OK")) != PMCU_OK) {
        return pmcu_error;
    }

    return PMCU_OK;
}

PMCU_Error modem_tcp_send(const uint8_t *buffer, size_t buffer_length) {
    modem_execute("AT+CIPSEND");
    if ((pmcu_error = uart_read_until_string(UART_A0, "> ", NULL, NULL, MODEM_COMMAND_TIMEOUT)) != PMCU_OK) {
        return pmcu_error;
    }

    uart_write_buffer(UART_A0, buffer, buffer_length);
    uart_write(UART_A0, 0x1a); // ctrl+z

    if ((pmcu_error = modem_read(modem_buffer)) != PMCU_OK) {
        return pmcu_error;
    }
    if (strcmp(modem_buffer, "SEND OK") != 0 && strcmp(modem_buffer, "OK") != 0) { // for some strange reason could return OK
        return SIM800L_UNEXPECTED_RESPONSE_ERROR;
    }

    return PMCU_OK;
}

inline PMCU_Error modem_tcp_recv(uint8_t *buffer, size_t buffer_length) {
    uart_read_buffer(UART_A0, buffer, buffer_length, 0);
    return PMCU_OK;
}

PMCU_Error modem_tcp_disconnect() {
    modem_execute("AT+CIPCLOSE");
    pmcu_error = modem_read(modem_buffer);
    return pmcu_error;
}
