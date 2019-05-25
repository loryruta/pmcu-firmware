#ifndef CONSOLE_H_
#define CONSOLE_H_

#define CONSOLE_LOG_ENABLED
#define CONSOLE_ERR_ENABLED
#define CONSOLE_ERR_INTERVAL 5

void console_log(const char *prompt, const char *log);

/**
 * Halts the code and writes continously the error
 * along with the given prompt.
 */
void console_err(const char *prompt, const char *err);

#endif
