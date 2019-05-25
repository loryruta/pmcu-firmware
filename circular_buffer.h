#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#define CIRCULAR_BUFFER_MAX_SIZE 256

typedef struct {
    unsigned int count;
    unsigned int write_position;
    unsigned int read_position;
    unsigned int size;
    unsigned char data[CIRCULAR_BUFFER_MAX_SIZE];
} circular_buffer;

void circular_buffer_init(circular_buffer *buffer);

int circular_buffer_is_empty(circular_buffer *buffer);

int circular_buffer_is_full(circular_buffer *buffer);

int circular_buffer_write(circular_buffer *buffer, unsigned char element);

int circular_buffer_read(circular_buffer *buffer, unsigned char *element);

void circular_buffer_clear(circular_buffer *buffer);

#endif
