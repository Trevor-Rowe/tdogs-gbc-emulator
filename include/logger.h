#ifndef GBC_LOGGER_H
#define GBC_LOGGER_H

typedef enum
{
    INFO,
    TEST,
    DEBUG,
    WARNING,
    ERROR

} LoggingLevel;

void init_logger(bool log_file_enabled);

bool toggle_master();

void log_message
(
    LoggingLevel level, 
    const char *file,
    const char *func,
    const char *format,
    ...
);

#endif