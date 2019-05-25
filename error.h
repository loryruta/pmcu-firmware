#ifndef ERROR_H_
#define ERROR_H_

#include <msp430.h>

/**
 * Checks whether the given function returns PMCU_OK, if not halts the firmware and prints the error.
 */
#define __pmcu_assert(source, function) if ((pmcu_error = function) != PMCU_OK) PMCU_error_print(source, pmcu_error, NULL);

/**
 * Checks whether the given function returns PMCU_OK, if not returns the error to an higher function.
 */
#define __pmcu_handle(function) if ((pmcu_error = function) != PMCU_OK) return pmcu_error;

/**
 * Retries x times until the function returns PMCU_OK.
 */
#define __pmcu_retry(function, times) \
        uint8_t i; \
        for (i = 0; i < times; i++) { \
            if ((pmcu_error = function) != PMCU_OK) { \
                continue; \
            } else { \
                break; \
            } \
        }

#define PMCU_ALL_ERRORS(ACTION) \
    ACTION(PMCU_OK) \
    ACTION(PMCU_GENERIC_ERROR) \
    \
    ACTION(UART_TIMEOUT_ERROR) \
    ACTION(UART_UNEXPECTED_BYTE_ERROR) \
    ACTION(UART_BUFFER_TOO_SMALL_ERROR) \
    \
    ACTION(DHT22_OPERATION_NOT_ALLOWED) \
    ACTION(DHT22_TIMEOUT) \
    ACTION(DHT22_WRONG_CHECKSUM) \
    \
    ACTION(SIM800L_TIMEOUT_ERROR) \
    ACTION(SIM800L_UNEXPECTED_RESPONSE_ERROR) \
    ACTION(SIM800L_MAX_RETRIALS_REACHED_ERROR) \
    \
    ACTION(SPS30_START_BYTE_EXPECTED) \
    ACTION(SPS30_BUFFER_TOO_SMALL) \
    ACTION(SPS30_WRONG_CHECKSUM) \
    ACTION(SPS30_WRONG_DATA_LENGTH) \
    ACTION(SPS30_UNKNOWN_COMMAND) \
    ACTION(SPS30_NO_ACCESS_RIGHT_FOR_COMMAND) \
    ACTION(SPS30_ILLEGAL_COMMAND_PARAMETER) \
    ACTION(SPS30_INTERNAL_FUNCTION_ARGUMENT_OUT_OF_RANGE) \
    ACTION(SPS30_COMMAND_NOT_ALLOWED_IN_CURRENT_STATE) \
    ACTION(SPS30_UNKNOWN_STATE) \
    ACTION(SPS30_STOP_BYTE_EXPECTED) \
    ACTION(SPS30_INVALID_STUFFED_BYTE) \
    \
    ACTION(MQTT_UNEXPECTED_RESPONSE_ERROR)

#define GENERATE_STRING(STRING) #STRING,
#define GENERATE_ENUM(ENUM) ENUM,

typedef enum {
    PMCU_ALL_ERRORS(GENERATE_ENUM)
} PMCU_Error;

PMCU_Error pmcu_error;

const char *PMCU_error_str(PMCU_Error error);

void PMCU_log(const char *message);

void PMCU_error_print(const char *source, PMCU_Error error, const char *message);

#endif
