#ifndef MODEM_H_
#define MODEM_H_

#define MODEM_COMMAND_TIMEOUT 10

#include <stdlib.h>
#include <stdint.h>

#include "error.h"

PMCU_Error modem_sync();

PMCU_Error modem_reset();

PMCU_Error modem_get_imei(char *imei);

PMCU_Error modem_get_location(char *location);

/**
 * Attaches to the GPRS network and creates a bearer profile
 * and a PDP context. Those will make the modem able to communicate
 * over bare TCP/UDP, or application protocols such as HTTP/FTP... (see docs).
 */
PMCU_Error modem_gprs_attach();

int sim800l_tcp_is_connected();

PMCU_Error modem_tcp_connect(const char *host, const char *port);

PMCU_Error modem_tcp_send(const uint8_t *buffer, size_t buffer_length);

PMCU_Error modem_tcp_recv(uint8_t *buffer, size_t buffer_length);

PMCU_Error modem_tcp_disconnect();

/**
 * Disables bearer profile, PDP context and deattaches fromthea GPRS network.
 */
int modem_gprs_deconfig();

#endif
