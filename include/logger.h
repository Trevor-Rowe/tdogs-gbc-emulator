#ifndef LOGGER_H
#define LOGGER_H

typedef enum
{
    INFO,
    TEST,
    DEBUG,
    WARNING,
    ERROR

} LoggingLevel;

void log_message
(
    LoggingLevel level, 
    const char *file,
    const char *func,
    const char *format,
    ...
);

void cpu_log(LoggingLevel level, const char *message, ...);

void joypad_log(LoggingLevel level, const char *message, ...);

#endif