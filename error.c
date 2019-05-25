#include "error.h"

#include "timer.h"
#include "uart.h"

#include <string.h>

const char *pmcu_errors_str[] = { PMCU_ALL_ERRORS(GENERATE_STRING) };

inline const char *PMCU_error_str(PMCU_Error error) {
    return pmcu_errors_str[error];
}

void PMCU_log(const char *message) {
    char tmp[8];
    ltoa(timer_timestamp(), tmp);

    uart_write_string(UART_A1, "[");
    uart_write_string(UART_A1, tmp);
    uart_write_string(UART_A1, "] ");
    uart_write_string(UART_A1, message);
    uart_write_string(UART_A1, "\r\n");
}

void PMCU_error_print(const char *source, PMCU_Error error, const char *message) {
    timer_Task task;

    do {
        repeat:

        uart_write_string(UART_A1, "\r\n");
        uart_write_string(UART_A1, source);
        uart_write_string(UART_A1, ": ");
        uart_write_string(UART_A1, PMCU_error_str(error));
        if (message) {
            uart_write_string(UART_A1, " > ");
            uart_write_string(UART_A1, message);
        }
        uart_write_string(UART_A1, "\r\n");
        uart_write_string(UART_A1, "\r\n");

        timer_task_start(&task, 3);

        while (1) {
            if (task.satisfied) {
                goto repeat; // sorry mom
            }
        }
    } while (1); // runs forever
}
