#ifndef DHT22_H_
#define DHT22_H_

#include <msp430.h>
#include <stdint.h>

#include "error.h"

PMCU_Error dht22_read(uint8_t *buffer);

#endif
