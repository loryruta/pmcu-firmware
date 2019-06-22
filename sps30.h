#ifndef SPS30_H_
#define SPS30_H_

#include "uart.h"
#include "error.h"

#define SPS30_TIMEOUT 5

/**
 * This must be used as the very first command.
 * It switches the state of SPS30 from IDLE-MODE to MEASURING-MODE.
 */
PMCU_Error sps30_start_measurement();

/*
 This should be used as the second command.
 Receive the confirm of switching the state.
 It is a debug version of the command because stores the response in an array (buff).
 Recommended length: 8
 */
int sps30_read_start_ack(unsigned char* buff);



/*
 This command turns the state of SPS30 to IDLE-MODE, which uses less power
 */
int sps30_stop_measurement();

/*
 This should be used to receive confirm of switching the state to IDLE-MODE, after stopping the measurement.
 It is a debug version, indeed involves an array (buff) as a parameter, in it will be stored the response.
 Recommended length: 8
 */
int sps30_read_stop_ack(unsigned char* buff);

/*
 This should be used to ask the SPS30 to send the measures. NOTE: do not ask more than once per second.
 */
int sps30_ask_measured_values();

PMCU_Error sps30_read_measured_values(uint8_t* buffer, size_t buffer_length, size_t *payload_length);


/*
 Execute manually the fan cleaning (to be done periodically in order to guarantee the quality of the measures).
 Default fan-cleaning interval: 160 hours (too much!)
 */
int sps30_start_fan_cleaning();

/*
 Receive the confirm of the start of fan-cleaning.
 */
int sps30_read_fan_ack(unsigned char* buff);

#endif

