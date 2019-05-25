#ifndef MQTT_H_
#define MQTT_H_

#include <stdlib.h>
#include <stdint.h>

#include "error.h"

size_t mqtt_pack_string(uint8_t *buffer, const char *string);

size_t mqtt_pack_fixed_header(uint8_t *buffer, uint8_t control_type, size_t remaining_length);

size_t mqtt_create_connect_packet(uint8_t *buffer, const char *client_id, const char *username, const char *password);

size_t mqtt_create_disconnect_packet(uint8_t *buffer);

PMCU_Error mqtt_connect(const uint8_t *connect_packet, size_t packet_size);

PMCU_Error mqtt_disconnect(uint8_t *disconnect_packet, size_t packet_size);

#endif
