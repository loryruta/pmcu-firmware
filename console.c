#include "console.h"
#include "uart.h"

const char *lst_prompt;
const char *lst_error;

void console_log(const char *prompt, const char *log) {
    uart_write_string(UART_A1, prompt);
    uart_write_string(UART_A1, "> ");
    uart_write_string(UART_A1, log);
    uart_write_string(UART_A1, "\r\n");
}

void console_raw_err(const char *prompt, const char *error) {
    uart_write_string(UART_A1, "---------------------\r\n");
    uart_write_string(UART_A1, prompt);
    uart_write_string(UART_A1, ">\r\n");
    uart_write_string(UART_A1, error);
    uart_write_string(UART_A1, "\r\n");
    uart_write_string(UART_A1, "---------------------\r\n");
}

void console_err(const char *prompt, const char *error) {
    lst_prompt = prompt;
    lst_error = error;

    console_raw_err(lst_prompt, lst_error);

    // every second timer
    TB0CCR0 = 4098 * CONSOLE_ERR_INTERVAL;
    TB0CTL = TASSEL__ACLK | MC__UP | ID__8 | TBIE;

    __bis_SR_register(LPM0_bits);
    __no_operation();
}

#pragma vector=TIMER0_B1_VECTOR
__interrupt void timer0_b1 (void) {
    console_raw_err(lst_prompt, lst_error);
    TB0CTL &= ~TBIFG;
}
