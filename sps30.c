#include "sps30.h"

#include <msp430.h>

//MSP to SPS30 packet structure
//START + ADDRESS + CMD + LENGTH + ...bytes... + CHECKSUM + STOP

//SPS30 to MSP packet structure
//START + ADDRESS + CMD + STATE + LENGTH + ...bytes... + CHECKSUM + STOP

//NOTE: for sure the second has got at least 1 more byte than the first

//ARRAY LENGTHS
#define MEASURED_DATA_LENGTH 47
#define START_ACK_LENGTH 7
#define STOP_ACK_LENGTH 7

//COMMAND CODES
#define START 0x00
#define STOP 0x01
#define READ 0x03
#define INFO_CLEAN 0x80
#define CLEAN 0x56
#define INFO 0xD0
#define RESET 0xD3

//STATE CODES
#define NO_ERROR 0x00
#define WRONG_DATA_LENGTH 0x01
#define UNKNOWN_CMD 0x02
#define FORBIDDEN_CMD 0x03
#define ILLEGAL_PARAMETER 0x04
#define ARG_OUT_OF_RANGE 0x28
#define CMD_NOT_IN_STATE 0x43

//UART SPS30 ADDRESS
#define ADDRESS 0x00

//START AND STOP BYTES
#define START_BYTE 0x7E
#define STOP_BYTE 0x7E

//COMMANDS

void write_byte(uint8_t byte) {
    uint8_t tmp = byte;
    switch (byte) {
    case 0x7E:
        byte = 0x5E;
        break;
    case 0x7D:
        byte = 0x5D;
        break;
    case 0x11:
        byte = 0x31;
        break;
    case 0x13:
        byte = 0x33;
        break;
    }
    if (tmp != byte) {
        uart_write(UART_A0, 0x7D);
    }
    uart_write(UART_A0, byte);
}

void write_buffer(uint8_t *command, size_t length) {
    size_t i;
    for (i = 0; i < length; i++) {
        write_byte(command[i]);
    }
}

PMCU_Error read_byte(uint8_t *byte) {
    uart_read(UART_A0, byte, NULL);
    if (*byte == 0x7D) {
        uart_read(UART_A0, byte, NULL);
        switch (*byte) {
        case 0x5E:
            *byte = 0x7E;
            break;
        case 0x5D:
            *byte = 0x7D;
            break;
        case 0x31:
            *byte = 0x11;
            break;
        case 0x33:
            *byte = 0x13;
            break;
        default:
            return SPS30_INVALID_STUFFED_BYTE;
        }
    }
    return PMCU_OK;
}

PMCU_Error send_shdlc_frame(uint8_t command, const uint8_t *payload, size_t payload_length) {
    size_t i;
    uint8_t checksum;

    // start
    uart_write(UART_A0, 0x7E);

    // address
    uart_write(UART_A0, 0x00);
    checksum = 0x00;

    // command
    uart_write(UART_A0, command);
    checksum += command;

    // length
    payload_length = payload_length & 0xff;
    uart_write(UART_A0, payload_length);
    checksum += payload_length;

    // payload
    for (i = 0; i < payload_length; i++) {
        write_byte(payload[i]);
        checksum += payload[i];
    }

    // checksum
    checksum = ~checksum;
    uart_write(UART_A0, checksum);

    // stop
    uart_write(UART_A0, 0x7E);

    return PMCU_OK;
}

/**
 * Receives an SHDLC frame and saves the payload on a given buffer.
 * - buffer: the array where to store results
 * - buffer_length: the length of the given buffer (just for test purposes)
 * - payload_length: a pointer to a variable where the real payload length will be saved
 */
PMCU_Error recv_shdlc_frame(uint8_t *buffer, size_t buffer_length, size_t *payload_length) {
    uint8_t byte, state, length, i, checksum;
    PMCU_Error error;

    // start
    uart_read(UART_A0, &byte, NULL);
    if (byte != 0x7E) {
        return SPS30_START_BYTE_EXPECTED;
    }

    // address
    uart_read(UART_A0, &byte, NULL);
    checksum = byte;

    // command
    uart_read(UART_A0, &byte, NULL);
    checksum += byte;

    // state
    uart_read(UART_A0, &state, NULL);
    checksum += state;

    // length
    uart_read(UART_A0, &length, NULL);
    checksum += length;
    *payload_length = length;

    if (buffer && length > buffer_length) {
        return SPS30_BUFFER_TOO_SMALL;
    }

    // payload
    for (i = 0; i < length; i++) {
        if ((error = read_byte(&byte)) != PMCU_OK) {
            return error;
        }
        checksum += byte;
        if (buffer) {
            buffer[i] = byte;
        }
    }

    // checksum
    uart_read(UART_A0, &byte, NULL);
    checksum = ~checksum;
    if (checksum != byte) {
        return SPS30_WRONG_CHECKSUM;
    }

    // stop
    uart_read(UART_A0, &byte, NULL);
    if (byte != 0x7E) {
        return SPS30_STOP_BYTE_EXPECTED;
    }

    if (state != 0x00) {
        switch (state) {
        case 0x01: return SPS30_WRONG_DATA_LENGTH;
        case 0x02: return SPS30_UNKNOWN_COMMAND;
        case 0x03: return SPS30_NO_ACCESS_RIGHT_FOR_COMMAND;
        case 0x04: return SPS30_ILLEGAL_COMMAND_PARAMETER;
        case 0x28: return SPS30_INTERNAL_FUNCTION_ARGUMENT_OUT_OF_RANGE;
        case 0x43: return SPS30_COMMAND_NOT_ALLOWED_IN_CURRENT_STATE;
        }
    }

    return PMCU_OK;
}

inline PMCU_Error skip_shdlc_frame() {
    return recv_shdlc_frame(NULL, NULL, NULL);
}

PMCU_Error sps30_start_measurement() {
    const uint8_t command[] = {0x01, 0x03};
    send_shdlc_frame(0x00, command, 2);
    return skip_shdlc_frame();
}

PMCU_Error sps30_read_measured_values(uint8_t* buffer, size_t buffer_length, size_t *payload_length) {
    send_shdlc_frame(0x03, NULL, NULL);
    return recv_shdlc_frame(buffer, buffer_length, payload_length);
}

int sps30_stop_measurement() {
    uint8_t command[] = {0x7E, 0x00, 0x01, 0x00, 0xFE, 0x7E};
    uart_write_buffer(UART_A0, command, 6);

    uart_read_buffer(UART_A0, command, 7, 0); // ACK
    return 0;
}

int sps30_ask_measured_values() {
    return 0;
}

//TODO: check if length is right
int sps30_ask_cleaning_interval() {
    unsigned char array[] = {0x7E, 0x00, 0x80, 0x01, 0x00, 0x7D, 0x5E, 0x7E};
    uart_write_buffer(UART_A0, array, 8);
    return 0;
}

int sps30_read_cleaning_interval() {
    unsigned char array[11];
    uart_read_buffer(UART_A0, array, 11, 0);
    return 0;
}

int sps30_start_fan_cleaning() {
    unsigned char array[] = {0x7E, 0x00, 0x56, 0x00, 0xA9, 0x7E};
    uart_write_buffer(UART_A0, array, 6);
    return 0;
}

int sps30_read_fan_ack(unsigned char* buff) {
    uart_read_buffer(UART_A0, buff, 7, 0);
    return 0;
}
