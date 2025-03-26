#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "logger.h"

static bool lfe;
static bool debug_all;

void init_logger(bool log_file_enabled)
{
    lfe = log_file_enabled;
    debug_all = false;
}

bool toggle_master()
{
    debug_all = !debug_all;
    return debug_all;
}

void log_message
(
    LoggingLevel level,
    const char *file, 
    const char *func, 
    const char *format, 
    ...
) 
{
    // Log level string
    const char *level_str;
    switch (level) {
        case INFO:    level_str = "--INFO----"; break;
        case WARNING: level_str = "--WARNING-"; break;
        case ERROR:   level_str = "--ERROR---"; break;
        case DEBUG:   level_str = "--DEBUG---"; break;
        case TEST:    level_str = "--TEST----"; break;
        default:      level_str = "--LOG-----"; break;
    }
    if (level == TEST || level == INFO || debug_all)
    {
        int buffer_size = snprintf(NULL, 0, "\n[%-8s] | %-18s | %-15s |", level_str, file, func) + 1;
        char formatted_string[buffer_size];
        snprintf(formatted_string, buffer_size, "\n[%-8s] | %-18s | %-15s |", level_str, file, func);
        printf("%s ", formatted_string);
        // Print formatted message
        va_list args, args_copy;
        va_start(args, format);
        if (lfe)
        { 
            va_copy(args_copy, args);
            FILE *log_file = fopen("../logs/log.txt", "a");
            if (log_file == NULL)
            {
                perror("Error opening log file.");
                return;
            }
            fprintf(log_file, "%s " , formatted_string);
            vfprintf(log_file, format, args_copy);

            fflush(log_file);
            fclose(log_file);
            va_end(args_copy);
        }
        vprintf(format, args);
        va_end(args);
    }
}