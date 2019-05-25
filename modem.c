#include "modem.h"

#include "uart.h"
#include "circular_buffer.h"

#include <string.h>

#define MODEM_LOG_LENGTH 96

char modem_res_cmp[MODEM_LOG_LENGTH];
char modem_log[MODEM_LOG_LENGTH];
size_t modem_ret;

circular_buffer at_cmd_buf;


#define __modem_try(times, code) \
    for (modem_ret = 0; modem_ret < times; modem_ret++) { \
        code \
    } \
    if (modem_ret == times) { \
        return SIM800L_MAX_RETRIALS_REACHED_ERROR; \
    }

#define __modem_try_once(code) \
        __modem_try(1, code);

#define __modem_expect_or_repeat(expected_response) \
    if (modem_read(modem_res_cmp) != PMCU_OK) { \
        continue; \
    } \
    if (strcmp(modem_res_cmp, expected_response) != 0) { \
        PMCU_log(expected_response);\
        PMCU_log(modem_res_cmp);\
        continue; \
    }

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

void modem_execute(const char *cmd) {
    size_t i;
    i = 0;
    while (cmd[i] != '\0') {
        circular_buffer_write(&at_cmd_buf, cmd[i]);
        uart_write(UART_A0, cmd[i]);
        i++;
    }

    strcpy(modem_log, "MODEM AT> ");
    strcat(modem_log, cmd);
    PMCU_log(modem_log);

    uart_write(UART_A0, '\r');
}

PMCU_Error modem_sync() {
    circular_buffer_init(&at_cmd_buf);

    uart_write(UART_A0, 27); // ESC if was connected
    modem_read(modem_res_cmp);
    __modem_try(10,
        modem_execute("AT");
        if (uart_read_until_string(UART_A0, "OK\r\n", NULL, NULL, 3) != PMCU_OK) {
            continue;
        }
        break;
    );
    return PMCU_OK;
}

PMCU_Error modem_reset() {
    // resets the modem config
    modem_execute("ATZ");
    uart_read_until_string(UART_A0, "OK\r\n", NULL, NULL, MODEM_COMMAND_TIMEOUT);

    // disables echoes
    modem_execute("ATE0");
    uart_read_until_string(UART_A0, "\r", NULL, NULL, MODEM_COMMAND_TIMEOUT);
    if (uart_match_string(UART_A0, "\r\nOK\r\n", MODEM_COMMAND_TIMEOUT)) {
        return SIM800L_UNEXPECTED_RESPONSE_ERROR;
    }

    __modem_try(10,
        modem_execute("AT+CMEE=2");
        __modem_expect_or_repeat("OK");
        break;
    );

    __modem_try(10,
        modem_execute("AT+CIPSPRT=1");
        __modem_expect_or_repeat("OK");
        break;
    );

    return PMCU_OK;
}

PMCU_Error modem_get_imei(char *imei) {
    __modem_try(10,
        modem_execute("AT+CGSN");
        modem_read(imei);
        __modem_expect_or_repeat("OK");
        break;
    );
    return PMCU_OK;
}

PMCU_Error modem_get_location(char *location) {
    __modem_try(10,
        modem_execute("AT+CIPGSMLOC=1,1");
        modem_read(location);
        __modem_expect_or_repeat("OK");
        break;
    );
    return PMCU_OK;
}

PMCU_Error modem_gprs_attach() {
    // attaches to gprs network
    __modem_try(10,
         modem_execute("AT+CGATT=1");
         __modem_expect_or_repeat("OK");
         break;
    );

    // ************************************************** bearer profile creation for at+cipgsmloc

    // closes previous opened bearer profile (if any)
    __modem_try(10,
         modem_execute("AT+SAPBR=0,1");
         modem_read(modem_res_cmp);
         break;
    );

    // sets the connection type to gprs
    __modem_try(10,
         modem_execute("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
         __modem_expect_or_repeat("OK");
         break;
    );

    // sets apn to "TM"
    __modem_try(10,
        modem_execute("AT+SAPBR=3,1,\"APN\",\"TM\"");
        __modem_expect_or_repeat("OK");
        break;
    );

    // opens the created bearer profile
    __modem_try(10,
        modem_execute("AT+SAPBR=1,1");
        __modem_expect_or_repeat("OK");
        break;
    );

    // queries the created bearer
    __modem_try(10,
         modem_execute("AT+SAPBR=2,1");
         modem_read(modem_res_cmp); // ip address string
         __modem_expect_or_repeat("OK");
         break;
     );

    // ************************************************** pdp context creation for tcp/ip network

    // deactivates current pdp context
    __modem_try(10,
         modem_execute("AT+CIPSHUT");
         __modem_expect_or_repeat("SHUT OK");
         break;
     );

    // sets single ip connection
    __modem_try(10,
         modem_execute("AT+CIPMUX=0");
         __modem_expect_or_repeat("OK");
         break;
     );

    // starts task, sets apn, empty username and password
    __modem_try(10,
         modem_execute("AT+CSTT=\"TM\",\"\",\"\"");
         __modem_expect_or_repeat("OK");
         break;
     );

    // brings up wireless connection
    __modem_try(10,
         modem_execute("AT+CIICR");
         __modem_expect_or_repeat("OK");
         break;
     );

    // gets pdp ip address (should be equal to bearer profile's one)
    modem_execute("AT+CIFSR");
    modem_read(modem_res_cmp);

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
    modem_read(modem_res_cmp);

    // connects to the tcp server
    strcpy(modem_res_cmp, "AT+CIPSTART=\"TCP\",\"");
    strcat(modem_res_cmp, host);
    strcat(modem_res_cmp, "\",\"");
    strcat(modem_res_cmp, port);
    strcat(modem_res_cmp, "\"");

    __modem_try(3,
        modem_execute(modem_res_cmp);
        __modem_expect_or_repeat("OK");

        modem_read(modem_res_cmp);
        if (strcmp(modem_res_cmp, "CONNECT OK") != 0 && strcmp(modem_res_cmp, "ALREADY CONNECT") != 0) {
            continue;
        }
        break;
    );

    return PMCU_OK;
}

PMCU_Error modem_tcp_send(const uint8_t *buffer, size_t buffer_length) {
    __modem_try(10,
        modem_execute("AT+CIPSEND");
        if (uart_read_until_string(UART_A0, "> ", NULL, NULL, MODEM_COMMAND_TIMEOUT) != PMCU_OK) {
            continue;
        }
        uart_write_buffer(UART_A0, buffer, buffer_length);
        uart_write(UART_A0, 0x1a); // ctrl z

        modem_read(modem_res_cmp);
        if (strcmp(modem_res_cmp, "SEND OK") != 0 && strcmp(modem_res_cmp, "OK") != 0) { // for some strange reason could return OK
            continue;
        }
        break;
    );
    return PMCU_OK;
}

inline PMCU_Error modem_tcp_recv(uint8_t *buffer, size_t buffer_length) {
    uart_read_buffer(UART_A0, buffer, buffer_length, 0);
    return PMCU_OK;
}

PMCU_Error modem_tcp_disconnect() {
    modem_execute("AT+CIPCLOSE");
    modem_read(modem_res_cmp);
    return PMCU_OK;
}
