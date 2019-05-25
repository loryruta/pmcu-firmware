#include "mqtt.h"

#include "modem.h"
#include <string.h>

size_t mqtt_pack_string(uint8_t *buffer, const char *string) {
    size_t i;
    for (i = 0; string[i] != '\0'; i++) {
        buffer[2 + i] = string[i];
    }
    buffer[0] = (i & 0xff00) >> 8;
    buffer[1] = i & 0xff;
    return i + 2;
}

/* Our fixed header always have a remaining_length of 3 bytes */
size_t mqtt_pack_fixed_header(uint8_t *buffer, uint8_t control_type, size_t remaining_length) {
    size_t i;
    buffer[0] = control_type;
    for (i = 1; i < 4; i++) {
        buffer[i] = remaining_length & 0x7f;
        remaining_length >>= 7;
        if (i < 3) {
            buffer[i] |= 128;
        }
    }
    return i;
}

size_t mqtt_create_connect_packet(uint8_t *buffer, const char *client_id, const char *username, const char *password) {
    size_t position;
    position = 4;

    // **************** Variable header
    position += mqtt_pack_string(&buffer[position], "MQTT");
    buffer[position++] = 4;

    buffer[position] = 0;
    if (username != NULL) {
        buffer[position] |= 0b10000000;
    }
    if (password != NULL) {
        buffer[position] |= 0b01000000;
    }
    buffer[position] |= 0b10;
    position++;

    buffer[position++] = 0;
    buffer[position++] = 0;

    // **************** Payload
    position += mqtt_pack_string(&buffer[position], client_id);
    if (username != NULL) {
        position += mqtt_pack_string(&buffer[position], username);
    }
    if (password != NULL) {
        position += mqtt_pack_string(&buffer[position], password);
    }

    // **************** Fixed header
    mqtt_pack_fixed_header(buffer, 0b00010000, position - 4);

    return position;
}

size_t mqtt_create_disconnect_packet(uint8_t *buffer) {
    buffer[0] = 0b11100000;
    buffer[1] = 0;
    return 2;
}

PMCU_Error mqtt_connect(const uint8_t *connect_packet, size_t packet_size) {
    PMCU_Error err;
    uint8_t res[4];

    if ((err = modem_tcp_send(connect_packet, packet_size)) != PMCU_OK) {
        return err;
    }
    __pmcu_handle(modem_tcp_recv(res, 4));
    if (res[0] != 32 || res[3] != 0) {
        return MQTT_UNEXPECTED_RESPONSE_ERROR;
    }
    return PMCU_OK;
}

PMCU_Error mqtt_disconnect(uint8_t *disconnect_packet, size_t packet_size) {
    __pmcu_handle(modem_tcp_send(disconnect_packet, packet_size));
    return PMCU_OK;
}

