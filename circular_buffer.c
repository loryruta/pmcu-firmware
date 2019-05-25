#include "circular_buffer.h"

#include <stdlib.h>
#include <stdint.h>

void circular_buffer_init(circular_buffer *buffer) {
    buffer->count = 0;
    buffer->write_position = 0;
    buffer->read_position = 0;
    buffer->size = CIRCULAR_BUFFER_MAX_SIZE;
}

int circular_buffer_is_empty(circular_buffer *buffer) {
    return buffer->count == 0;
}

int circular_buffer_is_full(circular_buffer *buffer) {
    return buffer->count == buffer->size;
}

int circular_buffer_write(circular_buffer *buffer, unsigned char element) {
    if (buffer->count < buffer->size) {
        buffer->count++;
        buffer->data[buffer->write_position] = element;
        buffer->write_position = ++(buffer->write_position) < buffer->size ? buffer->write_position : 0;
        return 1;
    }
    return 0;
}

int circular_buffer_read(circular_buffer *buffer, uint8_t *byte) {
    if (buffer->count > 0) {
        buffer->count--;
        if (byte != NULL) {
            *byte = buffer->data[buffer->read_position];
        }
        buffer->read_position = ++(buffer->read_position) < buffer->size ? buffer->read_position : 0;
        return 1;
    }
    return 0;
}

void circular_buffer_clear(circular_buffer *buffer) {
    buffer->count = 0;
    buffer->write_position = 0;
    buffer->read_position = 0;
}
